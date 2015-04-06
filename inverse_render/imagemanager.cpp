#include "imagemanager.h"
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/sharable_lock.hpp>
#include <boost/interprocess/sync/upgradable_lock.hpp>

#include <functional>
#include <iostream>
#include <iomanip>
#include <fstream>
using namespace std;

#define SHM_IMAGEDATA_ID "SHM_IMAGEDATA_ID"
// TODO: Read imagetypes from config
// TODO: Proper locking mechanism

/*
 * Memory layout:
 * All mutexes
 * All initialization flags
 * Depth2RGB, if any
 * All CameraParams
 * All images for imagetypes[0]
 * All images for imagetypes[1]
 * All images for imagetypes[2]
 * ...
 */

// --------------------------------------------------------------
// Constructors and initialization
// --------------------------------------------------------------
ImageManager::ImageManager(int width, int height, int numImages)
    : w(width), h(height), sz(numImages)
{
    defaultinit("");
    initializeSharedMemory();
}
ImageManager::ImageManager(const string& camfile)
{
    ifstream in(camfile.c_str());
    in >> sz >> w >> h;

    defaultinit(camfile);
    initializeSharedMemory();
}

void ImageManager::defaultinit(const string& camfile) {
    initializeImageTypes();

    hash<string> str_hash;
    stringstream ss;
    ss << setbase(16) << SHM_IMAGEDATA_ID << str_hash(camfile);
    shmname = ss.str();
}

// Sets up what types of image
void ImageManager::initializeImageTypes() {
    // Raw input data
    imagetypes.push_back(ImageType("color",      "",     CV_32FC3));
    // Custom types begin here
    imagetypes.push_back(ImageType("confidence", "conf", CV_32FC1, ImageType::IT_SCALAR));
    imagetypes.push_back(ImageType("depth",      "pcd",  CV_32FC1, ImageType::IT_DEPTHMAP));
    // Processed results
    imagetypes.push_back(ImageType("edges",      "edge", CV_8UC3));
    imagetypes.push_back(ImageType("labels",     "lbl",  CV_8UC1));

    images.resize(imagetypes.size());
}

using namespace boost::interprocess;
bool ImageManager::initializeSharedMemory() {
    try {
        shared_memory_object shm(open_or_create, shmname.c_str(), read_write);
        shm.truncate(computeSize());
        mregion = mapped_region(shm, read_write);
    } catch(...) {
        return false;
    }

    char* s = static_cast<char*>(mregion.get_address());
    mutexes = (shmutex*)(s);
    s += imagetypes.size()*sizeof(shmutex);
    flags = (unsigned char*)(s);
    s += sz*imagetypes.size();
    depth2rgb = (R4Matrix*)(s);
    s += sizeof(R4Matrix);
    cameras = (CameraParams*)(s);
    s += sz*sizeof(CameraParams);
    for (int i = 0; i < imagetypes.size(); ++i) {
        images[i].resize(sz);
        for (int j = 0; j < sz; ++j) {
            images[i][j] = s;
            s += w*h*imagetypes[i].getSize();
        }
    }
    return true;
}


// --------------------------------------------------------------
// Utilities
// --------------------------------------------------------------
int ImageManager::computeSize() const {
    int sum = 0;
    for (int i = 0; i < imagetypes.size(); ++i) {
        sum += imagetypes[i].getSize();
    }
    return imagetypes.size()*sizeof(shmutex)      // Mutexes
         + sz*imagetypes.size()                   // Initialization flags
         + sizeof(R4Matrix)                       // Depth to RGB Transform
         + sz*sizeof(CameraParams)                // Camera parameters
         + sz*w*h*sum;                            // Image data
}

int ImageManager::nameToIndex(const string& type) const {
    for (int i = 0; i < imagetypes.size(); ++i) {
        if (imagetypes[i].getName() == type) return i;
    }
    return -1;
}

ImageManager::shmutex* ImageManager::getMutex(int t, int n) {
    return &mutexes[t];
}

// --------------------------------------------------------------
// Accessors
// --------------------------------------------------------------
const void* ImageManager::getImage(const string& type, int n) const {
    int i = nameToIndex(type);
    if (i < 0) return NULL;
    //boost::interprocess::shareable_lock<shmutex> lock(*getMutex(i,n));
    if (flags[i*sz+n] & DF_INITIALIZED) return images[i][n];
    else return NULL;
}
const void* ImageManager::getImage(int n) const {
    //boost::interprocess::shareable_lock<shmutex> lock(*getMutex(0,n));
    if (flags[n] & DF_INITIALIZED) return images[0][n];
    else return NULL;
}

void* ImageManager::getImageWriteable(const string& type, int n) {
    int i = nameToIndex(type);
    if (i < 0) return NULL;
    //boost::interprocess::scoped_lock<shmutex> lock(*getMutex(i,n));
    //memcpy(images[i][n], ptr, w*h*imagetypes[i].getSize());
    return NULL;
}

const CameraParams* ImageManager::getCamera(int n) const {
    if (n < 0 || n >= sz) return NULL;
    return &cameras[n];
}

unsigned char ImageManager::getFlags(const string& type, int n) const {
    int i = nameToIndex(type);
    if (i < 0) return DF_ERROR;
    if (n < 0 || n >= sz) return DF_ERROR;
    //boost::interprocess::shareable_lock<shmutex> lock(*getMutex(i,n));
    return flags[i*sz + n];
}
void ImageManager::setFlags(const string& type, int n, unsigned char value) {
    int i = nameToIndex(type);
    if (i < 0) return;
    if (n < 0 || n >= sz) return;
    //boost::interprocess::scoped_lock<shmutex> lock(*getMutex(i,n));
    flags[i*sz + n] = value;
}
const R4Matrix& ImageManager::getDepthToRgbTransform() const {
    return *depth2rgb;
}

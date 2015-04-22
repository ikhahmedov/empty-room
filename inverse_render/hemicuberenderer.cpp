#include "hemicuberenderer.h"
#include <iostream>

#define MAX_LIGHTS 127

using namespace std;

HemicubeRenderer::HemicubeRenderer(RenderManager* rm, int hemicubeResolution)
    : rendermanager(rm), res(hemicubeResolution)
{
    computeHemicubeFF();
}
void HemicubeRenderer::computeHemicubeFF() {
    // Some redundancy, but storage is cheap
    topHemicubeFF = new float*[res];
    sideHemicubeFF = new float*[res];
    for (int i = 0; i < res; ++i) {
        topHemicubeFF[i] = new float[res];
        sideHemicubeFF[i] = new float[res];
    }
    float x = 1./res;
    float d = 2./res;
    int o = res/2;
    float tot = 0;
    for (int i = 1; i <= res/2; ++i, x += d) {
        float y = 1./res;
        for (int j = 1; j <= i; ++j, y += d) {
            float denom = x*x + y*y + 1;
            topHemicubeFF[o+i-1][o+j-1] = d*d/(M_PI*denom*denom);
            topHemicubeFF[o+i-1][o-j]   = topHemicubeFF[o+i-1][o+j-1];
            topHemicubeFF[o-i][o-j]     = topHemicubeFF[o+i-1][o+j-1];
            topHemicubeFF[o-i][o+j-1]   = topHemicubeFF[o+i-1][o+j-1];
            topHemicubeFF[o+j-1][o+i-1] = topHemicubeFF[o+i-1][o+j-1];
            topHemicubeFF[o+j-1][o-i]   = topHemicubeFF[o+i-1][o+j-1];
            topHemicubeFF[o-j][o-i]     = topHemicubeFF[o+i-1][o+j-1];
            topHemicubeFF[o-j][o+i-1]   = topHemicubeFF[o+i-1][o+j-1];
        }
        float z = 1./res;
        for (int j = 1; j <= res/2; ++j, z += d) {
            float denom = z*z + x*x + 1;
            sideHemicubeFF[o+i-1][o+j-1] = d*d*z/(M_PI*denom*denom);
            sideHemicubeFF[o+i-1][o-j] = sideHemicubeFF[o+i-1][o+j-1];
            sideHemicubeFF[o-i][o-j]   = sideHemicubeFF[o+i-1][o+j-1];
            sideHemicubeFF[o-i][o+j-1] = sideHemicubeFF[o+i-1][o+j-1];
        }
    }
}

float HemicubeRenderer::renderHemicube(
        const R3Point& p,
        const R3Vector& n,
        Material& m,
        vector<float>& lightareas,
        float* image, float* light)
{
    double blank = 0;
    if (image == NULL) image = new float[3*res*res];
    if (light == NULL) light = new float[3*res*res];
    R3Point pp = p + 0.0001*n;
    R3Vector x = abs(n[0])>abs(n[1])?R3yaxis_vector:R3xaxis_vector;
    x.Cross(n);
    x.Normalize();
    R3Vector y = n%x;
    R3Vector orientations[] = {x,-x,y,-y};
    for (int o = 0; o < 4; ++o) {
        renderFace(pp, orientations[o], n, image, true);
        renderFace(pp, orientations[o], n, light, false);
        for (int i = 0; i < res/2; ++i) {
            for (int j = 0; j < res; ++j) {
                int visibility = *reinterpret_cast<int*>(&light[3*(i*res+j)]);
                int lightid = *reinterpret_cast<int*>(&light[3*(i*res+j)+1]);
                if (lightid >= 0 && visibility > 0) {
                    lightid--;
                    if (lightid >= lightareas.size()) lightareas.resize(lightid+1);
                    lightareas[lightid] += sideHemicubeFF[i][j];
                } else if (lightid < 0 && visibility == 0) {
                    blank += sideHemicubeFF[i][j];
                } else {
                    m.r += sideHemicubeFF[i][j]*image[3*(i*res+j)];
                    m.g += sideHemicubeFF[i][j]*image[3*(i*res+j)+1];
                    m.b += sideHemicubeFF[i][j]*image[3*(i*res+j)+2];
                }
            }
        }
    }
    renderFace(pp, n, y, image, true);
    renderFace(pp, n, y, light, false);
    for (int i = 0; i < res; ++i) {
        for (int j = 0; j < res; ++j) {
            int visibility = *reinterpret_cast<int*>(&light[3*(i*res+j)]);
            int lightid = *reinterpret_cast<int*>(&light[3*(i*res+j)+1]);
            if (lightid != 0 && visibility > 0) {
                lightid--;
                if (lightid >= lightareas.size()) lightareas.resize(lightid+1);
                lightareas[lightid] += topHemicubeFF[i][j];
            } else if (lightid == 0 && visibility == 0) {
                blank += topHemicubeFF[i][j];
            } else {
                m.r += topHemicubeFF[i][j]*image[3*(i*res+j)];
                m.g += topHemicubeFF[i][j]*image[3*(i*res+j)+1];
                m.b += topHemicubeFF[i][j]*image[3*(i*res+j)+2];
            }
        }
    }
    return blank;
}

void HemicubeRenderer::renderFace(const R3Point& p,
        const R3Vector& towards, const R3Vector& up,
        float* image, int mode)
{
    CameraParams cam;
    cam.pos = p;
    cam.towards = towards;
    cam.up = up;
    cam.fov = 90;
    cam.width = res;
    cam.height = res;
    render(&cam, image, mode);
}

void HemicubeRenderer::render(const CameraParams* cam, float* image, int mode)
{
    rendermanager->readFromRender(cam, image, mode, true);
}

void HemicubeRenderer::computeSamples(
        vector<SampleData>& data,
        vector<int> indices,
        int numsamples,
        double discardthreshold,
        float** images)
{
    float* lightimage;
    float* radimage;
    if (images) {
        for (int i = 0; i < numsamples; ++i) {
            images[2*i] = new float[3*res*res];
            images[2*i+1] = new float[3*res*res];
        }
    } else {
        lightimage = new float[3*res*res];
        radimage = new float[3*res*res];
    }

    default_random_engine generator;
    uniform_int_distribution<int> dist(0, indices.size());
    for (int i = 0; i < numsamples; ++i) {
        int n;
        do {
            n = dist(generator);
        } while (rendermanager->getMeshManager()->getLabel(indices[n]) > 0 || rendermanager->getMeshManager()->getVertexSampleCount(indices[n]) == 0);

        SampleData sd;
        sd.vertexid = indices[n];
        sd.radiosity = rendermanager->getMeshManager()->getVertexColor(indices[n]);
        if (images) {
            radimage = images[2*i];
            lightimage = images[2*i+1];
        }
        sd.fractionUnknown = renderHemicube(
                rendermanager->getMeshManager()->VertexPosition(indices[n]),
                rendermanager->getMeshManager()->VertexNormal(indices[n]),
                sd.netIncoming, sd.lightamount, radimage, lightimage
        );
        if (sd.fractionUnknown > discardthreshold) {
            --i;
            continue;
        }
        data.push_back(sd);
        if (i%10 == 9) cout << "Rendered " << i+1 << "/" << numsamples << endl;
    }
    cout << "Done sampling" << endl;
}

void HemicubeRenderer::createLabelImage(const CameraParams* cam, void* image) {
    int w = cam->width;
    int h = cam->height;
    char* ret = (char*) image;
    float* rendered = new float[w*h*3];
    render(cam, rendered, false);
    for (int i = 0; i < w*h; ++i) {
        ret[i] = (char) *reinterpret_cast<int*>(&rendered[3*i+2]);
    }
    delete [] rendered;
}

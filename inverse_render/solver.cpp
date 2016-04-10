#include "solver.h"
#include <set>
#include <random>
#include <fstream>

using namespace std;

void InverseRender::computeSamples(
        vector<SampleData>& data,
        vector<int> indices,
        int numsamples,
        double discardthreshold,
        bool saveImages,
        boost::function<void(int)> callback)
{
    hr->computeSamples(data, indices, numsamples, lights, discardthreshold, saveImages?&images:NULL, callback);
}

void InverseRender::reloadLights() {
    for (int i = 0; i < lightintensities.size(); i++) {
        if (lightintensities[i]->typeId() & LIGHTTYPE_RGB) {
            lights[i] = ((RGBLight*)lightintensities[i])->getLight(0);
        } else if (lightintensities[i]->typeId() & LIGHTTYPE_YIQ) {
            lights[i] = ((YIQLight*)lightintensities[i])->getLight();
        }
    }
}

void InverseRender::setupLightParameters(MeshManager* m) {
    lights.clear();
    int ret = 0;
    set<unsigned char> lightids;
    for (int i = 0; i < m->size(); i++) {
        unsigned char l = m->getLabel(i,MeshManager::LABEL_CHANNEL);
        if (l) lightids.insert(l);
    }
    for (auto lightinfo : lightids) {
        int t = LIGHTTYPE(lightinfo);
        Light* l = NewLightFromLightType(t);
        lights.push_back(l);
        if ((l->typeId() & LIGHTTYPE_LINE) || (l->typeId() & LIGHTTYPE_POINT)) {
            lightintensities.push_back(new YIQLight(l));
        } else {
            lightintensities.push_back(new RGBLight(l));
        }
    }
}

void InverseRender::writeVariablesMatlab(vector<SampleData>& data, string filename) {
    ofstream out(filename);
    out << "A = [" << endl;
    // FIXME
    for (int i = 0; i < data.size(); ++i) {
        for (int j = 0; j < data[i].lightamount.size(); ++j) {
            for (int k = 0; k < data[i].lightamount[j]->numParameters(); k++) {
                if (j+k) out << ",";
                out << data[i].lightamount[j]->coef(k);
            }
        }
        if (i != data.size()-1) out << ";" << endl;
    }
    out << "];" << endl;
    out << "weights = [" << endl;
    for (int i = 0; i < data.size(); ++i) {
        out << data[i].fractionUnknown;
        if (i != data.size()-1) out << ";";
    }
    out << "];" << endl;
    out << "direct = [" << endl;
    for (int i = 0; i < data.size(); ++i) {
        out << data[i].fractionDirect;
        if (i != data.size()-1) out << ";";
    }
    out << "];" << endl;
    for (int ch = 0; ch < 3; ++ch) {
        out << "% Channel " << ch << endl;
        out << "B" << ch << " = [" << endl;
        for (int i = 0; i < data.size(); ++i) {
            out << data[i].radiosity(ch);
            if (i != data.size()-1) out << ";";
        }
        out << "];" << endl;
        out << "C" << ch << " = [" << endl;
        for (int i = 0; i < data.size(); ++i) {
            out << data[i].netIncoming(ch);
            if (i != data.size()-1) out << ";";
        }
        out << "];" << endl;
    }
    for (int ch = 0; ch < 3; ch++) {
        out << "L" << ch << " = [" << endl;
        for (int i = 0; i < lights[ch].size(); i++) {
            if (lights[ch][i]) {
                for (int j = 0; j < lights[ch][i]->numParameters(); j++) {
                    if (i+j) out << ";";
                    out << lights[ch][i]->coef(j);
                }
            }
        }
        out << "];" << endl;
    }
}
void InverseRender::writeVariablesBinary(vector<SampleData>& data, string filename) {
    ofstream out(filename, ofstream::binary);
    uint32_t sz = data.size();
    out.write((char*) &sz, 4);
    sz = lightintensities.size();
    out.write((char*) &sz, 4);
    for (int i = 0; i < lightintensities.size(); i++) {
        int t = lightintensities[i]->typeId();
        out.write((char*) &t, sizeof(int));
        lightintensities[i]->writeToStream(out, true);
    }
    for (int i = 0; i < data.size(); ++i) {
        out.write((char*)&(data[i].fractionUnknown), sizeof(float));
        out.write((char*)&(data[i].vertexid), sizeof(int));
        out.write((char*)&(data[i].radiosity(0)), sizeof(float));
        out.write((char*)&(data[i].radiosity(1)), sizeof(float));
        out.write((char*)&(data[i].radiosity(2)), sizeof(float));
        out.write((char*)&(data[i].netIncoming(0)), sizeof(float));
        out.write((char*)&(data[i].netIncoming(1)), sizeof(float));
        out.write((char*)&(data[i].netIncoming(2)), sizeof(float));
        for (int j = 0; j < data[i].lightamount.size(); ++j) {
            data[i].lightamount[j]->writeToStream(out, true);
        }
    }
    for (int j = 0; j < 3; j++) {
        out.write((char*)&(wallMaterial(j)), sizeof(float));
    }
}

void InverseRender::loadVariablesBinary(vector<SampleData>& data, string filename) {
    ifstream in(filename, ifstream::binary);
    uint32_t sz;
    in.read((char*) &sz, 4);
    data.resize(sz);
    in.read((char*) &sz, 4);
    for (int i = 0; i < sz; i++) {
        int t;
        in.read((char*) &t, sizeof(int));
        Light* l;
        if (t & LIGHTTYPE_RGB) {
            t -= LIGHTTYPE_RGB;
            l = NewLightFromLightType(t);
            lights.push_back(l);
            lightintensities.push_back(new RGBLight(l));
            lightintensities[i]->readFromStream(in, true);
        } else if (t & LIGHTTYPE_YIQ) {
            t -= LIGHTTYPE_YIQ;
            l = NewLightFromLightType(t);
            lights.push_back(l);
            lightintensities.push_back(new YIQLight(l));
            lightintensities[i]->readFromStream(in, true);
        }
    }
    for (int i = 0; i < data.size(); ++i) {
        data[i].lightamount.resize(0);
        in.read((char*)&(data[i].fractionUnknown), sizeof(float));
        in.read((char*)&(data[i].vertexid), sizeof(int));
        in.read((char*)&(data[i].radiosity(0)), sizeof(float));
        in.read((char*)&(data[i].radiosity(1)), sizeof(float));
        in.read((char*)&(data[i].radiosity(2)), sizeof(float));
        in.read((char*)&(data[i].netIncoming(0)), sizeof(float));
        in.read((char*)&(data[i].netIncoming(1)), sizeof(float));
        in.read((char*)&(data[i].netIncoming(2)), sizeof(float));
        for (int j = 0; j < sz; j++) {
            int t = lights[j]->typeId();
            data[i].lightamount.push_back(NewLightFromLightType(t));
            data[i].lightamount[j]->readFromStream(in, true);
        }
    }
}

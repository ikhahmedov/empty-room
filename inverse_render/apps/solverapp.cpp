#include "rendering/opengl_compat.h"
#include "invrenderapp.h"
#include "wallfinder/wall_finder.h"
#include "solver.h"
#include <iostream>
#include <pcl/console/parse.h>

using namespace std;

class SolverApp : public InvrenderApp {
    public:
        SolverApp() : numlights(0), hemicuberesolution(150), numiters(1000), maxerr(0.1), discardthreshold(0.25), matlabfilename("") {}
        virtual int run() {
            mmgr->loadSamples();
            InverseRender ir(mmgr, numlights, hemicuberesolution, getProgressFunction(1,2));
            vector<SampleData> walldata;
            vector<int> wallindices;
            for (int i = 0; i < mmgr->size(); i++) {
                if (mmgr->getLabel(i, 1) == WallFinder::LABEL_WALL)
                    wallindices.push_back(i);
            }
            ir.computeSamples(walldata, wallindices, numsamples, discardthreshold, false, getProgressFunction(0,2));
            if (matlabfilename.length())
                ir.writeVariablesMatlab(walldata, matlabfilename);
            ir.setNumRansacIters(numiters);
            ir.setMaxPercentErr(maxerr);
            ir.solve(walldata);
            cout << "data:WallMaterial " << ir.wallMaterial.r << " " << ir.wallMaterial.g << " " << ir.wallMaterial.b << endl;
            for (int i = 0; i < numlights; i++) {
                cout << "data:Light" << i << " " << ir.lights[i].r << " " << ir.lights[i].g << " " << ir.lights[i].b << endl;
            }
        }
    protected:
        virtual void printargs() {

        }
        virtual int _parseargs(int argc, char** argv) {
            if (!mmgr) return 0;
            if (pcl::console::find_argument(argc, argv, "-hemicuberesolution") >= 0) {
                pcl::console::parse_argument(argc, argv, "-hemicuberesolution", hemicuberesolution);
            }
            if (pcl::console::find_argument(argc, argv, "-numlights") >= 0) {
                pcl::console::parse_argument(argc, argv, "-numlights", numlights);
            }
            if (pcl::console::find_argument(argc, argv, "-discardthreshold") >= 0) {
                pcl::console::parse_argument(argc, argv, "-discardthreshold", discardthreshold);
            }
            if (pcl::console::find_argument(argc, argv, "-numsamples") >= 0) {
                pcl::console::parse_argument(argc, argv, "-numsamples", numsamples);
            }
            if (pcl::console::find_argument(argc, argv, "-numiters") >= 0) {
                pcl::console::parse_argument(argc, argv, "-numiters", numiters);
            }
            if (pcl::console::find_argument(argc, argv, "-maxerr") >= 0) {
                pcl::console::parse_argument(argc, argv, "-maxerr", maxerr);
            }
            if (pcl::console::find_argument(argc, argv, "-outputsamplesfile") >= 0) {
                pcl::console::parse_argument(argc, argv, "-outputsamplesfile", matlabfilename);
            }
            return true;
        }
        int numiters;
        int numsamples;
        float discardthreshold;
        float maxerr;
        int hemicuberesolution;
        int numlights;
        string matlabfilename;
};

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1,1);
    glutCreateWindow("");
    glutHideWindow();
    SolverApp app;
    if (!app.parseargs(argc, argv)) return 1;
    return app.run();
}
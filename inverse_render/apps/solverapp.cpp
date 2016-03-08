#include "rendering/opengl_compat.h"
#include "invrenderapp.h"
#include "wallfinder/wall_finder.h"
#include "solver.h"
#include "rerender.h"
#include "datamanager/imageio.h"
#include <iostream>
#include <pcl/console/parse.h>

using namespace std;

class SolverApp : public InvrenderApp {
    public:
        SolverApp()
            : hemicuberesolution(150), discardthreshold(0.25),
              matlabfilename(""), pbrtfilename(""), rerenderedplyfilename(""), errorplyfilename(""),
              meshcolorscale(1), gamma(1),
              label(WallFinder::LABEL_WALL),
              cameranum(-1), solveTexture(false),
              scale(0), reglambda(0) {}
        virtual int run() {
            // Setup
            mmgr->loadSamples();
            InverseRender ir(mmgr, hemicuberesolution, getProgressFunction(1,2));

            // Populate wall data
            vector<SampleData> data;
            vector<int> indices;
            for (int i = 0; i < mmgr->size(); i++) {
                if (mmgr->getLabel(i, 1) == label)
                    indices.push_back(i);
            }

            if (inputmatlabfilename.length()) {
                ir.readVariablesMatlab(data, inputmatlabfilename);
            } else {
                ir.computeSamples(data, indices, numsamples, discardthreshold, false, getProgressFunction(0,2));
            }
            if (lightfilename.length()) {
                ir.readLightsFromTextFile(lightfilename);
            }

            // Optimization
            if (solveTexture) {
                if (matlabfilename.length()) {
                    ir.writeVariablesMatlab(data, matlabfilename);
                }
                Texture tex;
                R3Vector v(0,0,0);
                R3Point p(0,0,0);
                for (int i = 0; i < indices.size(); i++) {
                    v += mmgr->VertexNormal(indices[i]);
                    p += mmgr->VertexPosition(indices[i]);
                }
                v.Normalize();
                p /= indices.size();

                R3Plane plane(p, v);
                ir.solveTexture(data, imgr, plane, tex, label);
                if (tex.texture) ImageIO::writeExrImage(texturefilename, tex.texture, tex.size, tex.size);
            } else {
                if (scale > 0) {
                    ir.setLossFunction(LOSS_HUBER);
                    ir.setLossFunctionScale(scale);
                }
                ir.solve(data, reglambda);
            }

            // Output optimization data and results
            if (matlabfilename.length()) {
                ir.writeVariablesMatlab(data, matlabfilename);
            }
            if (solveTexture) return 0;
            if (lightfilename.length()) {
                ir.writeLightsToTextFile(lightfilename);
            }
            cout << "data:WallMaterial " << ir.wallMaterial.r << " " << ir.wallMaterial.g << " " << ir.wallMaterial.b << endl;
            for (int i = 0; i < ir.lights.size(); i++) {
                cout << "data:Light " << i << " " << ir.lights[i].r << " " << ir.lights[i].g << " " << ir.lights[i].b << endl;
            }

            // Output debugging results of optimization
            bool dorerender = rerenderedplyfilename.length()>0;
            bool doerror = errorplyfilename.length()>0;
            double meanerr = 0;
            double maxerr = 0;
            if (dorerender || doerror) {
                int wk = 0;
                vector<Material> colors;
                vector<Material> errors;
                for (int i = 0; i < mmgr->size(); i++) {
                    if (indices[wk] == i) {
                        Material res(0,0,0);
                        for (int j = 0; j < data[wk].lightamount.size(); j++) {
                            res += ir.lights[j]*data[wk].lightamount[j];
                        }
                        double frac = (1-data[wk].fractionDirect)/(1-data[wk].fractionDirect-data[wk].fractionUnknown);
                        res += data[wk].netIncoming*frac;
                        res = res*ir.wallMaterial;
                        colors.push_back(res);
                        Material errmat = res-data[wk].radiosity;
                        errmat.r = abs(errmat.r);
                        errmat.g = abs(errmat.g);
                        errmat.b = abs(errmat.b);

                        meanerr += errmat.r + errmat.g + errmat.b;
                        maxerr = maxerr<errmat.r?errmat.r:maxerr;
                        maxerr = maxerr<errmat.g?errmat.g:maxerr;
                        maxerr = maxerr<errmat.b?errmat.b:maxerr;
                        errors.push_back(errmat);
                        wk++;
                    } else {
                        colors.push_back(mmgr->getMedianVertexColor(i));
                        errors.push_back(Material(200, 0, 200));
                    }
                }
                if (dorerender) mmgr->writePlyMesh(rerenderedplyfilename, colors, meshcolorscale, gamma);
                if (doerror) mmgr->writePlyMesh(errorplyfilename, errors, .5);
                cout << "Mean error: " << meanerr/(3*data.size()) << "; " << "max error: " << maxerr << endl;
            }

            // Output scene file for rerendering
            if (pbrtfilename.length()) {
                room->wallMaterial.diffuse.r = ir.wallMaterial.r;
                room->wallMaterial.diffuse.g = ir.wallMaterial.g;
                room->wallMaterial.diffuse.b = ir.wallMaterial.b;
                room->floorMaterial.diffuse.r = ir.wallMaterial.r;
                room->floorMaterial.diffuse.g = ir.wallMaterial.g;
                room->floorMaterial.diffuse.b = ir.wallMaterial.b;
                room->ceilingMaterial.diffuse.r = ir.wallMaterial.r;
                room->ceilingMaterial.diffuse.g = ir.wallMaterial.g;
                room->ceilingMaterial.diffuse.b = ir.wallMaterial.b;
                if (imgr && room) {
                    if (cameranum < 0) {
                        for (int i = 0; i < imgr->size(); i++) {
                            char buf[100];
                            snprintf(buf, 100, pbrtfilename.c_str(), i);
                            outputPbrtFile(
                                    buf, room, *mmgr, ir.lights, ir.coeftype,
                                    imgr->getCamera(i));
                        }
                    } else {
                        outputPbrtFile(
                                pbrtfilename, room, *mmgr, ir.lights, ir.coeftype,
                                imgr->getCamera(cameranum));
                    }
                } else {
                    cout << "Error - need to supply imagemanager and roommodel to output pbrt file" << endl;
                }
            }
            return 0;
        }
    protected:
        virtual void printargs() {

        }
        virtual int _parseargs(int argc, char** argv) {
            if (!mmgr) return 0;
            if (pcl::console::find_argument(argc, argv, "-hemicuberesolution") >= 0) {
                pcl::console::parse_argument(argc, argv, "-hemicuberesolution", hemicuberesolution);
            }
            if (pcl::console::find_argument(argc, argv, "-label") >= 0) {
                pcl::console::parse_argument(argc, argv, "-label", label);
            }
            if (pcl::console::find_argument(argc, argv, "-discardthreshold") >= 0) {
                pcl::console::parse_argument(argc, argv, "-discardthreshold", discardthreshold);
            }
            if (pcl::console::find_argument(argc, argv, "-numsamples") >= 0) {
                pcl::console::parse_argument(argc, argv, "-numsamples", numsamples);
            }
            if (pcl::console::find_argument(argc, argv, "-scale") >= 0) {
                pcl::console::parse_argument(argc, argv, "-scale", scale);
            }
            if (pcl::console::find_argument(argc, argv, "-outputsamplesfile") >= 0) {
                pcl::console::parse_argument(argc, argv, "-outputsamplesfile", matlabfilename);
            }
            if (pcl::console::find_argument(argc, argv, "-inputsamplesfile") >= 0) {
                pcl::console::parse_argument(argc, argv, "-inputsamplesfile", inputmatlabfilename);
            }
            if (pcl::console::find_argument(argc, argv, "-inputlightfile") >= 0) {
                pcl::console::parse_argument(argc, argv, "-inputlightfile", lightfilename);
            }
            if (pcl::console::find_argument(argc, argv, "-outputpbrtfile") >= 0) {
                pcl::console::parse_argument(argc, argv, "-outputpbrtfile", pbrtfilename);
            }
            if (pcl::console::find_argument(argc, argv, "-cameraid") >= 0) {
                pcl::console::parse_argument(argc, argv, "-cameraid", cameranum);
            }
            if (pcl::console::find_argument(argc, argv, "-lambda") >= 0) {
                pcl::console::parse_argument(argc, argv, "-lambda", reglambda);
            }
            if (pcl::console::find_argument(argc, argv, "-outputcolorscale") >= 0) {
                pcl::console::parse_argument(argc, argv, "-outputcolorscale", meshcolorscale);
            }
            if (pcl::console::find_argument(argc, argv, "-outputgamma") >= 0) {
                pcl::console::parse_argument(argc, argv, "-outputgamma", gamma);
            }
            if (pcl::console::find_argument(argc, argv, "-outputtexture") >= 0) {
                pcl::console::parse_argument(argc, argv, "-outputtexture", texturefilename);
                solveTexture = true;
            }
            if (pcl::console::find_argument(argc, argv, "-outputerrormesh") >= 0) {
                pcl::console::parse_argument(argc, argv, "-outputerrormesh", errorplyfilename);
            }
            if (pcl::console::find_argument(argc, argv, "-outputrerendermesh") >= 0) {
                pcl::console::parse_argument(argc, argv, "-outputrerendermesh", rerenderedplyfilename);
            }
            return true;
        }
        int numsamples;
        float discardthreshold;
        float scale;
        int hemicuberesolution;
        string matlabfilename;
        string inputmatlabfilename;
        string pbrtfilename;
        string rerenderedplyfilename;
        string errorplyfilename;
        string lightfilename;
        int cameranum;
        int label;
        double reglambda;
        float meshcolorscale;
        float gamma;
        bool solveTexture;
        string texturefilename;
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

#ifndef _LIGHT_H
#define _LIGHT_H

#include <iterator>
#include <iostream>
#include <vector>
#include <Eigen/Eigen>

enum LightType {
    LIGHTTYPE_NULL=0,
    LIGHTTYPE_SH=2,
    LIGHTTYPE_ENVMAP=1,
    LIGHTTYPE_AREA=3,
    LIGHTTYPE_POINT=16,
    LIGHTTYPE_LINE=32,
    NUMLIGHTTYPES,
};

class Light {
    public:
        Light() : reglambda(0) {;}
        virtual int numParameters() const { return 0; }
        virtual int typeId() const { return LIGHTTYPE_NULL; }
        virtual double& coef(int n) { return v[n]; }
        virtual void setRegularization(double d) { reglambda = d; }
        virtual double getRegularization() const { return reglambda; }

        virtual double lightContribution(double* it) const;
        virtual double lightContribution(std::vector<double>::const_iterator it) const;
        virtual double lightContribution(Light* l) const;
        virtual bool addLightFF(
                double px, double py, double pz,
                double dx, double dy, double dz,
                double weight=1) { return false; }

        virtual void writeToStream(std::ostream& out, bool binary=false);
        virtual void readFromStream(std::istream& in, bool binary=false);

    protected:
        std::vector<double> v;
        double reglambda;
};

class AreaLight : public Light {
    public:
        AreaLight() { v.resize(numParameters(), 0); }
        virtual int numParameters() const { return 1; }
        virtual int typeId() const { return LIGHTTYPE_AREA; }

        virtual bool addLightFF(
                double px, double py, double pz,
                double dx, double dy, double dz,
                double weight=1)
        {
            v[0] += weight;
            return true;
        }

    protected:
};

class SHLight : public Light {
    public:
        SHLight() : numbands(5) { v.resize(numParameters(), 0); }
        virtual int numParameters() const { return numbands*numbands; }
        virtual int typeId() const { return LIGHTTYPE_SH; }

        virtual bool addLightFF(
                double px, double py, double pz,
                double dx, double dy, double dz,
                double weight=1);

    protected:
        int numbands;
};

class CubemapLight : public Light {
    public:
        CubemapLight() : res(5) { v.resize(numParameters(), 0); }
        virtual int numParameters() const { return res*res*6; }
        virtual int typeId() const { return LIGHTTYPE_ENVMAP; }

        int getCubemapResolution() const { return res; }

        virtual bool addLightFF(
                double px, double py, double pz,
                double dx, double dy, double dz,
                double weight=1);

    protected:
        int res;
};

class PointLight : public Light {
    public:
        PointLight(Light* l)
            : light(l)
        {
            setPosition(0,0,0);
        }
        PointLight(double x, double y, double z, Light* l)
            : light(l)
        {
            setPosition(x,y,z);
        }
        void setPosition(double x, double y, double z) {
            p[0] = x;
            p[1] = y;
            p[2] = z;
        }
        double getPosition(int n) {
            return p[n];
        }
        virtual int numParameters() const { return light->numParameters(); }
        virtual int typeId() const { return LIGHTTYPE_POINT | light->typeId(); }
        virtual double& coef(int n) { return light->coef(n); }

        virtual void addIncident(
                double px, double py, double pz,
                double dx, double dy, double dz,
                double weight=1);

        virtual void writeToStream(std::ostream& out, bool binary=false);
        virtual void readFromStream(std::istream& in, bool binary=false);
    protected:
        double p[3];
        Light* light;
};

class LineLight : public Light {
    public:
        LineLight()
            : numcells(16)
        {
            setPosition(0,0,0,0);
            setPosition(1,0,0,0);
            v.resize(numParameters());
        }
        LineLight(double x1, double y1, double z1,
                  double x2, double y2, double z2)
            : numcells(16)
        {
            setPosition(0,x1,y1,z1);
            setPosition(1,x2,y2,z2);
            v.resize(numParameters());
        }
        LineLight(Eigen::Vector3d p1, Eigen::Vector3d p2)
            : numcells(16)
        {
            setPosition(0,p1);
            setPosition(1,p2);
            v.resize(numParameters());
        }
        virtual int numParameters() const { return numcells; }
        virtual int typeId() const { return LIGHTTYPE_LINE | LIGHTTYPE_ENVMAP; }
        void setPosition(int n, double x, double y, double z) {
            setPosition(n, Eigen::Vector3d(x,y,z));
        }
        void setPosition(int n, Eigen::Vector3d pos);
        void computeFromPoints(std::vector<Eigen::Vector3d> pts);
        double getPosition(int n, int c) const {
            return p[n][c];
        }
        double getVector(int n) const {
            return vec[n];
        }
        Eigen::Vector3d getVector() const {
            return vec;
        }
        double getLength() const { return length; }
        Eigen::Vector3d getPosition(int n) const {
            return p[n];
        }

        virtual void addIncident(
                double px, double py, double pz,
                double dx, double dy, double dz,
                double weight=1);

        virtual void writeToStream(std::ostream& out, bool binary=false);
        virtual void readFromStream(std::istream& in, bool binary=false);
    protected:
        Eigen::Vector3d p[2];
        Eigen::Vector3d vec;
        Eigen::Vector3d perp;
        double length;
        int numcells;
};

Light* NewLightFromLightType(int type);
void writeLightsToFile(std::string filename, std::vector<std::vector<Light*> >& lights, bool binary=false);
void readLightsFromFile(std::string filename, std::vector<std::vector<Light*> >& lights, bool binary=false);
#endif

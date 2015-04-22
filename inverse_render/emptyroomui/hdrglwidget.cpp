#include <GL/glew.h>
#include <iostream>
#include <fstream>
#include "hdrglwidget.h"
#include "loadshader.h"

using namespace std;

static const char* shadername[NUM_TMOS] = {
    "linear shader",
    "logarithmic shader",
    "gamma shader",
    "float-as-int shader"
};
static const char* vertexshadertext =
"#version 400\n" \
"in vec2 v_coord;\n" \
"uniform sampler2D rendered_image;\n" \
"out vec2 f_texcoord;\n" \
"void main(void) {\n" \
  "gl_Position = vec4(v_coord, 0.0, 1.0);\n" \
  "f_texcoord = (v_coord + 1.0) / 2.0;\n" \
"}";

static const char* shadertext[NUM_TMOS] = {
    // linear shader
    "#version 400\n" \
    "uniform sampler2D rendered_image;\n" \
    "in vec2 f_texcoord;\n" \
    "uniform vec3 hdr_bounds;\n" \
    "out vec4 color;\n" \
    "void main(void) {\n" \
        "vec4 f = texture2D(rendered_image, f_texcoord);\n" \
        "color = clamp((f-hdr_bounds[0])/(hdr_bounds[1]-hdr_bounds[0]), 0, 1);\n" \
    "}",
    // Logarithmic shader
    "#version 400\n" \
    "uniform sampler2D rendered_image;\n" \
    "in vec2 f_texcoord;\n" \
    "uniform vec3 hdr_bounds;\n" \
    "out vec4 color;\n" \
    "void main(void) {\n" \
        "vec4 f = log(texture2D(rendered_image, f_texcoord));\n" \
        "vec3 lb = log(hdr_bounds);\n" \
        "color = clamp((f-lb[0])/(lb[1]-lb[0]), 0, 1);\n" \
    "}",
    // Gamma shader
    "#version 400\n" \
    "uniform sampler2D rendered_image;\n" \
    "in vec2 f_texcoord;\n" \
    "uniform vec3 hdr_bounds;\n" \
    "out vec4 color;\n" \
    "void main(void) {\n" \
        "vec4 f = texture2D(rendered_image, f_texcoord);\n" \
        "color = clamp(pow((f-hdr_bounds[0])/(hdr_bounds[1]-hdr_bounds[0]),hdr_bounds[2]*vec4(1,1,1,1)), 0, 1);\n" \
    "}",
    // Float-as-int shader
    "#version 400\n" \
    "uniform sampler2D rendered_image;\n" \
    "in vec2 f_texcoord;\n" \
    "uniform vec3 hdr_bounds;\n" \
    "out vec4 color;\n" \
    "void main(void) {\n" \
        "ivec4 f = floatBitsToInt(texture2D(rendered_image, f_texcoord));\n" \
        "color = clamp((f-hdr_bounds[0])/(hdr_bounds[1]-hdr_bounds[0]), 0, 1);\n" \
    "}",
};


HDRGlHelper::HDRGlHelper() :
    mapping(TMO_LINEAR), mini(0), maxi(1)
{
}

HDRGlHelper::~HDRGlHelper()
{
    glDeleteBuffers(1, &vbo_fbo_vertices);
    glDeleteBuffers(1, &fbo);
    glDeleteRenderbuffers(1, &fbo_z);
    glDeleteTextures(1, &fbo_tex);
}

void HDRGlHelper::setMapping(int v) {
    if (v != mapping) {
        mapping = v;
        emit update();
    }
}

void HDRGlHelper::setScale(int lo, int hi) {
    mini = LINTOLOG(lo);
    maxi = LINTOLOG(hi);
    emit update();
}

void HDRGlHelper::initializeHelper() {
    int w = 640;
    int h = 480;

    // Initialize fbo
    glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
    glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
    glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);

    glGenTextures(1, &fbo_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, w, h, 0, GL_RGB, GL_FLOAT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &fbo_z);
    glBindRenderbuffer(GL_RENDERBUFFER, fbo_z);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo_z);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        qDebug((const char*)gluErrorString(status) );
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Initialize texture quad vertices
    GLfloat fbo_vertices[] = {
        -1, -1,
         1, -1,
        -1,  1,
         1,  1,
    };
    glGenBuffers(1, &vbo_fbo_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_fbo_vertices);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fbo_vertices), fbo_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLchar* uniforms[NUM_UNIFORMS] = {
        "rendered_image",
        "hdr_bounds",
    };
    GLchar* attribute_name = "v_coord";
    for (int i = 0; i < NUM_TMOS; ++i) {
        progs[i].progid = LoadShader(vertexshadertext, shadertext[i]);
        progs[i].v_coord = glGetAttribLocation(progs[i].progid, attribute_name);
        if (progs[i].v_coord == -1) {
            qDebug("Error binding attribute %s in %s", attribute_name, shadername[i]);
            return;
        }
        for (int j = 0; j < NUM_UNIFORMS; ++j) {
            progs[i].uniform_ids[j] = glGetUniformLocation(progs[i].progid, uniforms[j]);
            if (progs[i].uniform_ids[j] == -1) {
                qDebug("Error binding uniform %s in %s", uniforms[j], shadername[i]);
                return;
            }
        }
    }
    glPushAttrib(GL_ALL_ATTRIB_BITS - GL_VIEWPORT_BIT);
}

void HDRGlHelper::renderToTexture(boost::function<void()> f) {
    glEnable(GL_TEXTURE_2D);
    glUseProgram(0);
    int lastfbo;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &lastfbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glPopAttrib();
    if (f) f();
    glPushAttrib(GL_ALL_ATTRIB_BITS-GL_VIEWPORT_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, lastfbo);
}

void HDRGlHelper::paintHelper() {
    renderToTexture(renderfunc);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    HDRShaderProgram& p = progs[mapping];
    glUseProgram(p.progid);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glUniform1i(p.uniform_ids[UNIFORM_FBO_TEXTURE], 0);
    glUniform3f(p.uniform_ids[UNIFORM_HDR_BOUNDS], mini, maxi, 2.2);
    glEnableVertexAttribArray(p.v_coord);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_fbo_vertices);
    glVertexAttribPointer(
      p.v_coord,  // attribute
      2,                  // number of elements per vertex, here (x,y)
      GL_FLOAT,           // the type of each element
      GL_FALSE,           // take our values as-is
      0,                  // no extra data between each position
      0                   // offset of first element
    );
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(p.v_coord);
}

void HDRGlHelper::readFromTexture(int x, int y, int w, int h, float* data) {
    float* tmp = new float[w*h*3];
    float* depth = new float[w*h];
    float* currcolor = tmp;
    float* currdepth = depth;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadPixels(x,y,w,h,GL_RGB,GL_FLOAT,tmp);
    glReadPixels(x,y,w,h,GL_DEPTH_COMPONENT,GL_FLOAT,depth);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    for (int i = 0; i < w*h; ++i) {
        for (int j = 0; j < 3; ++j) *(data++) = *(currcolor++);
        *(data++) = *(currdepth++);
    }
    delete [] tmp;
    delete [] depth;
}

void HDRGlHelper::resizeHelper(int width, int height) {
    currw = width;
    currh = height;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, fbo_z);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    emit update();
}

void HDRQGlViewerWidget::resizeGL(int width, int height) {
    QGLViewer::resizeGL(width, height);
    helper.resizeHelper(width, height);
    _doresize(width, height);
}

void HDRQGlViewerWidget::select(const QPoint &point) {
    makeCurrent();
    camera()->loadProjectionMatrix();
    camera()->loadModelViewMatrix();
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    helper.renderToTexture(boost::bind(&HDRQGlViewerWidget::drawWithNames, this));

    int w = selectRegionWidth();
    int h = selectRegionHeight();
    int x = point.x();
    int y = height() - point.y() - 1;

    float* data = new float[w*h*4];
    helper.readFromTexture(x-w/2,y-h/2,w,h,data);
    float closest = 1e8;
    int closestid = -1;
    for (int i = 0; i < w*h; ++i) {
        int n = RGBToIndex(data+4*i);
        if (data[4*i+3] < closest) {
            closestid = n;
            closest = data[4*i+3];
        }
    }
    setSelectedName(closestid);
    postSelection(point);
}

static const int base = 17;

int HDRQGlViewerWidget::RGBToIndex(float* rgb) {
    int ret = 0;
    int currpow = base-1;
    for (int i = 0; i < 3; ++i) {
        ret += rgb[i]*currpow + 0.5;
        currpow *= base;
    }
    return ret-1;
}

void HDRQGlViewerWidget::IndexToRGB(int i, float* rgb) {
    int v = i+1;
    for (int i = 0; i < 3; ++i) {
        rgb[i] = (v%base)/(float) (base-1);
        v /= base;
    }
}

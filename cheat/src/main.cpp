#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <cmath>
#include <cstring>
#include <atomic>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include "And64InlineHook.hpp"

#define LOG_TAG "Veolecte"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static GLuint g_program = 0;
static GLuint g_vbo = 0;
static GLint g_locScreen = -1;
static int g_screenW = 1080;
static int g_screenH = 1920;
static bool g_rendererReady = false;

struct Vertex { float x, y, r, g, b, a; };

static const char* VS_SRC =
    "#version 300 es\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec4 aColor;\n"
    "uniform vec2 uScreen;\n"
    "out vec4 vColor;\n"
    "void main(){\n"
    "  vec2 ndc=(aPos/uScreen)*2.0-1.0;\n"
    "  ndc.y=-ndc.y;\n"
    "  gl_Position=vec4(ndc,0.0,1.0);\n"
    "  vColor=aColor;\n"
    "}\n";

static const char* FS_SRC =
    "#version 300 es\n"
    "precision mediump float;\n"
    "in vec4 vColor;\n"
    "out vec4 frag;\n"
    "void main(){ frag=vColor; }\n";

static GLuint CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

static void RendererInit() {
    if (g_rendererReady) return;
    GLuint vs = CompileShader(GL_VERTEX_SHADER, VS_SRC);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, FS_SRC);
    g_program = glCreateProgram();
    glAttachShader(g_program, vs);
    glAttachShader(g_program, fs);
    glLinkProgram(g_program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    g_locScreen = glGetUniformLocation(g_program, "uScreen");
    glGenBuffers(1, &g_vbo);
    g_rendererReady = true;
    LOGI("Renderer ready");
}

static void DrawVerts(const Vertex* v, int count, GLenum mode) {
    if (!g_rendererReady || count <= 0) return;
    glUseProgram(g_program);
    glUniform2f(g_locScreen, (float)g_screenW, (float)g_screenH);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vertex), v, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2*sizeof(float)));
    glDrawArrays(mode, 0, count);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

static void DrawRectFilled(float x, float y, float w, float h,
                           float r, float g, float b, float a) {
    Vertex v[6] = {
        {x,y,r,g,b,a},{x+w,y,r,g,b,a},{x+w,y+h,r,g,b,a},
        {x,y,r,g,b,a},{x+w,y+h,r,g,b,a},{x,y+h,r,g,b,a},
    };
    DrawVerts(v, 6, GL_TRIANGLES);
}

static void DrawLine(float x1, float y1, float x2, float y2,
                     float r, float g, float b, float a, float t) {
    float dx=x2-x1, dy=y2-y1;
    float len=sqrtf(dx*dx+dy*dy);
    if(len<0.001f) return;
    float nx=-dy/len*t*0.5f, ny=dx/len*t*0.5f;
    Vertex v[6] = {
        {x1+nx,y1+ny,r,g,b,a},{x1-nx,y1-ny,r,g,b,a},{x2+nx,y2+ny,r,g,b,a},
        {x1-nx,y1-ny,r,g,b,a},{x2-nx,y2-ny,r,g,b,a},{x2+nx,y2+ny,r,g,b,a},
    };
    DrawVerts(v, 6, GL_TRIANGLES);
}

static void DrawRectOutline(float x, float y, float w, float h,
                            float r, float g, float b, float a, float t) {
    DrawLine(x,y,x+w,y,r,g,b,a,t);
    DrawLine(x,y+h,x+w,y+h,r,g,b,a,t);
    DrawLine(x,y,x,y+h,r,g,b,a,t);
    DrawLine(x+w,y,x+w,y+h,r,g,b,a,t);
}

static void ESPRender() {
    DrawRectOutline(440,860,200,200, 0.67f,0.4f,1.0f,0.9f, 3.0f);
    DrawRectFilled(460,880,160,40, 0.4f,0.2f,0.8f,0.7f);
}

static void MenuRender() {
    DrawRectFilled(50,50,360,320, 0.06f,0.04f,0.1f,0.92f);
    DrawRectFilled(50,50,360,55, 0.3f,0.15f,0.5f,1.0f);
    DrawRectFilled(50,105,360,2, 0.67f,0.4f,1.0f,0.6f);
    DrawRectFilled(70,130,320,50, 0.2f,0.5f,0.2f,0.85f);
    DrawRectFilled(70,200,320,50, 0.5f,0.2f,0.2f,0.85f);
    DrawRectFilled(70,270,20,20, 0.67f,0.4f,1.0f,1.0f);
    DrawRectOutline(50,50,360,320, 0.67f,0.4f,1.0f,0.4f, 1.5f);
}

typedef EGLBoolean (*eglSwapBuffers_t)(EGLDisplay, EGLSurface);
static eglSwapBuffers_t g_origSwapBuffers = nullptr;
static std::atomic<bool> g_initDone(false);

static void EnsureInit() {
    if (g_initDone.load()) return;
    GLint vp[4];
    glGetIntegerv(GL_VIEWPORT, vp);
    if (vp[2] > 0 && vp[3] > 0) {
        g_screenW = vp[2];
        g_screenH = vp[3];
    }
    RendererInit();
    g_initDone.store(true);
    LOGI("Veolecte active (%dx%d)", g_screenW, g_screenH);
}

static EGLBoolean HookedSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    EnsureInit();

    GLint prevProgram=0, prevVBO=0, prevVP[4], prevSc[4];
    GLint prevBlendSrc=GL_ONE, prevBlendDst=GL_ZERO;
    GLboolean prevBlend=GL_FALSE, prevDepth=GL_FALSE;
    GLboolean prevCull=GL_FALSE, prevScissor=GL_FALSE;

    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevVBO);
    glGetIntegerv(GL_VIEWPORT, prevVP);
    glGetIntegerv(GL_SCISSOR_BOX, prevSc);
    glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrc);
    glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDst);
    prevBlend   = glIsEnabled(GL_BLEND);
    prevDepth   = glIsEnabled(GL_DEPTH_TEST);
    prevCull    = glIsEnabled(GL_CULL_FACE);
    prevScissor = glIsEnabled(GL_SCISSOR_TEST);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ESPRender();
    MenuRender();

    if(prevDepth)   glEnable(GL_DEPTH_TEST);   else glDisable(GL_DEPTH_TEST);
    if(prevBlend)   glEnable(GL_BLEND);        else glDisable(GL_BLEND);
    if(prevCull)    glEnable(GL_CULL_FACE);    else glDisable(GL_CULL_FACE);
    if(prevScissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glBlendFunc(prevBlendSrc, prevBlendDst);
    glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);
    glScissor(prevSc[0], prevSc[1], prevSc[2], prevSc[3]);
    glBindBuffer(GL_ARRAY_BUFFER, prevVBO);
    glUseProgram(prevProgram);

    return g_origSwapBuffers(dpy, surface);
}

static void* CheatThread(void*) {
    sleep(5);
    void* libEGL = dlopen("libEGL.so", RTLD_NOW);
    if (!libEGL) { LOGE("dlopen libEGL failed"); return nullptr; }
    void* target = dlsym(libEGL, "eglSwapBuffers");
    if (!target) { LOGE("dlsym failed"); return nullptr; }

    A64HookFunction(target, (void*)&HookedSwapBuffers, (void**)&g_origSwapBuffers);

    if (g_origSwapBuffers) {
        LOGI("Hook installed OK");
    } else {
        LOGE("Hook FAILED");
    }
    return nullptr;
}

__attribute__((constructor))
static void OnLoad() {
    LOGI("Veolecte loaded");
    pthread_t tid;
    pthread_create(&tid, nullptr, CheatThread, nullptr);
    pthread_detach(tid);
}

#ifndef LBMRENDERER_H
#define LBMRENDERER_H

#include <windows.h>
#include <GL/gl.h>

typedef struct lbmRenderer
{
    HWND hwnd;
    HDC dc;
    HGLRC glrc;
    float r;
    int w;
    int h;
} lbmRenderer;

// Global setup
void lbmRendererStartup();
void lbmRendererShutdown();

lbmRenderer *lbmRendererCreate();
void lbmRendererDestroy(lbmRenderer *renderer);

typedef void (*lbmUpdateFunc)(void *userdata, float dt);

void lbmAddUpdateFunc(lbmUpdateFunc func, void *userdata);
void lbmPump();

#endif

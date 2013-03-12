#include "lbmRenderer.h"

#include "dyn.h"

#include <stdlib.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Globals and forwards

static const char * sClassName = "LBMWindow";

static LRESULT APIENTRY lbmRendererWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
static void lbmRendererUpdate(lbmRenderer * renderer, float dt);

typedef struct lbmUpdateInfo
{
    lbmUpdateFunc func;
    void *userdata;
} lbmUpdateInfo;

static lbmUpdateInfo *sUpdates = NULL;

// ---------------------------------------------------------------------------
// Global setup

void lbmRendererStartup()
{
    WNDCLASS wndClass;
    wndClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = lbmRendererWndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = GetModuleHandle(NULL);
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = GetStockObject(BLACK_BRUSH);
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = sClassName;
    RegisterClass(&wndClass);

    daCreate(&sUpdates, sizeof(lbmUpdateInfo));
}

void lbmRendererShutdown()
{
    daDestroy(&sUpdates, NULL);
}

// ---------------------------------------------------------------------------
// Helpers

static void lbmSetupPixelFormat(HDC dc)
{
    PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),  /* size */
        1,                              /* version */
        PFD_SUPPORT_OPENGL |
        PFD_DRAW_TO_WINDOW |
        PFD_DOUBLEBUFFER,               /* support double-buffering */
        PFD_TYPE_RGBA,                  /* color type */
        16,                             /* prefered color depth */
        0, 0, 0, 0, 0, 0,               /* color bits (ignored) */
        0,                              /* no alpha buffer */
        0,                              /* alpha bits (ignored) */
        0,                              /* no accumulation buffer */
        0, 0, 0, 0,                     /* accum bits (ignored) */
        16,                             /* depth buffer */
        0,                              /* no stencil buffer */
        0,                              /* no auxiliary buffers */
        PFD_MAIN_PLANE,                 /* main layer */
        0,                              /* reserved */
        0, 0, 0,                        /* no layer, visible, damage masks */
    };
    int pixelFormat;

    pixelFormat = ChoosePixelFormat(dc, &pfd);
    if (pixelFormat == 0)
    {
        MessageBox(WindowFromDC(dc), "ChoosePixelFormat failed.", "Error", MB_ICONERROR | MB_OK);
        exit(1);
    }

    if (SetPixelFormat(dc, pixelFormat, &pfd) != TRUE)
    {
        MessageBox(WindowFromDC(dc), "SetPixelFormat failed.", "Error", MB_ICONERROR | MB_OK);
        exit(1);
    }
}

// ---------------------------------------------------------------------------
// lbmRenderer

static void lbmRendererGLInit(lbmRenderer *renderer)
{
    wglMakeCurrent(renderer->dc, renderer->glrc);

    /* set viewing projection */
    glMatrixMode(GL_PROJECTION);
    glFrustum(-0.5F, 0.5F, -0.5F, 0.5F, 1.0F, 3.0F);

    /* position viewer */
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0.0F, 0.0F, -2.0F);

    /* position object */
    glRotatef(30.0F, 1.0F, 0.0F, 0.0F);
    glRotatef(30.0F, 0.0F, 1.0F, 0.0F);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
}

static void lbmRendererRedraw(lbmRenderer *renderer)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0F, 0.0F, -2.0F);
    glRotatef(renderer->r, 0.0F, 1.0F, 0.0F);

    glBegin(GL_QUADS);
    glNormal3f(0.0F, 0.0F, 1.0F);
    glVertex3f(0.5F, 0.5F, 0.5F); glVertex3f(-0.5F, 0.5F, 0.5F);
    glVertex3f(-0.5F,-0.5F, 0.5F); glVertex3f(0.5F,-0.5F, 0.5F);

    glNormal3f(0.0F, 0.0F,-1.0F);
    glVertex3f(-0.5F,-0.5F,-0.5F); glVertex3f(-0.5F, 0.5F,-0.5F);
    glVertex3f(0.5F, 0.5F,-0.5F); glVertex3f(0.5F,-0.5F,-0.5F);

    glNormal3f(0.0F, 1.0F, 0.0F);
    glVertex3f(0.5F, 0.5F, 0.5F); glVertex3f(0.5F, 0.5F,-0.5F);
    glVertex3f(-0.5F, 0.5F,-0.5F); glVertex3f(-0.5F, 0.5F, 0.5F);

    glNormal3f(0.0F,-1.0F, 0.0F);
    glVertex3f(-0.5F,-0.5F,-0.5F); glVertex3f(0.5F,-0.5F,-0.5F);
    glVertex3f(0.5F,-0.5F, 0.5F); glVertex3f(-0.5F,-0.5F, 0.5F);

    glNormal3f(1.0F, 0.0F, 0.0F);
    glVertex3f(0.5F, 0.5F, 0.5F); glVertex3f(0.5F,-0.5F, 0.5F);
    glVertex3f(0.5F,-0.5F,-0.5F); glVertex3f(0.5F, 0.5F,-0.5F);

    glNormal3f(-1.0F, 0.0F, 0.0F);
    glVertex3f(-0.5F,-0.5F,-0.5F); glVertex3f(-0.5F,-0.5F, 0.5F);
    glVertex3f(-0.5F, 0.5F, 0.5F); glVertex3f(-0.5F, 0.5F,-0.5F);
    glEnd();

    SwapBuffers(renderer->dc);
}

static void lbmRendererResize(lbmRenderer *renderer, int w, int h)
{
    renderer->w = w;
    renderer->h = h;
    glViewport(0, 0, w, h);
}

static LRESULT APIENTRY lbmRendererWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    lbmRenderer * renderer = (lbmRenderer *)GetWindowLong(hwnd, GWL_USERDATA);
    switch (message)
    {
        case WM_DESTROY:
            if(renderer)
            {
                lbmRendererDestroy(renderer);
                PostQuitMessage(0);
            }
            return 0;
        case WM_SIZE:
            if (renderer && renderer->glrc)
            {
                int w = (int) LOWORD(lParam);
                int h = (int) HIWORD(lParam);
                lbmRendererResize(renderer, w, h);
                return 0;
            }
        case WM_PAINT:
        {
            if(renderer)
            {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                if (renderer->glrc)
                {
                    lbmRendererRedraw(renderer);
                }
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
        break;
        case WM_CHAR:
            switch ((int)wParam)
            {
                case VK_ESCAPE:
                    DestroyWindow(hwnd);
                    return 0;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

static WINDOWPLACEMENT g_wpPrev = { sizeof(g_wpPrev) };

lbmRenderer * lbmRendererCreate()
{
    MONITORINFO mi = { sizeof(mi) };
    lbmRenderer * renderer = calloc(1, sizeof(lbmRenderer));
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);

    renderer->hwnd = CreateWindow(sClassName, "LBM", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 0, 0, w, h, NULL, NULL, GetModuleHandle(NULL), NULL);
    SetWindowLong(renderer->hwnd, GWL_USERDATA, (LONG)renderer);

    if (GetWindowPlacement(renderer->hwnd, &g_wpPrev) && GetMonitorInfo(MonitorFromWindow(renderer->hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
    {
        DWORD dwStyle = GetWindowLong(renderer->hwnd, GWL_STYLE);
        SetWindowLong(renderer->hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(renderer->hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

    renderer->dc = GetDC(renderer->hwnd);
    lbmSetupPixelFormat(renderer->dc);
    renderer->glrc = wglCreateContext(renderer->dc);
    lbmRendererGLInit(renderer);

    ShowWindow(renderer->hwnd, SW_SHOW);
    UpdateWindow(renderer->hwnd);

    lbmAddUpdateFunc((lbmUpdateFunc)lbmRendererUpdate, (void *)renderer);
    return renderer;
}

void lbmRendererDestroy(lbmRenderer * renderer)
{
    SetWindowLong(renderer->hwnd, GWL_USERDATA, 0);
    if (renderer->glrc)
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(renderer->glrc);
    }
    ReleaseDC(renderer->hwnd, renderer->dc);
    free(renderer);
}

static void lbmRendererUpdate(lbmRenderer * renderer, float dt)
{
    renderer->r += 90.0f * dt;
    if(renderer->r > 360.0f)
    {
        renderer->r -= 360.0f;
    }

    printf("%d %d\n", renderer->w, renderer->h);

    lbmRendererRedraw(renderer);
}

// ---------------------------------------------------------------------------
// Update/Pump

void lbmAddUpdateFunc(lbmUpdateFunc func, void *userdata)
{
    lbmUpdateInfo info;
    info.func = func;
    info.userdata = userdata;
    daPush(&sUpdates, info);
}

void lbmUpdate(float dt)
{
    int i;
    for(i = 0; i < daSize(&sUpdates); ++i)
    {
        lbmUpdateInfo *info = &sUpdates[i];
        info->func(info->userdata, dt);
    }
}

void lbmPump()
{
    unsigned int lastTick = GetTickCount();
    MSG msg;

    PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else // No message to do
        {
            unsigned int tick = GetTickCount();
            unsigned int diffTick = tick - lastTick;
            if(diffTick > 10)
            {
                lbmUpdate((float)(diffTick) / 1000.0f);
                lastTick = tick;
            }
        }
    }
}

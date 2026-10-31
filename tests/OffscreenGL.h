// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <iostream>
class OffscreenGL {
public:
    EGLDisplay display=EGL_NO_DISPLAY;
    EGLSurface surface=EGL_NO_SURFACE;
    EGLContext context=EGL_NO_CONTEXT;
    bool Open(int width=640, int height=400)
    {
        auto get_platform=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        display=get_platform ? get_platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr) : eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major=0,minor=0;
        if (display==EGL_NO_DISPLAY || !eglInitialize(display,&major,&minor) || !eglBindAPI(EGL_OPENGL_API)) return false;
        const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,
            EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};
        EGLConfig config{}; EGLint count=0;
        if (!eglChooseConfig(display,attributes,&config,1,&count) || !count) return false;
        const EGLint size[]={EGL_WIDTH,width,EGL_HEIGHT,height,EGL_NONE};
        surface=eglCreatePbufferSurface(display,config,size);
        context=eglCreateContext(display,config,EGL_NO_CONTEXT,nullptr);
        return surface!=EGL_NO_SURFACE && context!=EGL_NO_CONTEXT && eglMakeCurrent(display,surface,surface,context);
    }
    ~OffscreenGL()
    {
        if (display!=EGL_NO_DISPLAY) {
            eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
            if (context!=EGL_NO_CONTEXT) eglDestroyContext(display,context);
            if (surface!=EGL_NO_SURFACE) eglDestroySurface(display,surface);
            eglTerminate(display);
        }
    }
};

// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_LINUX_TESTING_MOCK_EPOXY_H_
#define FLUTTER_SHELL_PLATFORM_LINUX_TESTING_MOCK_EPOXY_H_

#include "gmock/gmock.h"

#include <epoxy/egl.h>
#include <epoxy/gl.h>

namespace flutter {
namespace testing {

using EGLProcAddress = void (*)(void);

class MockEpoxy {
 public:
  MockEpoxy();
  ~MockEpoxy();

  MOCK_METHOD(bool, epoxy_has_gl_extension, (const char* extension));
  MOCK_METHOD(bool,
              epoxy_has_egl_extension,
              (EGLDisplay display, const char* extension));
  MOCK_METHOD(int, epoxy_egl_version, (EGLDisplay display));
  MOCK_METHOD(bool, epoxy_is_desktop_gl, ());
  MOCK_METHOD(int, epoxy_gl_version, ());
  MOCK_METHOD(EGLProcAddress, eglGetProcAddress, (const char* procname));
  MOCK_METHOD(EGLDisplay, eglGetCurrentDisplay, ());
  MOCK_METHOD(EGLBoolean,
              eglChooseConfig,
              (EGLDisplay dpy,
               const EGLint* attrib_list,
               EGLConfig* configs,
               EGLint config_size,
               EGLint* num_config));
  MOCK_METHOD(EGLSurface,
              eglCreatePbufferSurface,
              (EGLDisplay dpy, EGLConfig config, const EGLint* attrib_list));
  MOCK_METHOD(
      EGLBoolean,
      eglMakeCurrent,
      (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx));
  MOCK_METHOD(EGLImage,
              eglCreateImage,
              (EGLDisplay dpy,
               EGLContext ctx,
               EGLenum target,
               EGLClientBuffer buffer,
               const EGLAttrib* attrib_list));
  MOCK_METHOD(EGLImage,
              eglCreateImageKHR,
              (EGLDisplay dpy,
               EGLContext ctx,
               EGLenum target,
               EGLClientBuffer buffer,
               const EGLint* attrib_list));
  MOCK_METHOD(EGLBoolean, eglDestroyImage, (EGLDisplay dpy, EGLImage image));
  MOCK_METHOD(EGLBoolean, eglDestroyImageKHR, (EGLDisplay dpy, EGLImage image));
  MOCK_METHOD(EGLSync,
              eglCreateSync,
              (EGLDisplay dpy, EGLenum type, const EGLAttrib* attrib_list));
  MOCK_METHOD(EGLSyncKHR,
              eglCreateSyncKHR,
              (EGLDisplay dpy, EGLenum type, const EGLint* attrib_list));
  MOCK_METHOD(EGLint,
              eglClientWaitSync,
              (EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout));
  MOCK_METHOD(
      EGLint,
      eglClientWaitSyncKHR,
      (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout));
  MOCK_METHOD(EGLBoolean,
              eglWaitSync,
              (EGLDisplay dpy, EGLSync sync, EGLint flags));
  MOCK_METHOD(EGLint,
              eglWaitSyncKHR,
              (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags));
  MOCK_METHOD(EGLBoolean, eglDestroySync, (EGLDisplay dpy, EGLSync sync));
  MOCK_METHOD(EGLBoolean, eglDestroySyncKHR, (EGLDisplay dpy, EGLSyncKHR sync));
  MOCK_METHOD(void, glClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a));
  MOCK_METHOD(void,
              glBlitFramebuffer,
              (GLint srcX0,
               GLint srcY0,
               GLint srcX1,
               GLint srcY1,
               GLint dstX0,
               GLint dstY0,
               GLint dstX1,
               GLint dstY1,
               GLbitfield mask,
               GLenum filter));
  MOCK_METHOD(void,
              glDeleteFramebuffers,
              (GLsizei n, const GLuint* framebuffers));
  MOCK_METHOD(void,
              glDeleteRenderbuffers,
              (GLsizei n, const GLuint* renderbuffers));
  MOCK_METHOD(void, glDeleteTextures, (GLsizei n, const GLuint* textures));
  MOCK_METHOD(void, glGenFramebuffers, (GLsizei n, GLuint* framebuffers));
  MOCK_METHOD(void, glGenRenderbuffers, (GLsizei n, GLuint* renderbuffers));
  MOCK_METHOD(void, glGenTextures, (GLsizei n, GLuint* textures));
  MOCK_METHOD(GLenum, glCheckFramebufferStatus, (GLenum target));
  MOCK_METHOD(GLenum, glGetError, ());
  MOCK_METHOD(void, glFinish, ());
  MOCK_METHOD(void,
              glEGLImageTargetTexture2DOES,
              (GLenum target, GLeglImageOES image));
  MOCK_METHOD(const GLubyte*, glGetString, (GLenum pname));
  MOCK_METHOD(void,
              glUniform2f,
              (GLint location, GLfloat value0, GLfloat value1));
};

}  // namespace testing
}  // namespace flutter

#endif  // FLUTTER_SHELL_PLATFORM_LINUX_TESTING_MOCK_EPOXY_H_

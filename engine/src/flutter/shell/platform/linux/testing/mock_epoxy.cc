// Copyright 2013 The Flutter Authors. All rights reserved.

// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/linux/testing/mock_epoxy.h"
#include <string.h>
#include <vector>
#include "flutter/fml/logging.h"

using namespace flutter::testing;

typedef struct {
  EGLint config_id;
  EGLint buffer_size;
  EGLint color_buffer_type;
  EGLint transparent_type;
  EGLint level;
  EGLint red_size;
  EGLint green_size;
  EGLint blue_size;
  EGLint alpha_size;
  EGLint depth_size;
  EGLint stencil_size;
  EGLint samples;
  EGLint sample_buffers;
  EGLint native_visual_id;
  EGLint native_visual_type;
  EGLint native_renderable;
  EGLint config_caveat;
  EGLint bind_to_texture_rgb;
  EGLint bind_to_texture_rgba;
  EGLint renderable_type;
  EGLint conformant;
  EGLint surface_type;
  EGLint max_pbuffer_width;
  EGLint max_pbuffer_height;
  EGLint max_pbuffer_pixels;
  EGLint min_swap_interval;
  EGLint max_swap_interval;
} MockConfig;

typedef struct {
} MockDisplay;

typedef struct {
} MockContext;

typedef struct {
} MockSurface;

typedef struct {
} MockImage;

typedef struct {
} MockSync;

static MockEpoxy* mock = nullptr;
static bool display_initialized = false;
static MockDisplay mock_display;
static MockConfig mock_config;
static MockContext mock_context;
static MockSurface mock_surface;
static MockImage mock_image;
static MockSync mock_sync;

static EGLint mock_error = EGL_SUCCESS;

MockEpoxy::MockEpoxy() {
  mock = this;
  ON_CALL(*this, epoxy_egl_version(::testing::_))
      .WillByDefault(::testing::Return(15));
  ON_CALL(*this, eglGetCurrentDisplay)
      .WillByDefault(
          ::testing::Return(reinterpret_cast<EGLDisplay>(&mock_display)));
  ON_CALL(*this, eglMakeCurrent).WillByDefault(::testing::Return(EGL_TRUE));
  ON_CALL(*this, eglCreateImage)
      .WillByDefault(
          ::testing::Return(reinterpret_cast<EGLImage>(&mock_image)));
  ON_CALL(*this, eglCreateImageKHR)
      .WillByDefault(
          ::testing::Return(reinterpret_cast<EGLImageKHR>(&mock_image)));
  ON_CALL(*this, eglDestroyImage).WillByDefault(::testing::Return(EGL_TRUE));
  ON_CALL(*this, eglDestroyImageKHR).WillByDefault(::testing::Return(EGL_TRUE));
  ON_CALL(*this, eglCreateSync)
      .WillByDefault(::testing::Return(reinterpret_cast<EGLSync>(&mock_sync)));
  ON_CALL(*this, eglCreateSyncKHR)
      .WillByDefault(
          ::testing::Return(reinterpret_cast<EGLSyncKHR>(&mock_sync)));
  ON_CALL(*this, eglClientWaitSync)
      .WillByDefault(::testing::Return(EGL_CONDITION_SATISFIED));
  ON_CALL(*this, eglClientWaitSyncKHR)
      .WillByDefault(::testing::Return(EGL_CONDITION_SATISFIED_KHR));
  ON_CALL(*this, eglWaitSync).WillByDefault(::testing::Return(EGL_TRUE));
  ON_CALL(*this, eglWaitSyncKHR).WillByDefault(::testing::Return(EGL_TRUE));
  ON_CALL(*this, eglDestroySync).WillByDefault(::testing::Return(EGL_TRUE));
  ON_CALL(*this, eglDestroySyncKHR).WillByDefault(::testing::Return(EGL_TRUE));
  ON_CALL(*this, glCheckFramebufferStatus)
      .WillByDefault(::testing::Return(GL_FRAMEBUFFER_COMPLETE));
  ON_CALL(*this, glGetError).WillByDefault(::testing::Return(GL_NO_ERROR));
  ON_CALL(*this, glGetString(GL_VERSION))
      .WillByDefault(
          ::testing::Return(reinterpret_cast<const GLubyte*>("OpenGL ES 2.0")));
  ON_CALL(*this, eglChooseConfig)
      .WillByDefault([](EGLDisplay dpy, const EGLint* attrib_list,
                        EGLConfig* configs, EGLint config_size,
                        EGLint* num_config) {
        EGLint n_returned = 0;
        if (configs != nullptr && config_size >= 1) {
          configs[0] = &mock_config;
          n_returned = 1;
        }
        if (num_config != nullptr) {
          *num_config = configs == nullptr ? 1 : n_returned;
        }
        mock_error = EGL_SUCCESS;
        return EGL_TRUE;
      });
}

MockEpoxy::~MockEpoxy() {
  if (mock == this) {
    mock = nullptr;
  }
}

static bool check_display(EGLDisplay dpy) {
  if (dpy == nullptr) {
    mock_error = EGL_BAD_DISPLAY;
    return false;
  }

  return true;
}

static bool check_initialized(EGLDisplay dpy) {
  if (!display_initialized) {
    mock_error = EGL_NOT_INITIALIZED;
    return false;
  }

  return true;
}

static bool check_config(EGLConfig config) {
  if (config == nullptr) {
    mock_error = EGL_BAD_CONFIG;
    return false;
  }

  return true;
}

static EGLBoolean bool_success() {
  mock_error = EGL_SUCCESS;
  return EGL_TRUE;
}

static EGLBoolean bool_failure(EGLint error) {
  mock_error = error;
  return EGL_FALSE;
}

EGLBoolean _eglBindAPI(EGLenum api) {
  return bool_success();
}

EGLBoolean _eglChooseConfig(EGLDisplay dpy,
                            const EGLint* attrib_list,
                            EGLConfig* configs,
                            EGLint config_size,
                            EGLint* num_config) {
  if (!check_display(dpy) || !check_initialized(dpy)) {
    return EGL_FALSE;
  }

  if (mock != nullptr) {
    return mock->eglChooseConfig(dpy, attrib_list, configs, config_size,
                                 num_config);
  }

  if (configs == nullptr) {
    if (num_config != nullptr) {
      *num_config = 1;
    }
    return bool_success();
  }

  EGLint n_returned = 0;
  if (config_size >= 1) {
    configs[0] = &mock_config;
    n_returned++;
  }

  if (num_config != nullptr) {
    *num_config = n_returned;
  }

  return bool_success();
}

EGLContext _eglCreateContext(EGLDisplay dpy,
                             EGLConfig config,
                             EGLContext share_context,
                             const EGLint* attrib_list) {
  if (!check_display(dpy) || !check_initialized(dpy) || !check_config(config)) {
    return EGL_NO_CONTEXT;
  }

  mock_error = EGL_SUCCESS;
  return &mock_context;
}

EGLContext _eglGetCurrentContext() {
  return &mock_context;
}

EGLSurface _eglCreatePbufferSurface(EGLDisplay dpy,
                                    EGLConfig config,
                                    const EGLint* attrib_list) {
  if (!check_display(dpy) || !check_initialized(dpy) || !check_config(config)) {
    return EGL_NO_SURFACE;
  }

  mock_error = EGL_SUCCESS;
  return mock != nullptr
             ? mock->eglCreatePbufferSurface(dpy, config, attrib_list)
             : &mock_surface;
}

EGLBoolean _eglDestroyContext(EGLDisplay dpy, EGLContext context) {
  return check_display(dpy) ? bool_success() : EGL_FALSE;
}

EGLBoolean _eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
  return check_display(dpy) ? bool_success() : EGL_FALSE;
}

EGLSurface _eglCreateWindowSurface(EGLDisplay dpy,
                                   EGLConfig config,
                                   EGLNativeWindowType win,
                                   const EGLint* attrib_list) {
  if (!check_display(dpy) || !check_initialized(dpy) || !check_config(config)) {
    return EGL_NO_SURFACE;
  }

  mock_error = EGL_SUCCESS;
  return &mock_surface;
}

EGLBoolean _eglGetConfigAttrib(EGLDisplay dpy,
                               EGLConfig config,
                               EGLint attribute,
                               EGLint* value) {
  if (!check_display(dpy) || !check_initialized(dpy) || !check_config(config)) {
    return EGL_FALSE;
  }

  MockConfig* c = static_cast<MockConfig*>(config);
  switch (attribute) {
    case EGL_CONFIG_ID:
      *value = c->config_id;
      return bool_success();
    case EGL_BUFFER_SIZE:
      *value = c->buffer_size;
      return bool_success();
    case EGL_COLOR_BUFFER_TYPE:
      *value = c->color_buffer_type;
      return bool_success();
    case EGL_TRANSPARENT_TYPE:
      *value = c->transparent_type;
      return bool_success();
    case EGL_LEVEL:
      *value = c->level;
      return bool_success();
    case EGL_RED_SIZE:
      *value = c->red_size;
      return bool_success();
    case EGL_GREEN_SIZE:
      *value = c->green_size;
      return bool_success();
    case EGL_BLUE_SIZE:
      *value = c->blue_size;
      return bool_success();
    case EGL_ALPHA_SIZE:
      *value = c->alpha_size;
      return bool_success();
    case EGL_DEPTH_SIZE:
      *value = c->depth_size;
      return bool_success();
    case EGL_STENCIL_SIZE:
      *value = c->stencil_size;
      return bool_success();
    case EGL_SAMPLES:
      *value = c->samples;
      return bool_success();
    case EGL_SAMPLE_BUFFERS:
      *value = c->sample_buffers;
      return bool_success();
    case EGL_NATIVE_VISUAL_ID:
      *value = c->native_visual_id;
      return bool_success();
    case EGL_NATIVE_VISUAL_TYPE:
      *value = c->native_visual_type;
      return bool_success();
    case EGL_NATIVE_RENDERABLE:
      *value = c->native_renderable;
      return bool_success();
    case EGL_CONFIG_CAVEAT:
      *value = c->config_caveat;
      return bool_success();
    case EGL_BIND_TO_TEXTURE_RGB:
      *value = c->bind_to_texture_rgb;
      return bool_success();
    case EGL_BIND_TO_TEXTURE_RGBA:
      *value = c->bind_to_texture_rgba;
      return bool_success();
    case EGL_RENDERABLE_TYPE:
      *value = c->renderable_type;
      return bool_success();
    case EGL_CONFORMANT:
      *value = c->conformant;
      return bool_success();
    case EGL_SURFACE_TYPE:
      *value = c->surface_type;
      return bool_success();
    case EGL_MAX_PBUFFER_WIDTH:
      *value = c->max_pbuffer_width;
      return bool_success();
    case EGL_MAX_PBUFFER_HEIGHT:
      *value = c->max_pbuffer_height;
      return bool_success();
    case EGL_MAX_PBUFFER_PIXELS:
      *value = c->max_pbuffer_pixels;
      return bool_success();
    case EGL_MIN_SWAP_INTERVAL:
      *value = c->min_swap_interval;
      return bool_success();
    case EGL_MAX_SWAP_INTERVAL:
      *value = c->max_swap_interval;
      return bool_success();
    default:
      return bool_failure(EGL_BAD_ATTRIBUTE);
  }
}

EGLDisplay _eglGetDisplay(EGLNativeDisplayType display_id) {
  return &mock_display;
}

EGLDisplay _eglGetCurrentDisplay() {
  return mock != nullptr ? mock->eglGetCurrentDisplay() : &mock_display;
}

EGLDisplay _eglGetPlatformDisplayEXT(EGLenum platform,
                                     void* native_display,
                                     const EGLint* attrib_list) {
  return &mock_display;
}

EGLint _eglGetError() {
  EGLint error = mock_error;
  mock_error = EGL_SUCCESS;
  return error;
}

void (*_eglGetProcAddress(const char* procname))(void) {
  mock_error = EGL_SUCCESS;
  return mock != nullptr ? mock->eglGetProcAddress(procname) : nullptr;
}

EGLBoolean _eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
  if (!check_display(dpy)) {
    return EGL_FALSE;
  }

  if (!display_initialized) {
    mock_config.config_id = 1;
    mock_config.buffer_size = 32;
    mock_config.color_buffer_type = EGL_RGB_BUFFER;
    mock_config.transparent_type = EGL_NONE;
    mock_config.level = 1;
    mock_config.red_size = 8;
    mock_config.green_size = 8;
    mock_config.blue_size = 8;
    mock_config.alpha_size = 0;
    mock_config.depth_size = 0;
    mock_config.stencil_size = 0;
    mock_config.samples = 0;
    mock_config.sample_buffers = 0;
    mock_config.native_visual_id = 1;
    mock_config.native_visual_type = 0;
    mock_config.native_renderable = EGL_TRUE;
    mock_config.config_caveat = EGL_NONE;
    mock_config.bind_to_texture_rgb = EGL_TRUE;
    mock_config.bind_to_texture_rgba = EGL_FALSE;
    mock_config.renderable_type = EGL_OPENGL_ES2_BIT;
    mock_config.conformant = EGL_OPENGL_ES2_BIT;
    mock_config.surface_type = EGL_WINDOW_BIT | EGL_PBUFFER_BIT;
    mock_config.max_pbuffer_width = 1024;
    mock_config.max_pbuffer_height = 1024;
    mock_config.max_pbuffer_pixels = 1024 * 1024;
    mock_config.min_swap_interval = 0;
    mock_config.max_swap_interval = 1000;
    display_initialized = true;
  }

  if (major != nullptr) {
    *major = 1;
  }
  if (minor != nullptr) {
    *minor = 5;
  }

  return bool_success();
}

EGLBoolean _eglMakeCurrent(EGLDisplay dpy,
                           EGLSurface draw,
                           EGLSurface read,
                           EGLContext ctx) {
  if (!check_display(dpy) || !check_initialized(dpy)) {
    return EGL_FALSE;
  }

  return mock != nullptr ? mock->eglMakeCurrent(dpy, draw, read, ctx)
                         : bool_success();
}

EGLBoolean _eglTerminate(EGLDisplay dpy) {
  if (!check_display(dpy)) {
    return EGL_FALSE;
  }
  display_initialized = false;
  return bool_success();
}
EGLBoolean _eglQueryContext(EGLDisplay display,
                            EGLContext context,
                            EGLint attribute,
                            EGLint* value) {
  if (attribute == EGL_CONTEXT_CLIENT_TYPE) {
    *value = EGL_OPENGL_API;
    return EGL_TRUE;
  }
  return EGL_FALSE;
}

EGLBoolean _eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
  if (!check_display(dpy) || !check_initialized(dpy)) {
    return EGL_FALSE;
  }

  return bool_success();
}

EGLImage _eglCreateImage(EGLDisplay dpy,
                         EGLContext ctx,
                         EGLenum target,
                         EGLClientBuffer buffer,
                         const EGLAttrib* attrib_list) {
  return mock != nullptr
             ? mock->eglCreateImage(dpy, ctx, target, buffer, attrib_list)
             : reinterpret_cast<EGLImage>(&mock_image);
}

EGLImageKHR _eglCreateImageKHR(EGLDisplay dpy,
                               EGLContext ctx,
                               EGLenum target,
                               EGLClientBuffer buffer,
                               const EGLint* attrib_list) {
  return mock != nullptr
             ? mock->eglCreateImageKHR(dpy, ctx, target, buffer, attrib_list)
             : &mock_image;
}

EGLBoolean _eglDestroyImageKHR(EGLDisplay dpy, EGLImage image) {
  return mock != nullptr ? mock->eglDestroyImageKHR(dpy, image)
                         : bool_success();
}

EGLBoolean _eglDestroyImage(EGLDisplay dpy, EGLImage image) {
  return mock != nullptr ? mock->eglDestroyImage(dpy, image) : bool_success();
}

EGLSync _eglCreateSync(EGLDisplay dpy,
                       EGLenum type,
                       const EGLAttrib* attrib_list) {
  return mock != nullptr ? mock->eglCreateSync(dpy, type, attrib_list)
                         : reinterpret_cast<EGLSync>(&mock_sync);
}

EGLSyncKHR _eglCreateSyncKHR(EGLDisplay dpy,
                             EGLenum type,
                             const EGLint* attrib_list) {
  return mock != nullptr ? mock->eglCreateSyncKHR(dpy, type, attrib_list)
                         : reinterpret_cast<EGLSyncKHR>(&mock_sync);
}

EGLint _eglClientWaitSync(EGLDisplay dpy,
                          EGLSync sync,
                          EGLint flags,
                          EGLTime timeout) {
  return mock != nullptr ? mock->eglClientWaitSync(dpy, sync, flags, timeout)
                         : EGL_CONDITION_SATISFIED;
}

EGLint _eglClientWaitSyncKHR(EGLDisplay dpy,
                             EGLSyncKHR sync,
                             EGLint flags,
                             EGLTimeKHR timeout) {
  return mock != nullptr ? mock->eglClientWaitSyncKHR(dpy, sync, flags, timeout)
                         : EGL_CONDITION_SATISFIED_KHR;
}

EGLBoolean _eglWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags) {
  return mock != nullptr ? mock->eglWaitSync(dpy, sync, flags) : EGL_TRUE;
}

EGLint _eglWaitSyncKHR(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags) {
  return mock != nullptr ? mock->eglWaitSyncKHR(dpy, sync, flags) : EGL_TRUE;
}

EGLBoolean _eglDestroySync(EGLDisplay dpy, EGLSync sync) {
  return mock != nullptr ? mock->eglDestroySync(dpy, sync) : bool_success();
}

EGLBoolean _eglDestroySyncKHR(EGLDisplay dpy, EGLSyncKHR sync) {
  return mock != nullptr ? mock->eglDestroySyncKHR(dpy, sync) : bool_success();
}

static GLuint bound_texture_2d;
static GLuint bound_framebuffer;

static std::map<GLenum, GLuint> framebuffer_renderbuffers;

static GLboolean enable_blend = GL_FALSE;
static GLboolean enable_scissor_test = GL_FALSE;

static void _setEnable(GLenum cap, GLboolean value) {
  if (cap == GL_BLEND) {
    enable_blend = value;
  } else if (cap == GL_SCISSOR_TEST) {
    enable_scissor_test = value;
  }
}

void _glAttachShader(GLuint program, GLuint shader) {}

static void _glBindFramebuffer(GLenum target, GLuint framebuffer) {
  if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) {
    bound_framebuffer = framebuffer;
  }
}

static void _glBindRenderbuffer(GLenum target, GLuint framebuffer) {}

static void _glBindTexture(GLenum target, GLuint texture) {
  if (target == GL_TEXTURE_2D) {
    bound_texture_2d = texture;
  }
}

static void _glBlitFramebuffer(GLint srcX0,
                               GLint srcY0,
                               GLint srcX1,
                               GLint srcY1,
                               GLint dstX0,
                               GLint dstY0,
                               GLint dstX1,
                               GLint dstY1,
                               GLbitfield mask,
                               GLenum filter) {
  mock->glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1,
                          dstY1, mask, filter);
}

GLuint _glCreateProgram() {
  return 0;
}

void _glCompileShader(GLuint shader) {}

void _glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
  mock->glClearColor(r, g, b, a);
}

GLuint _glCreateShader(GLenum shaderType) {
  return 0;
}

void _glDeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
  if (mock) {
    mock->glDeleteFramebuffers(n, framebuffers);
  }
}

void _glDeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
  if (mock) {
    mock->glDeleteRenderbuffers(n, renderbuffers);
  }
}

void _glDeleteShader(GLuint shader) {}

void _glDeleteTextures(GLsizei n, const GLuint* textures) {
  if (mock) {
    mock->glDeleteTextures(n, textures);
  }
}

static void _glDisable(GLenum cap) {
  _setEnable(cap, GL_FALSE);
}

static void _glEnable(GLenum cap) {
  _setEnable(cap, GL_TRUE);
}

static void _glEGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image) {
  if (mock != nullptr) {
    mock->glEGLImageTargetTexture2DOES(target, image);
  }
}

static GLenum _glCheckFramebufferStatus(GLenum target) {
  return mock != nullptr ? mock->glCheckFramebufferStatus(target)
                         : GL_FRAMEBUFFER_COMPLETE;
}

static void _glFinish() {
  if (mock != nullptr) {
    mock->glFinish();
  }
}

static void _glFramebufferRenderbuffer(GLenum target,
                                       GLenum attachment,
                                       GLenum renderbuffertarget,
                                       GLuint renderbuffer) {
  framebuffer_renderbuffers[attachment] = renderbuffer;
}

static void _glFramebufferTexture2D(GLenum target,
                                    GLenum attachment,
                                    GLenum textarget,
                                    GLuint texture,
                                    GLint level) {}

static void _glGenTextures(GLsizei n, GLuint* textures) {
  for (GLsizei i = 0; i < n; i++) {
    textures[i] = 0;
  }
  if (mock) {
    mock->glGenTextures(n, textures);
  }
}

static void _glGenFramebuffers(GLsizei n, GLuint* framebuffers) {
  for (GLsizei i = 0; i < n; i++) {
    framebuffers[i] = 0;
  }
  if (mock) {
    mock->glGenFramebuffers(n, framebuffers);
  }
}

static void _glGenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
  for (GLsizei i = 0; i < n; i++) {
    renderbuffers[i] = 0;
  }
  if (mock) {
    mock->glGenRenderbuffers(n, renderbuffers);
  }
}

static void _glGetFramebufferAttachmentParameteriv(GLenum target,
                                                   GLenum attachment,
                                                   GLenum pname,
                                                   GLint* params) {
  if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) {
    auto it = framebuffer_renderbuffers.find(attachment);
    *params =
        (it != framebuffer_renderbuffers.end()) ? GL_RENDERBUFFER : GL_NONE;
  } else if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) {
    auto it = framebuffer_renderbuffers.find(attachment);
    *params = (it != framebuffer_renderbuffers.end()) ? it->second : 0;
  }
}

static void _glGetIntegerv(GLenum pname, GLint* data) {
  if (pname == GL_TEXTURE_BINDING_2D) {
    *data = bound_texture_2d;
  } else if (pname == GL_FRAMEBUFFER_BINDING ||
             pname == GL_DRAW_FRAMEBUFFER_BINDING) {
    *data = bound_framebuffer;
  }
}

static void _glGetProgramiv(GLuint program, GLenum pname, GLint* params) {
  if (pname == GL_LINK_STATUS) {
    *params = GL_TRUE;
  }
}

static void _glGetProgramInfoLog(GLuint program,
                                 GLsizei maxLength,
                                 GLsizei* length,
                                 GLchar* infoLog) {}

static void _glGetShaderiv(GLuint shader, GLenum pname, GLint* params) {
  if (pname == GL_COMPILE_STATUS) {
    *params = GL_TRUE;
  }
}

static void _glGetShaderInfoLog(GLuint shader,
                                GLsizei maxLength,
                                GLsizei* length,
                                GLchar* infoLog) {}

static const GLubyte* _glGetString(GLenum pname) {
  if (mock != nullptr) {
    return mock->glGetString(pname);
  }
  return pname == GL_VERSION ? reinterpret_cast<const GLubyte*>("OpenGL ES 2.0")
                             : nullptr;
}

static void _glUniform2f(GLint location, GLfloat value0, GLfloat value1) {
  mock->glUniform2f(location, value0, value1);
}

static GLboolean _glIsEnabled(GLenum cap) {
  if (cap == GL_BLEND) {
    return enable_blend;
  } else if (cap == GL_SCISSOR_TEST) {
    return enable_scissor_test;
  } else {
    return GL_FALSE;
  }
}

static void _glTexParameterf(GLenum target, GLenum pname, GLfloat param) {}

static void _glTexParameteri(GLenum target, GLenum pname, GLint param) {}

static void _glTexImage2D(GLenum target,
                          GLint level,
                          GLint internalformat,
                          GLsizei width,
                          GLsizei height,
                          GLint border,
                          GLenum format,
                          GLenum type,
                          const void* pixels) {
  if (pixels != nullptr) {
    FML_CHECK(internalformat == GL_RGBA || internalformat == GL_RGBA8);
    FML_CHECK(format == GL_RGBA);
    FML_CHECK(type == GL_UNSIGNED_BYTE);

    // Simple mock read to detect out-of-bounds reads in tests.
    size_t size = width * height * 4;
    std::vector<uint8_t> temp(size);
    memcpy(temp.data(), pixels, size);
  }
}

static GLenum _glGetError() {
  return mock != nullptr ? mock->glGetError() : GL_NO_ERROR;
}

void _glLinkProgram(GLuint program) {}

void _glRenderbufferStorage(GLenum target,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height) {}

void _glShaderSource(GLuint shader,
                     GLsizei count,
                     const GLchar* const* string,
                     const GLint* length) {}

bool epoxy_has_gl_extension(const char* extension) {
  return mock != nullptr ? mock->epoxy_has_gl_extension(extension) : false;
}

bool epoxy_has_egl_extension(EGLDisplay display, const char* extension) {
  return mock != nullptr ? mock->epoxy_has_egl_extension(display, extension)
                         : false;
}

int epoxy_egl_version(EGLDisplay display) {
  return mock != nullptr ? mock->epoxy_egl_version(display) : 15;
}

bool epoxy_is_desktop_gl(void) {
  return mock != nullptr ? mock->epoxy_is_desktop_gl() : false;
}

int epoxy_gl_version(void) {
  return mock != nullptr ? mock->epoxy_gl_version() : 0;
}

#ifdef __GNUC__
#define CONSTRUCT(_func) static void _func(void) __attribute__((constructor));
#define DESTRUCT(_func) static void _func(void) __attribute__((destructor));
#elif defined(_MSC_VER) && (_MSC_VER >= 1500)
#define CONSTRUCT(_func)                                                   \
  static void _func(void);                                                 \
  static int _func##_wrapper(void) {                                       \
    _func();                                                               \
    return 0;                                                              \
  }                                                                        \
  __pragma(section(".CRT$XCU", read))                                      \
      __declspec(allocate(".CRT$XCU")) static int (*_array##_func)(void) = \
          _func##_wrapper;

#else
#error "You will need constructor support for your compiler"
#endif

CONSTRUCT(library_init)

EGLBoolean (*epoxy_eglBindAPI)(EGLenum api);
EGLBoolean (*epoxy_eglChooseConfig)(EGLDisplay dpy,
                                    const EGLint* attrib_list,
                                    EGLConfig* configs,
                                    EGLint config_size,
                                    EGLint* num_config);
EGLContext (*epoxy_eglCreateContext)(EGLDisplay dpy,
                                     EGLConfig config,
                                     EGLContext share_context,
                                     const EGLint* attrib_list);
EGLBoolean (*epoxy_eglDestroyContext)(EGLDisplay dpy, EGLContext context);
EGLBoolean (*epoxy_eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
EGLSurface (*epoxy_eglCreatePbufferSurface)(EGLDisplay dpy,
                                            EGLConfig config,
                                            const EGLint* attrib_list);
EGLSurface (*epoxy_eglCreateWindowSurface)(EGLDisplay dpy,
                                           EGLConfig config,
                                           EGLNativeWindowType win,
                                           const EGLint* attrib_list);
EGLBoolean (*epoxy_eglGetConfigAttrib)(EGLDisplay dpy,
                                       EGLConfig config,
                                       EGLint attribute,
                                       EGLint* value);
EGLDisplay (*epoxy_eglGetDisplay)(EGLNativeDisplayType display_id);
EGLDisplay (*epoxy_eglGetPlatformDisplayEXT)(EGLenum platform,
                                             void* native_display,
                                             const EGLint* attrib_list);
EGLint (*epoxy_eglGetError)();
void (*(*epoxy_eglGetProcAddress)(const char* procname))(void);
EGLBoolean (*epoxy_eglInitialize)(EGLDisplay dpy, EGLint* major, EGLint* minor);
EGLBoolean (*epoxy_eglMakeCurrent)(EGLDisplay dpy,
                                   EGLSurface draw,
                                   EGLSurface read,
                                   EGLContext ctx);
EGLBoolean (*epoxy_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
EGLBoolean (*epoxy_eglTerminate)(EGLDisplay dpy);
EGLImage (*epoxy_eglCreateImage)(EGLDisplay dpy,
                                 EGLContext ctx,
                                 EGLenum target,
                                 EGLClientBuffer buffer,
                                 const EGLAttrib* attrib_list);
EGLImageKHR (*epoxy_eglCreateImageKHR)(EGLDisplay dpy,
                                       EGLContext ctx,
                                       EGLenum target,
                                       EGLClientBuffer buffer,
                                       const EGLint* attrib_list);
EGLBoolean (*epoxy_eglDestroyImage)(EGLDisplay dpy, EGLImage image);
EGLBoolean (*epoxy_eglDestroyImageKHR)(EGLDisplay dpy, EGLImage image);
EGLSync (*epoxy_eglCreateSync)(EGLDisplay dpy,
                               EGLenum type,
                               const EGLAttrib* attrib_list);
EGLSyncKHR (*epoxy_eglCreateSyncKHR)(EGLDisplay dpy,
                                     EGLenum type,
                                     const EGLint* attrib_list);
EGLint (*epoxy_eglClientWaitSync)(EGLDisplay dpy,
                                  EGLSync sync,
                                  EGLint flags,
                                  EGLTime timeout);
EGLint (*epoxy_eglClientWaitSyncKHR)(EGLDisplay dpy,
                                     EGLSyncKHR sync,
                                     EGLint flags,
                                     EGLTimeKHR timeout);
EGLBoolean (*epoxy_eglWaitSync)(EGLDisplay dpy, EGLSync sync, EGLint flags);
EGLint (*epoxy_eglWaitSyncKHR)(EGLDisplay dpy, EGLSyncKHR sync, EGLint flags);
EGLBoolean (*epoxy_eglDestroySync)(EGLDisplay dpy, EGLSync sync);
EGLBoolean (*epoxy_eglDestroySyncKHR)(EGLDisplay dpy, EGLSyncKHR sync);

void (*epoxy_glAttachShader)(GLuint program, GLuint shader);
void (*epoxy_glBindFramebuffer)(GLenum target, GLuint framebuffer);
void (*epoxy_glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
void (*epoxy_glBindTexture)(GLenum target, GLuint texture);
void (*epoxy_glBlitFramebuffer)(GLint srcX0,
                                GLint srcY0,
                                GLint srcX1,
                                GLint srcY1,
                                GLint dstX0,
                                GLint dstY0,
                                GLint dstX1,
                                GLint dstY1,
                                GLbitfield mask,
                                GLenum filter);
void (*epoxy_glCompileShader)(GLuint shader);
GLuint (*epoxy_glCreateProgram)();
GLuint (*epoxy_glCreateShader)(GLenum shaderType);
void (*epoxy_glDeleteFramebuffers)(GLsizei n, const GLuint* framebuffers);
void (*expoxy_glDeleteShader)(GLuint shader);
void (*epoxy_glDeleteTextures)(GLsizei n, const GLuint* textures);
void (*epoxy_glEGLImageTargetTexture2DOES)(GLenum target, GLeglImageOES image);
GLenum (*epoxy_glCheckFramebufferStatus)(GLenum target);
void (*epoxy_glFinish)();
void (*epoxy_glFramebufferRenderbuffer)(GLenum target,
                                        GLenum attachment,
                                        GLenum renderbuffertarget,
                                        GLuint renderbuffer);
void (*epoxy_glFramebufferTexture2D)(GLenum target,
                                     GLenum attachment,
                                     GLenum textarget,
                                     GLuint texture,
                                     GLint level);
void (*epoxy_glGetFramebufferAttachmentParameteriv)(GLenum target,
                                                    GLenum attachment,
                                                    GLenum pname,
                                                    GLint* params);
void (*epoxy_glGenFramebuffers)(GLsizei n, GLuint* framebuffers);
void (*epoxy_glGenTextures)(GLsizei n, GLuint* textures);
void (*epoxy_glLinkProgram)(GLuint program);
void (*epoxy_glRenderbufferStorage)(GLenum target,
                                    GLenum internalformat,
                                    GLsizei width,
                                    GLsizei height);
void (*epoxy_glShaderSource)(GLuint shader,
                             GLsizei count,
                             const GLchar* const* string,
                             const GLint* length);
void (*epoxy_glTexParameterf)(GLenum target, GLenum pname, GLfloat param);
void (*epoxy_glTexParameteri)(GLenum target, GLenum pname, GLint param);
void (*epoxy_glTexImage2D)(GLenum target,
                           GLint level,
                           GLint internalformat,
                           GLsizei width,
                           GLsizei height,
                           GLint border,
                           GLenum format,
                           GLenum type,
                           const void* pixels);
void (*epoxy_glUniform2f)(GLint location, GLfloat value0, GLfloat value1);
GLenum (*epoxy_glGetError)();

static void library_init() {
  epoxy_eglBindAPI = _eglBindAPI;
  epoxy_eglChooseConfig = _eglChooseConfig;
  epoxy_eglCreateContext = _eglCreateContext;
  epoxy_eglDestroyContext = _eglDestroyContext;
  epoxy_eglDestroySurface = _eglDestroySurface;
  epoxy_eglGetCurrentContext = _eglGetCurrentContext;
  epoxy_eglCreatePbufferSurface = _eglCreatePbufferSurface;
  epoxy_eglCreateWindowSurface = _eglCreateWindowSurface;
  epoxy_eglGetConfigAttrib = _eglGetConfigAttrib;
  epoxy_eglGetDisplay = _eglGetDisplay;
  epoxy_eglGetCurrentDisplay = _eglGetCurrentDisplay;
  epoxy_eglGetPlatformDisplayEXT = _eglGetPlatformDisplayEXT;
  epoxy_eglGetError = _eglGetError;
  epoxy_eglGetProcAddress = _eglGetProcAddress;
  epoxy_eglInitialize = _eglInitialize;
  epoxy_eglMakeCurrent = _eglMakeCurrent;
  epoxy_eglQueryContext = _eglQueryContext;
  epoxy_eglSwapBuffers = _eglSwapBuffers;
  epoxy_eglTerminate = _eglTerminate;
  epoxy_eglCreateImage = _eglCreateImage;
  epoxy_eglCreateImageKHR = _eglCreateImageKHR;
  epoxy_eglDestroyImage = _eglDestroyImage;
  epoxy_eglDestroyImageKHR = _eglDestroyImageKHR;
  epoxy_eglCreateSync = _eglCreateSync;
  epoxy_eglCreateSyncKHR = _eglCreateSyncKHR;
  epoxy_eglClientWaitSync = _eglClientWaitSync;
  epoxy_eglClientWaitSyncKHR = _eglClientWaitSyncKHR;
  epoxy_eglWaitSync = _eglWaitSync;
  epoxy_eglWaitSyncKHR = _eglWaitSyncKHR;
  epoxy_eglDestroySync = _eglDestroySync;
  epoxy_eglDestroySyncKHR = _eglDestroySyncKHR;

  epoxy_glAttachShader = _glAttachShader;
  epoxy_glBindFramebuffer = _glBindFramebuffer;
  epoxy_glBindRenderbuffer = _glBindRenderbuffer;
  epoxy_glBindTexture = _glBindTexture;
  epoxy_glBlitFramebuffer = _glBlitFramebuffer;
  epoxy_glCompileShader = _glCompileShader;
  epoxy_glClearColor = _glClearColor;
  epoxy_glCreateProgram = _glCreateProgram;
  epoxy_glCreateShader = _glCreateShader;
  epoxy_glDeleteFramebuffers = _glDeleteFramebuffers;
  epoxy_glDeleteRenderbuffers = _glDeleteRenderbuffers;
  epoxy_glDeleteShader = _glDeleteShader;
  epoxy_glDeleteTextures = _glDeleteTextures;
  epoxy_glEGLImageTargetTexture2DOES = _glEGLImageTargetTexture2DOES;
  epoxy_glCheckFramebufferStatus = _glCheckFramebufferStatus;
  epoxy_glFinish = _glFinish;
  epoxy_glDisable = _glDisable;
  epoxy_glEnable = _glEnable;
  epoxy_glFramebufferRenderbuffer = _glFramebufferRenderbuffer;
  epoxy_glFramebufferTexture2D = _glFramebufferTexture2D;
  epoxy_glGenFramebuffers = _glGenFramebuffers;
  epoxy_glGenRenderbuffers = _glGenRenderbuffers;
  epoxy_glGenTextures = _glGenTextures;
  epoxy_glGetFramebufferAttachmentParameteriv =
      _glGetFramebufferAttachmentParameteriv;
  epoxy_glGetIntegerv = _glGetIntegerv;
  epoxy_glGetProgramiv = _glGetProgramiv;
  epoxy_glGetProgramInfoLog = _glGetProgramInfoLog;
  epoxy_glGetShaderiv = _glGetShaderiv;
  epoxy_glGetShaderInfoLog = _glGetShaderInfoLog;
  epoxy_glGetString = _glGetString;
  epoxy_glIsEnabled = _glIsEnabled;
  epoxy_glLinkProgram = _glLinkProgram;
  epoxy_glRenderbufferStorage = _glRenderbufferStorage;
  epoxy_glShaderSource = _glShaderSource;
  epoxy_glTexParameterf = _glTexParameterf;
  epoxy_glTexParameteri = _glTexParameteri;
  epoxy_glTexImage2D = _glTexImage2D;
  epoxy_glUniform2f = _glUniform2f;
  epoxy_glGetError = _glGetError;
}

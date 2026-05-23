// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/linux/fl_opengl_driver.h"

#include <cstring>

#include <epoxy/egl.h>
#include <epoxy/gl.h>

static gboolean gl_string_contains(GLenum name, const char* needle) {
  const char* value = reinterpret_cast<const char*>(glGetString(name));
  return value != nullptr && std::strstr(value, needle) != nullptr;
}

static FlOpenGLDriver detect_driver() {
  if (gl_string_contains(GL_VENDOR, "NVIDIA")) {
    return kFlOpenGLDriverNvidia;
  }
  if (gl_string_contains(GL_VENDOR, "Vivante Corporation")) {
    return kFlOpenGLDriverVivante;
  }
  if (gl_string_contains(GL_RENDERER, "LoongGPU")) {
    return kFlOpenGLDriverLoongGPU;
  }
  return kFlOpenGLDriverUnknown;
}

static gboolean has_framebuffer_blit_api() {
  return epoxy_gl_version() >= 30 ||
         epoxy_has_gl_extension("GL_EXT_framebuffer_blit");
}

static gboolean has_resolvable_framebuffer_blit_proc(FlOpenGLDriver driver) {
  if (epoxy_glBlitFramebuffer == nullptr) {
    return FALSE;
  }

  if (driver != kFlOpenGLDriverLoongGPU) {
    return TRUE;
  }

  // LoongGPU exposes both GLX desktop and EGL ES contexts on UOS25. Flutter's
  // Linux embedder uses EGL ES, where libepoxy must have a provider for
  // glBlitFramebuffer before this compositor can safely call it.
  return eglGetProcAddress("glBlitFramebuffer") != nullptr ||
         eglGetProcAddress("glBlitFramebufferEXT") != nullptr ||
         eglGetProcAddress("glBlitFramebufferANGLE") != nullptr;
}

FlOpenGLDriverCapabilities fl_opengl_driver_get_capabilities() {
  FlOpenGLDriverCapabilities capabilities = {};
  capabilities.driver = detect_driver();

  // NVIDIA and Vivante are temporarily disabled due to
  // https://github.com/flutter/flutter/issues/152099.
  if (capabilities.driver == kFlOpenGLDriverNvidia ||
      capabilities.driver == kFlOpenGLDriverVivante) {
    capabilities.supports_framebuffer_blit = FALSE;
    return capabilities;
  }

  capabilities.supports_framebuffer_blit =
      has_framebuffer_blit_api() &&
      has_resolvable_framebuffer_blit_proc(capabilities.driver);
  return capabilities;
}

// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_LINUX_FL_OPENGL_DRIVER_H_
#define FLUTTER_SHELL_PLATFORM_LINUX_FL_OPENGL_DRIVER_H_

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
  kFlOpenGLDriverUnknown,
  kFlOpenGLDriverNvidia,
  kFlOpenGLDriverVivante,
  kFlOpenGLDriverLoongGPU,
} FlOpenGLDriver;

typedef struct {
  FlOpenGLDriver driver;
  gboolean supports_framebuffer_blit;
} FlOpenGLDriverCapabilities;

// Detects the active OpenGL driver and the features that Flutter's Linux shell
// can safely use with the current context.
FlOpenGLDriverCapabilities fl_opengl_driver_get_capabilities();

G_END_DECLS

#endif  // FLUTTER_SHELL_PLATFORM_LINUX_FL_OPENGL_DRIVER_H_

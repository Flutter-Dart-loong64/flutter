// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <epoxy/egl.h>
#include <epoxy/gl.h>
#include <gdk/gdkwayland.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "flutter/shell/platform/linux/fl_opengl_manager.h"

struct _FlOpenGLManager {
  GObject parent_instance;

  // Display being rendered to.
  EGLDisplay display = EGL_NO_DISPLAY;

  // Context used by the Flutter engine for rendering.
  EGLContext render_context = EGL_NO_CONTEXT;

  // Context used by the Flutter engine to share resources.
  EGLContext resource_context = EGL_NO_CONTEXT;

  // Context used by platform thread.
  EGLContext platform_context = EGL_NO_CONTEXT;

  // Per-context fallback surfaces for drivers without surfaceless contexts.
  EGLSurface render_surface = EGL_NO_SURFACE;
  EGLSurface resource_surface = EGL_NO_SURFACE;
  EGLSurface platform_surface = EGL_NO_SURFACE;

  // TRUE when contexts can be made current without a surface.
  gboolean surfaceless = FALSE;

  // TRUE when the display, config, contexts, and any fallback surfaces exist.
  gboolean initialized = FALSE;

  // TRUE when the EGL context can export textures as EGL images.
  gboolean supports_egl_image = FALSE;
};

G_DEFINE_TYPE(FlOpenGLManager, fl_opengl_manager, G_TYPE_OBJECT)

static void fl_opengl_manager_dispose(GObject* object) {
  FlOpenGLManager* self = FL_OPENGL_MANAGER(object);

  if (self->display != EGL_NO_DISPLAY) {
    eglMakeCurrent(self->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);

    if (self->render_surface != EGL_NO_SURFACE) {
      eglDestroySurface(self->display, self->render_surface);
    }
    if (self->resource_surface != EGL_NO_SURFACE) {
      eglDestroySurface(self->display, self->resource_surface);
    }
    if (self->platform_surface != EGL_NO_SURFACE) {
      eglDestroySurface(self->display, self->platform_surface);
    }

    if (self->platform_context != EGL_NO_CONTEXT) {
      eglDestroyContext(self->display, self->platform_context);
    }
    if (self->resource_context != EGL_NO_CONTEXT) {
      eglDestroyContext(self->display, self->resource_context);
    }
    if (self->render_context != EGL_NO_CONTEXT) {
      eglDestroyContext(self->display, self->render_context);
    }

    eglTerminate(self->display);
  }

  self->display = EGL_NO_DISPLAY;
  self->render_context = EGL_NO_CONTEXT;
  self->resource_context = EGL_NO_CONTEXT;
  self->platform_context = EGL_NO_CONTEXT;
  self->render_surface = EGL_NO_SURFACE;
  self->resource_surface = EGL_NO_SURFACE;
  self->platform_surface = EGL_NO_SURFACE;
  self->initialized = FALSE;
  self->supports_egl_image = FALSE;

  G_OBJECT_CLASS(fl_opengl_manager_parent_class)->dispose(object);
}

static void fl_opengl_manager_class_init(FlOpenGLManagerClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = fl_opengl_manager_dispose;
}

static void log_egl_error(const gchar* operation) {
  g_warning("%s: EGL error 0x%04x", operation, eglGetError());
}

static EGLSurface create_fallback_surface(FlOpenGLManager* self,
                                          EGLConfig config) {
  const EGLint surface_attributes[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  return eglCreatePbufferSurface(self->display, config, surface_attributes);
}

static gboolean choose_config(FlOpenGLManager* self,
                              EGLConfig* config,
                              gboolean* supports_pbuffer) {
  const EGLint full_pbuffer_attributes[] = {EGL_SURFACE_TYPE,
                                            EGL_PBUFFER_BIT,
                                            EGL_RENDERABLE_TYPE,
                                            EGL_OPENGL_ES2_BIT,
                                            EGL_RED_SIZE,
                                            8,
                                            EGL_GREEN_SIZE,
                                            8,
                                            EGL_BLUE_SIZE,
                                            8,
                                            EGL_ALPHA_SIZE,
                                            8,
                                            EGL_DEPTH_SIZE,
                                            8,
                                            EGL_STENCIL_SIZE,
                                            8,
                                            EGL_NONE};
  const EGLint rgba_pbuffer_attributes[] = {EGL_SURFACE_TYPE,
                                            EGL_PBUFFER_BIT,
                                            EGL_RENDERABLE_TYPE,
                                            EGL_OPENGL_ES2_BIT,
                                            EGL_RED_SIZE,
                                            8,
                                            EGL_GREEN_SIZE,
                                            8,
                                            EGL_BLUE_SIZE,
                                            8,
                                            EGL_ALPHA_SIZE,
                                            8,
                                            EGL_NONE};
  const EGLint rgb_pbuffer_attributes[] = {EGL_SURFACE_TYPE,
                                           EGL_PBUFFER_BIT,
                                           EGL_RENDERABLE_TYPE,
                                           EGL_OPENGL_ES2_BIT,
                                           EGL_RED_SIZE,
                                           8,
                                           EGL_GREEN_SIZE,
                                           8,
                                           EGL_BLUE_SIZE,
                                           8,
                                           EGL_NONE};
  const EGLint pbuffer_attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                       EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                                       EGL_NONE};
  const EGLint full_surfaceless_attributes[] = {EGL_RENDERABLE_TYPE,
                                                EGL_OPENGL_ES2_BIT,
                                                EGL_RED_SIZE,
                                                8,
                                                EGL_GREEN_SIZE,
                                                8,
                                                EGL_BLUE_SIZE,
                                                8,
                                                EGL_ALPHA_SIZE,
                                                8,
                                                EGL_DEPTH_SIZE,
                                                8,
                                                EGL_STENCIL_SIZE,
                                                8,
                                                EGL_NONE};
  const EGLint rgba_surfaceless_attributes[] = {EGL_RENDERABLE_TYPE,
                                                EGL_OPENGL_ES2_BIT,
                                                EGL_RED_SIZE,
                                                8,
                                                EGL_GREEN_SIZE,
                                                8,
                                                EGL_BLUE_SIZE,
                                                8,
                                                EGL_ALPHA_SIZE,
                                                8,
                                                EGL_NONE};
  const EGLint rgb_surfaceless_attributes[] = {EGL_RENDERABLE_TYPE,
                                               EGL_OPENGL_ES2_BIT,
                                               EGL_RED_SIZE,
                                               8,
                                               EGL_GREEN_SIZE,
                                               8,
                                               EGL_BLUE_SIZE,
                                               8,
                                               EGL_NONE};
  const EGLint surfaceless_attributes[] = {EGL_RENDERABLE_TYPE,
                                           EGL_OPENGL_ES2_BIT, EGL_NONE};
  struct ConfigCandidate {
    const EGLint* attributes;
    gboolean supports_pbuffer;
  };
  const ConfigCandidate candidates[] = {
      {full_pbuffer_attributes, TRUE},
      {rgba_pbuffer_attributes, TRUE},
      {rgb_pbuffer_attributes, TRUE},
      {pbuffer_attributes, TRUE},
      {full_surfaceless_attributes, FALSE},
      {rgba_surfaceless_attributes, FALSE},
      {rgb_surfaceless_attributes, FALSE},
      {surfaceless_attributes, FALSE},
  };

  for (const ConfigCandidate& candidate : candidates) {
    EGLint num_configs = 0;
    if (eglChooseConfig(self->display, candidate.attributes, config, 1,
                        &num_configs) == EGL_TRUE &&
        num_configs > 0) {
      *supports_pbuffer = candidate.supports_pbuffer;
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean make_current_unchecked(FlOpenGLManager* self,
                                       EGLContext context,
                                       EGLSurface surface) {
  EGLSurface current_surface = self->surfaceless ? EGL_NO_SURFACE : surface;
  return eglMakeCurrent(self->display, current_surface, current_surface,
                        context) == EGL_TRUE;
}

static gboolean validate_context(FlOpenGLManager* self,
                                 EGLContext context,
                                 EGLSurface surface,
                                 const gchar* name) {
  if (!make_current_unchecked(self, context, surface)) {
    g_autofree gchar* operation =
        g_strdup_printf("Failed to make the %s EGL context current", name);
    log_egl_error(operation);
    return FALSE;
  }

  if (glGetString(GL_VERSION) == nullptr) {
    g_warning("Failed to query the OpenGL ES version for the %s EGL context",
              name);
    eglMakeCurrent(self->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
    return FALSE;
  }

  if (eglMakeCurrent(self->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                     EGL_NO_CONTEXT) != EGL_TRUE) {
    g_autofree gchar* operation =
        g_strdup_printf("Failed to clear the %s EGL context", name);
    log_egl_error(operation);
    return FALSE;
  }

  return TRUE;
}

static gboolean make_current(FlOpenGLManager* self,
                             EGLContext context,
                             EGLSurface surface) {
  g_return_val_if_fail(FL_IS_OPENGL_MANAGER(self), FALSE);

  if (!self->initialized || context == EGL_NO_CONTEXT) {
    return FALSE;
  }

  return make_current_unchecked(self, context, surface);
}

static void fl_opengl_manager_init(FlOpenGLManager* self) {
  GdkDisplay* display = gdk_display_get_default();
  EGLenum platform = 0;
  EGLNativeDisplayType native_display = EGL_DEFAULT_DISPLAY;
  if (GDK_IS_WAYLAND_DISPLAY(display)) {
    platform = EGL_PLATFORM_WAYLAND_EXT;
    native_display = reinterpret_cast<EGLNativeDisplayType>(
        gdk_wayland_display_get_wl_display(display));
#ifdef GDK_WINDOWING_X11
  } else if (GDK_IS_X11_DISPLAY(display)) {
    platform = EGL_PLATFORM_X11_EXT;
    native_display = reinterpret_cast<EGLNativeDisplayType>(
        gdk_x11_display_get_xdisplay(display));
#endif
  } else {
    g_critical("Unsupported GDK backend, unable to get EGL display");
    return;
  }

  if (epoxy_eglGetPlatformDisplayEXT != nullptr) {
    self->display = eglGetPlatformDisplayEXT(platform, native_display, nullptr);
  }
  if (self->display == EGL_NO_DISPLAY) {
    self->display = eglGetDisplay(native_display);
  }

  if (self->display == EGL_NO_DISPLAY) {
    log_egl_error("Failed to get EGL display");
    return;
  }

  EGLint major = 0;
  EGLint minor = 0;
  if (eglInitialize(self->display, &major, &minor) != EGL_TRUE) {
    log_egl_error("Failed to initialize EGL display");
    return;
  }

  if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE) {
    log_egl_error("Failed to bind the OpenGL ES API");
    return;
  }

  EGLConfig config = nullptr;
  gboolean config_supports_pbuffer = FALSE;
  if (!choose_config(self, &config, &config_supports_pbuffer)) {
    log_egl_error("Failed to choose an OpenGL ES 2 EGL config");
    return;
  }

  const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};

  self->render_context = eglCreateContext(self->display, config, EGL_NO_CONTEXT,
                                          context_attributes);
  if (self->render_context == EGL_NO_CONTEXT) {
    log_egl_error("Failed to create EGL context for rendering");
    return;
  }

  self->resource_context = eglCreateContext(
      self->display, config, self->render_context, context_attributes);
  if (self->resource_context == EGL_NO_CONTEXT) {
    log_egl_error("Failed to create EGL context for resource sharing");
    return;
  }

  self->platform_context = eglCreateContext(
      self->display, config, self->render_context, context_attributes);
  if (self->platform_context == EGL_NO_CONTEXT) {
    log_egl_error("Failed to create EGL context for platform thread");
    return;
  }

  // Prefer surfaceless contexts. If the driver rejects them, use an isolated
  // 1x1 pbuffer for each context so resource and raster threads never compete
  // for ownership of the same EGLSurface.
  if (eglMakeCurrent(self->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                     self->platform_context) == EGL_TRUE) {
    self->surfaceless = TRUE;
    eglMakeCurrent(self->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                   EGL_NO_CONTEXT);
  } else {
    EGLint surfaceless_error = eglGetError();
    if (!config_supports_pbuffer) {
      g_warning(
          "The EGL driver rejected surfaceless contexts and has no pbuffer "
          "config: EGL error 0x%04x",
          surfaceless_error);
      return;
    }
    self->render_surface = create_fallback_surface(self, config);
    self->resource_surface = create_fallback_surface(self, config);
    self->platform_surface = create_fallback_surface(self, config);
    if (self->render_surface == EGL_NO_SURFACE ||
        self->resource_surface == EGL_NO_SURFACE ||
        self->platform_surface == EGL_NO_SURFACE) {
      log_egl_error("Failed to create EGL pbuffer fallback surfaces");
      return;
    }
  }

  if (!validate_context(self, self->render_context, self->render_surface,
                        "render") ||
      !validate_context(self, self->resource_context, self->resource_surface,
                        "resource") ||
      !validate_context(self, self->platform_context, self->platform_surface,
                        "platform")) {
    return;
  }

  if (make_current_unchecked(self, self->platform_context,
                             self->platform_surface)) {
    gboolean has_egl_image =
        (major > 1 || (major == 1 && minor >= 5) ||
         epoxy_has_egl_extension(self->display, "EGL_KHR_image_base")) &&
        epoxy_eglCreateImageKHR != nullptr &&
        epoxy_eglDestroyImageKHR != nullptr;
    self->supports_egl_image = has_egl_image &&
                               epoxy_has_gl_extension("GL_OES_EGL_image") &&
                               epoxy_glEGLImageTargetTexture2DOES != nullptr;
    if (eglMakeCurrent(self->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT) != EGL_TRUE) {
      log_egl_error("Failed to clear the EGL platform context");
      self->supports_egl_image = FALSE;
      return;
    }
  } else {
    log_egl_error("Failed to restore the EGL platform context");
    return;
  }

  self->initialized = TRUE;
}

FlOpenGLManager* fl_opengl_manager_new() {
  FlOpenGLManager* self =
      FL_OPENGL_MANAGER(g_object_new(fl_opengl_manager_get_type(), nullptr));
  return self;
}

gboolean fl_opengl_manager_is_valid(FlOpenGLManager* self) {
  g_return_val_if_fail(FL_IS_OPENGL_MANAGER(self), FALSE);
  return self->initialized;
}

gboolean fl_opengl_manager_supports_egl_image(FlOpenGLManager* self) {
  g_return_val_if_fail(FL_IS_OPENGL_MANAGER(self), FALSE);
  return self->initialized && self->supports_egl_image;
}

gboolean fl_opengl_manager_make_current(FlOpenGLManager* self) {
  return make_current(self, self->render_context, self->render_surface);
}

gboolean fl_opengl_manager_make_resource_current(FlOpenGLManager* self) {
  return make_current(self, self->resource_context, self->resource_surface);
}

gboolean fl_opengl_manager_make_platform_current(FlOpenGLManager* self) {
  return make_current(self, self->platform_context, self->platform_surface);
}

gboolean fl_opengl_manager_clear_current(FlOpenGLManager* self) {
  g_return_val_if_fail(FL_IS_OPENGL_MANAGER(self), FALSE);
  if (!self->initialized) {
    return FALSE;
  }
  return eglMakeCurrent(self->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                        EGL_NO_CONTEXT) == EGL_TRUE;
}

// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fl_egl_image.h"

#include <epoxy/egl.h>
#include <epoxy/gl.h>

struct _FlEGLImage {
  GObject parent_instance;

  EGLDisplay display;

  EGLImage image;
};

static EGLImage create_egl_image(EGLDisplay display, GLuint texture_id) {
  if (display == EGL_NO_DISPLAY) {
    g_warning("Failed to create EGL image: Failed to get current EGL display");
    return EGL_NO_IMAGE_KHR;
  }

  EGLContext egl_context = eglGetCurrentContext();
  if (egl_context == EGL_NO_CONTEXT) {
    g_warning("Failed to create EGL image: Failed to get current EGL context");
    return EGL_NO_IMAGE_KHR;
  }

  EGLImage image = eglCreateImageKHR(
      display, egl_context, EGL_GL_TEXTURE_2D,
      reinterpret_cast<EGLClientBuffer>(static_cast<intptr_t>(texture_id)),
      nullptr);
  if (image == EGL_NO_IMAGE_KHR) {
    g_warning("Failed to create EGL image: EGL error 0x%04x", eglGetError());
  }
  return image;
}

G_DEFINE_TYPE(FlEGLImage, fl_egl_image, G_TYPE_OBJECT)

static void fl_egl_image_dispose(GObject* object) {
  FlEGLImage* self = FL_EGL_IMAGE(object);

  if (self->display != EGL_NO_DISPLAY && self->image != EGL_NO_IMAGE_KHR) {
    if (eglDestroyImageKHR(self->display, self->image) != EGL_TRUE) {
      g_warning("Failed to destroy EGL image: EGL error 0x%04x", eglGetError());
    }
  }
  self->display = EGL_NO_DISPLAY;
  self->image = EGL_NO_IMAGE_KHR;

  G_OBJECT_CLASS(fl_egl_image_parent_class)->dispose(object);
}

static void fl_egl_image_class_init(FlEGLImageClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = fl_egl_image_dispose;
}

static void fl_egl_image_init(FlEGLImage* self) {}

FlEGLImage* fl_egl_image_new(GLuint texture) {
  FlEGLImage* self =
      FL_EGL_IMAGE(g_object_new(fl_egl_image_get_type(), nullptr));

  self->display = eglGetCurrentDisplay();
  self->image = create_egl_image(self->display, texture);
  if (self->image == EGL_NO_IMAGE_KHR) {
    g_object_unref(self);
    return nullptr;
  }

  return self;
}

EGLImage fl_egl_image_get_image(FlEGLImage* image) {
  g_return_val_if_fail(FL_IS_EGL_IMAGE(image), EGL_NO_IMAGE_KHR);
  return image->image;
}

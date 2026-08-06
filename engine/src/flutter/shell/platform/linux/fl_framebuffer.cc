// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fl_framebuffer.h"

#include <epoxy/egl.h>
#include <epoxy/gl.h>

#include "flutter/shell/platform/linux/fl_egl_image.h"

struct _FlFramebuffer {
  GObject parent_instance;

  // Width of framebuffer in pixels.
  size_t width;

  // Height of framebuffer in pixels.
  size_t height;

  // Framebuffer ID.
  GLuint framebuffer_id;

  // Texture backing framebuffer.
  GLuint texture_id;

  // Stencil buffer associated with this framebuffer.
  GLuint depth_stencil;

  // EGL image for this texture.
  FlEGLImage* image;

  // Fence used to publish writes to an EGLImage sibling in another context.
  EGLDisplay sync_display;
  EGLSync sync;
  gboolean sync_uses_core_api;
};

G_DEFINE_TYPE(FlFramebuffer, fl_framebuffer, G_TYPE_OBJECT)

static void destroy_sync(FlFramebuffer* self);

static void fl_framebuffer_dispose(GObject* object) {
  FlFramebuffer* self = FL_FRAMEBUFFER(object);

  destroy_sync(self);

  g_clear_object(&self->image);
  glDeleteFramebuffers(1, &self->framebuffer_id);
  glDeleteTextures(1, &self->texture_id);
  glDeleteRenderbuffers(1, &self->depth_stencil);

  G_OBJECT_CLASS(fl_framebuffer_parent_class)->dispose(object);
}

static void fl_framebuffer_class_init(FlFramebufferClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = fl_framebuffer_dispose;
}

static void fl_framebuffer_init(FlFramebuffer* self) {}

FlFramebuffer* fl_framebuffer_new(GLint format,
                                  size_t width,
                                  size_t height,
                                  gboolean shareable) {
  FlFramebuffer* self =
      FL_FRAMEBUFFER(g_object_new(fl_framebuffer_get_type(), nullptr));

  self->width = width;
  self->height = height;

  glGenTextures(1, &self->texture_id);
  glGenFramebuffers(1, &self->framebuffer_id);

  glBindFramebuffer(GL_FRAMEBUFFER, self->framebuffer_id);

  glBindTexture(GL_TEXTURE_2D, self->texture_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
               GL_UNSIGNED_BYTE, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  if (shareable) {
    self->image = fl_egl_image_new(self->texture_id);
  }

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         self->texture_id, 0);

  glGenRenderbuffers(1, &self->depth_stencil);
  glBindRenderbuffer(GL_RENDERBUFFER, self->depth_stencil);
  glRenderbufferStorage(GL_RENDERBUFFER,      // target
                        GL_DEPTH24_STENCIL8,  // internal format
                        width,                // width
                        height                // height
  );
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, self->depth_stencil);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                            GL_RENDERBUFFER, self->depth_stencil);

  return self;
}

gboolean fl_framebuffer_get_shareable(FlFramebuffer* self) {
  g_return_val_if_fail(FL_IS_FRAMEBUFFER(self), FALSE);
  return self->image != nullptr;
}

FlFramebuffer* fl_framebuffer_create_sibling(FlFramebuffer* self) {
  g_return_val_if_fail(FL_IS_FRAMEBUFFER(self), nullptr);
  g_return_val_if_fail(self->image != nullptr, nullptr);

  EGLDisplay current_display = eglGetCurrentDisplay();
  if (current_display == EGL_NO_DISPLAY ||
      current_display != fl_egl_image_get_display(self->image)) {
    g_warning("Failed to import EGL image: EGL displays do not match");
    return nullptr;
  }

  FlFramebuffer* sibling =
      FL_FRAMEBUFFER(g_object_new(fl_framebuffer_get_type(), nullptr));

  sibling->width = self->width;
  sibling->height = self->height;
  sibling->image = FL_EGL_IMAGE(g_object_ref(self->image));

  // Make texture from existing image.
  GLint saved_texture_binding;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &saved_texture_binding);
  glGenTextures(1, &sibling->texture_id);
  glBindTexture(GL_TEXTURE_2D, sibling->texture_id);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  while (glGetError() != GL_NO_ERROR) {
  }
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D,
                               fl_egl_image_get_image(self->image));
  GLenum import_error = glGetError();
  glBindTexture(GL_TEXTURE_2D, saved_texture_binding);

  if (import_error != GL_NO_ERROR) {
    g_warning("Failed to import EGL image: GL error 0x%04x", import_error);
    g_object_unref(sibling);
    return nullptr;
  }

  // Make framebuffer that uses this texture.
  glGenFramebuffers(1, &sibling->framebuffer_id);
  GLint saved_framebuffer_binding;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &saved_framebuffer_binding);
  glBindFramebuffer(GL_FRAMEBUFFER, sibling->framebuffer_id);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         sibling->texture_id, 0);
  GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  GLenum attachment_error = glGetError();
  glBindFramebuffer(GL_FRAMEBUFFER, saved_framebuffer_binding);

  if (attachment_error != GL_NO_ERROR ||
      framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
    g_warning(
        "Failed to attach EGL image: GL error 0x%04x, framebuffer status "
        "0x%04x",
        attachment_error, framebuffer_status);
    g_object_unref(sibling);
    return nullptr;
  }

  return sibling;
}

gboolean fl_framebuffer_shares_storage(FlFramebuffer* self,
                                       FlFramebuffer* other) {
  g_return_val_if_fail(FL_IS_FRAMEBUFFER(self), FALSE);
  g_return_val_if_fail(FL_IS_FRAMEBUFFER(other), FALSE);
  return self->image != nullptr && self->image == other->image;
}

static void destroy_sync(FlFramebuffer* self) {
  if (self->sync == EGL_NO_SYNC || self->sync_display == EGL_NO_DISPLAY) {
    return;
  }

  EGLBoolean result = self->sync_uses_core_api
                          ? eglDestroySync(self->sync_display, self->sync)
                          : eglDestroySyncKHR(self->sync_display, self->sync);
  if (result != EGL_TRUE) {
    g_warning("Failed to destroy EGL sync: EGL error 0x%04x", eglGetError());
  }
  self->sync = EGL_NO_SYNC;
  self->sync_display = EGL_NO_DISPLAY;
}

void fl_framebuffer_signal_ready(FlFramebuffer* self) {
  g_return_if_fail(FL_IS_FRAMEBUFFER(self));
  if (self->image == nullptr) {
    return;
  }

  destroy_sync(self);
  self->sync_display = eglGetCurrentDisplay();
  if (self->sync_display == EGL_NO_DISPLAY) {
    glFinish();
    return;
  }

  self->sync_uses_core_api = epoxy_egl_version(self->sync_display) >= 15 &&
                             epoxy_eglCreateSync != nullptr &&
                             epoxy_eglDestroySync != nullptr &&
                             epoxy_eglClientWaitSync != nullptr;
  if (self->sync_uses_core_api) {
    self->sync = eglCreateSync(self->sync_display, EGL_SYNC_FENCE, nullptr);
  }

  gboolean has_khr_fence =
      epoxy_has_egl_extension(self->sync_display, "EGL_KHR_fence_sync") &&
      epoxy_has_gl_extension("GL_OES_EGL_sync") &&
      epoxy_eglCreateSyncKHR != nullptr && epoxy_eglDestroySyncKHR != nullptr &&
      epoxy_eglClientWaitSyncKHR != nullptr;
  if (self->sync == EGL_NO_SYNC && has_khr_fence) {
    if (self->sync_uses_core_api) {
      eglGetError();
    }
    self->sync_uses_core_api = FALSE;
    self->sync =
        eglCreateSyncKHR(self->sync_display, EGL_SYNC_FENCE_KHR, nullptr);
  }

  if (self->sync == EGL_NO_SYNC) {
    self->sync_display = EGL_NO_DISPLAY;
    glFinish();
    return;
  }

  glFlush();
}

gboolean fl_framebuffer_wait_ready(FlFramebuffer* self) {
  g_return_val_if_fail(FL_IS_FRAMEBUFFER(self), FALSE);
  if (self->sync == EGL_NO_SYNC) {
    return TRUE;
  }

  gboolean server_wait_attempted = FALSE;
  gboolean server_waited = FALSE;
  EGLDisplay current_display = eglGetCurrentDisplay();
  gboolean can_server_wait = current_display != EGL_NO_DISPLAY &&
                             current_display == self->sync_display;
  if (can_server_wait && self->sync_uses_core_api &&
      epoxy_eglWaitSync != nullptr) {
    server_wait_attempted = TRUE;
    server_waited = eglWaitSync(self->sync_display, self->sync, 0) == EGL_TRUE;
  } else if (can_server_wait && !self->sync_uses_core_api &&
             epoxy_has_egl_extension(self->sync_display, "EGL_KHR_wait_sync") &&
             epoxy_eglWaitSyncKHR != nullptr) {
    server_wait_attempted = TRUE;
    server_waited =
        eglWaitSyncKHR(self->sync_display, self->sync, 0) == EGL_TRUE;
  }
  if (server_waited) {
    destroy_sync(self);
    return TRUE;
  }
  if (server_wait_attempted) {
    eglGetError();
  }

  EGLint result =
      self->sync_uses_core_api
          ? eglClientWaitSync(self->sync_display, self->sync, 0, EGL_FOREVER)
          : eglClientWaitSyncKHR(self->sync_display, self->sync, 0,
                                 EGL_FOREVER_KHR);
  gboolean ready = result == EGL_CONDITION_SATISFIED;
  if (!ready) {
    g_warning("Failed to wait for EGL sync: EGL error 0x%04x", eglGetError());
  }
  destroy_sync(self);
  return ready;
}

GLuint fl_framebuffer_get_id(FlFramebuffer* self) {
  return self->framebuffer_id;
}

GLuint fl_framebuffer_get_texture_id(FlFramebuffer* self) {
  return self->texture_id;
}

size_t fl_framebuffer_get_width(FlFramebuffer* self) {
  return self->width;
}

size_t fl_framebuffer_get_height(FlFramebuffer* self) {
  return self->height;
}

// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/linux/fl_view_renderer_opengl.h"

#include <epoxy/gl.h>
#include <gdk/gdkwayland.h>

#include "flutter/shell/platform/linux/fl_compositor_opengl.h"
#include "flutter/shell/platform/linux/fl_engine_private.h"
#include "flutter/shell/platform/linux/fl_opengl_manager.h"
#include "flutter/shell/platform/linux/fl_task_runner.h"

#if defined(__loongarch__) || defined(__loongarch64)
static gboolean has_gdk_gl_option(const gchar* options, const gchar* option) {
  if (options == nullptr) {
    return FALSE;
  }

  g_auto(GStrv) values = g_strsplit_set(options, ",:; ", -1);
  for (size_t i = 0; values[i] != nullptr; i++) {
    if (g_str_equal(values[i], option)) {
      return TRUE;
    }
  }
  return FALSE;
}

// GTK 3 selects the Wayland EGL client API while GDK initializes, before a
// GdkGLContext exists. Make its consumer context use the same GLES API as
// Flutter's producer contexts without discarding caller-supplied GDK options.
__attribute__((constructor)) static void configure_loongarch_gdk_gl() {
  const gchar* options = g_getenv("GDK_GL");
  if (has_gdk_gl_option(options, "gles")) {
    return;
  }

  g_autofree gchar* updated = options == nullptr || options[0] == '\0'
                                  ? g_strdup("gles")
                                  : g_strconcat(options, ",gles", nullptr);
  g_setenv("GDK_GL", updated, TRUE);
}
#endif

// Maximum time to wait for a frame to be ready before giving up and rendering.
static constexpr gint64 kRenderTimeoutMicroseconds = 100000;  // 100ms

struct _FlViewRendererOpenGL {
  FlViewRenderer parent_instance;

  // Engine this widget is rendering.
  FlEngine* engine;

  // TRUE if the view size should be controlled by Flutter.
  gboolean sized_to_content;

  // Rendering context for OpenGL.
  GdkGLContext* render_context;

  // Combines layers into frame.
  FlCompositorOpenGL* compositor;

  // Task runner to wait for frames on.
  FlTaskRunner* task_runner;

  // Ensure Flutter and GTK can access the frame stored in the compositor.
  GMutex frame_mutex;
};

G_DEFINE_TYPE(FlViewRendererOpenGL,
              fl_view_renderer_opengl,
              fl_view_renderer_get_type())

// Redraw the view from the GTK thread.
static gboolean redraw_cb(gpointer user_data) {
  g_autoptr(FlViewRendererOpenGL) self = FL_VIEW_RENDERER_OPENGL(user_data);

  if (self->compositor == nullptr) {
    return G_SOURCE_REMOVE;
  }

  fl_view_renderer_notify_frame(FL_VIEW_RENDERER(self));

  // If Flutter is controlling the window size, then resize the view if
  // necessary.
  GtkWidget* render_widget = GTK_WIDGET(self);
  GtkAllocation allocation;
  gtk_widget_get_allocation(render_widget, &allocation);
  gint scale_factor = gtk_widget_get_scale_factor(render_widget);
  size_t width = allocation.width * scale_factor;
  size_t height = allocation.height * scale_factor;
  size_t frame_width, frame_height;
  {
    g_autoptr(GMutexLocker) locker = g_mutex_locker_new(&self->frame_mutex);
    fl_compositor_opengl_get_frame_size(self->compositor, &frame_width,
                                        &frame_height);
  }
  gboolean frame_size_matches = width == frame_width && height == frame_height;
  if (self->sized_to_content && !frame_size_matches) {
    gtk_widget_set_size_request(render_widget, frame_width / scale_factor,
                                frame_height / scale_factor);
    GtkWidget* toplevel = gtk_widget_get_toplevel(render_widget);
    if (GTK_IS_WINDOW(toplevel)) {
      // Resize to smallest size, so that the window will shrink to fit the new
      // size of the render area.
      gtk_window_resize(GTK_WINDOW(toplevel), 1, 1);
    }
    return G_SOURCE_REMOVE;
  }

  gtk_widget_queue_draw(render_widget);

  return G_SOURCE_REMOVE;
}

// Wait for a frame matching the window size to be ready, or until the timeout
// expires. Must be called with the frame mutex held; the mutex is still held
// when this function returns.
static void wait_for_frame(FlViewRendererOpenGL* self,
                           GdkWindow* window,
                           gint scale_factor) {
  gint64 expiry_time = g_get_monotonic_time() + kRenderTimeoutMicroseconds;
  while (true) {
    size_t width = gdk_window_get_width(window) * scale_factor;
    size_t height = gdk_window_get_height(window) * scale_factor;
    size_t frame_width, frame_height;
    fl_compositor_opengl_get_frame_size(self->compositor, &frame_width,
                                        &frame_height);
    if (frame_width == width && frame_height == height) {
      break;
    }

    if (g_get_monotonic_time() > expiry_time) {
      g_warning(
          "Timed out waiting for OpenGL frame of size %zdx%zd (have "
          "%zdx%zd)",
          width, height, frame_width, frame_height);
      break;
    }

    g_mutex_unlock(&self->frame_mutex);
    fl_task_runner_wait(self->task_runner, expiry_time);
    g_mutex_lock(&self->frame_mutex);
  }
}

static gboolean validate_egl_image_sharing(FlViewRendererOpenGL* self) {
  FlOpenGLManager* manager = fl_engine_get_opengl_manager(self->engine);
  if (!fl_opengl_manager_make_platform_current(manager)) {
    return FALSE;
  }

  FlFramebuffer* producer = fl_framebuffer_new(GL_RGBA, 1, 1, TRUE);
  gboolean producer_shareable = fl_framebuffer_get_shareable(producer);
  if (producer_shareable) {
    GLint saved_framebuffer_binding = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &saved_framebuffer_binding);
    glBindFramebuffer(GL_FRAMEBUFFER, fl_framebuffer_get_id(producer));
    while (glGetError() != GL_NO_ERROR) {
    }
    glClearColor(1.0, 0.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    producer_shareable = glGetError() == GL_NO_ERROR;
    glBindFramebuffer(GL_FRAMEBUFFER, saved_framebuffer_binding);
  }
  if (producer_shareable) {
    fl_framebuffer_signal_ready(producer);
  }
  fl_opengl_manager_clear_current(manager);

  FlFramebuffer* consumer = nullptr;
  gboolean valid = FALSE;
  if (producer_shareable) {
    gdk_gl_context_make_current(self->render_context);
    consumer = fl_framebuffer_create_sibling(producer);
    valid = consumer != nullptr && fl_framebuffer_wait_ready(producer);
    if (valid) {
      GLint saved_framebuffer_binding = 0;
      GLubyte pixel[4] = {};
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &saved_framebuffer_binding);
      glBindFramebuffer(GL_FRAMEBUFFER, fl_framebuffer_get_id(consumer));
      while (glGetError() != GL_NO_ERROR) {
      }
      glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
      valid = glGetError() == GL_NO_ERROR && pixel[0] == 0xff &&
              pixel[1] == 0x00 && pixel[2] == 0xff && pixel[3] == 0xff;
      glBindFramebuffer(GL_FRAMEBUFFER, saved_framebuffer_binding);
      if (!valid) {
        g_warning("EGL image sharing probe did not preserve pixel contents");
      }
    }
    g_clear_object(&consumer);
    gdk_gl_context_clear_current();
  }

  // Producer resources must be destroyed in the EGL share group that created
  // them, not in GDK's consumer context.
  if (fl_opengl_manager_make_platform_current(manager)) {
    g_object_unref(producer);
    fl_opengl_manager_clear_current(manager);
  } else {
    g_warning("Failed to restore EGL context after image sharing probe");
    g_object_unref(producer);
    valid = FALSE;
  }

  return valid;
}

// Implements GtkWidget::realize.
static void fl_view_renderer_opengl_realize(GtkWidget* widget) {
  FlViewRendererOpenGL* self = FL_VIEW_RENDERER_OPENGL(widget);

  GTK_WIDGET_CLASS(fl_view_renderer_opengl_parent_class)->realize(widget);

  g_autoptr(GError) error = nullptr;
  self->render_context = gdk_window_create_gl_context(
      gtk_widget_get_window(GTK_WIDGET(self)), &error);
  if (self->render_context == nullptr) {
    g_warning("Failed to create OpenGL context: %s", error->message);
    return;
  }

  // Flutter renders with OpenGL ES through EGL on Wayland. Request the same
  // API for the GDK consumer context before it is realized; some drivers,
  // including LoongGPU LG110, cannot create GDK's desktop GL default there.
  if (GDK_IS_WAYLAND_DISPLAY(gtk_widget_get_display(widget))) {
    gdk_gl_context_set_use_es(self->render_context, TRUE);
  }

  if (!gdk_gl_context_realize(self->render_context, &error)) {
    g_warning("Failed to realize OpenGL context: %s", error->message);
    g_clear_object(&self->render_context);
    return;
  }

  // Wayland can use EGLImage when both the Flutter producer context and the
  // GDK consumer context expose the required API. X11 uses a GLX consumer and
  // must copy via the CPU. In particular, LoongGPU advertises
  // GL_OES_EGL_image in GLX but crashes when importing an EGL-created image.
  gboolean shareable = FALSE;
  if (GDK_IS_WAYLAND_DISPLAY(gtk_widget_get_display(widget)) &&
      fl_opengl_manager_supports_egl_image(
          fl_engine_get_opengl_manager(self->engine))) {
    gdk_gl_context_make_current(self->render_context);
    shareable = epoxy_has_gl_extension("GL_OES_EGL_image") &&
                epoxy_glEGLImageTargetTexture2DOES != nullptr;
    gdk_gl_context_clear_current();
    if (shareable) {
      shareable = validate_egl_image_sharing(self);
    }
  }
  if (GDK_IS_WAYLAND_DISPLAY(gtk_widget_get_display(widget))) {
    g_debug("EGL image sharing is %s for the GDK Wayland backend",
            shareable ? "enabled" : "disabled");
  } else {
    g_debug("EGL image sharing is disabled for the GDK X11 backend");
  }
  self->task_runner =
      FL_TASK_RUNNER(g_object_ref(fl_engine_get_task_runner(self->engine)));
  self->compositor = fl_compositor_opengl_new(
      fl_engine_get_opengl_manager(self->engine), shareable);
}

// Implements GtkWidget::draw.
static gboolean fl_view_renderer_opengl_draw(GtkWidget* widget, cairo_t* cr) {
  FlViewRendererOpenGL* self = FL_VIEW_RENDERER_OPENGL(widget);

  fl_view_renderer_paint_background(FL_VIEW_RENDERER(self), cr);

  // The compositor is created when the widget is realized; if it is not yet
  // available there is nothing to render beyond the background.
  if (self->compositor == nullptr) {
    return TRUE;
  }

  GdkWindow* window = gtk_widget_get_window(widget);
  gint scale_factor = gdk_window_get_scale_factor(window);

  g_mutex_lock(&self->frame_mutex);

  // If frame not ready, then wait for it.
  if (!self->sized_to_content) {
    wait_for_frame(self, window, scale_factor);
  }

  if (self->render_context != nullptr) {
    gdk_gl_context_make_current(self->render_context);
  }

  gboolean result = fl_compositor_opengl_render(self->compositor, cr, window);

  if (self->render_context != nullptr) {
    gdk_gl_context_clear_current();
  }

  g_mutex_unlock(&self->frame_mutex);

  return result;
}

// Implements GtkWidget::unrealize.
static void fl_view_renderer_opengl_unrealize(GtkWidget* widget) {
  FlViewRendererOpenGL* self = FL_VIEW_RENDERER_OPENGL(widget);

  g_mutex_lock(&self->frame_mutex);
  if (self->render_context != nullptr && self->compositor != nullptr) {
    gdk_gl_context_make_current(self->render_context);
    fl_compositor_opengl_clear_render_cache(self->compositor);
    gdk_gl_context_clear_current();
  }
  g_mutex_unlock(&self->frame_mutex);

  GTK_WIDGET_CLASS(fl_view_renderer_opengl_parent_class)->unrealize(widget);
}

// Implements FlViewRenderer::present_layers.
static void fl_view_renderer_opengl_present_layers(FlViewRenderer* renderer,
                                                   const FlutterLayer** layers,
                                                   size_t layers_count) {
  FlViewRendererOpenGL* self = FL_VIEW_RENDERER_OPENGL(renderer);

  // Frames may be presented before the widget is realized and the compositor
  // is set up; ignore them.
  if (self->compositor == nullptr) {
    return;
  }

  g_mutex_lock(&self->frame_mutex);
  fl_compositor_opengl_composite_layers(self->compositor, layers, layers_count);
  g_mutex_unlock(&self->frame_mutex);

  // Wake up the GTK thread if it is waiting for this frame.
  fl_task_runner_stop_wait(self->task_runner);

  // Perform the redraw in the GTK thread.
  g_idle_add(redraw_cb, g_object_ref(self));
}

static void fl_view_renderer_opengl_dispose(GObject* object) {
  FlViewRendererOpenGL* self = FL_VIEW_RENDERER_OPENGL(object);

  g_clear_object(&self->engine);
  g_clear_object(&self->render_context);
  g_clear_object(&self->task_runner);
  g_mutex_clear(&self->frame_mutex);

  G_OBJECT_CLASS(fl_view_renderer_opengl_parent_class)->dispose(object);
}

static void fl_view_renderer_opengl_finalize(GObject* object) {
  FlViewRendererOpenGL* self = FL_VIEW_RENDERER_OPENGL(object);

  // The compositor is released here rather than in dispose() so it outlives a
  // forced dispose (e.g. gtk_widget_destroy()) and is only freed once the last
  // reference is dropped. This keeps it alive for the raster thread, which
  // holds a strong reference on the view (and thus this renderer) while
  // presenting.
  g_clear_object(&self->compositor);

  G_OBJECT_CLASS(fl_view_renderer_opengl_parent_class)->finalize(object);
}

static void fl_view_renderer_opengl_class_init(
    FlViewRendererOpenGLClass* klass) {
  GObjectClass* object_class = G_OBJECT_CLASS(klass);
  object_class->dispose = fl_view_renderer_opengl_dispose;
  object_class->finalize = fl_view_renderer_opengl_finalize;

  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
  widget_class->realize = fl_view_renderer_opengl_realize;
  widget_class->unrealize = fl_view_renderer_opengl_unrealize;
  widget_class->draw = fl_view_renderer_opengl_draw;

  FlViewRendererClass* renderer_class = FL_VIEW_RENDERER_CLASS(klass);
  renderer_class->present_layers = fl_view_renderer_opengl_present_layers;
}

static void fl_view_renderer_opengl_init(FlViewRendererOpenGL* self) {
  g_mutex_init(&self->frame_mutex);
}

FlViewRendererOpenGL* fl_view_renderer_opengl_new(FlEngine* engine,
                                                  gboolean sized_to_content) {
  g_return_val_if_fail(FL_IS_ENGINE(engine), nullptr);

  FlViewRendererOpenGL* self = FL_VIEW_RENDERER_OPENGL(
      g_object_new(fl_view_renderer_opengl_get_type(), nullptr));
  self->engine = FL_ENGINE(g_object_ref(engine));
  self->sized_to_content = sized_to_content;
  return self;
}

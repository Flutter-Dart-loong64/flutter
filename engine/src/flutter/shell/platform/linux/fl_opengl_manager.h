// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_LINUX_FL_OPENGL_MANAGER_H_
#define FLUTTER_SHELL_PLATFORM_LINUX_FL_OPENGL_MANAGER_H_

#include <glib-object.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE(FlOpenGLManager,
                     fl_opengl_manager,
                     FL,
                     OPENGL_MANAGER,
                     GObject)

/**
 * fl_opengl_manager_new:
 *
 * Creates an object that allows Flutter to render by OpenGL ES.
 *
 * Returns: a new #FlOpenGLManager.
 */
FlOpenGLManager* fl_opengl_manager_new();

/**
 * fl_opengl_manager_is_valid:
 * @manager: an #FlOpenGLManager.
 *
 * Checks whether EGL and all required OpenGL ES contexts were initialized.
 *
 * Returns: %TRUE if the manager is ready for rendering.
 */
gboolean fl_opengl_manager_is_valid(FlOpenGLManager* manager);

/**
 * fl_opengl_manager_supports_egl_image:
 * @manager: an #FlOpenGLManager.
 *
 * Checks whether the Flutter EGL context can export textures as EGL images.
 * The consumer context must check its own GL_OES_EGL_image support before
 * importing an image.
 *
 * Returns: %TRUE if EGL image export is supported.
 */
gboolean fl_opengl_manager_supports_egl_image(FlOpenGLManager* manager);

/**
 * fl_opengl_manager_make_current:
 * @manager: an #FlOpenGLManager.
 *
 * Makes the rendering context current.
 *
 * Returns: %TRUE if the context made current.
 */
gboolean fl_opengl_manager_make_current(FlOpenGLManager* manager);

/**
 * fl_opengl_manager_make_resource_current:
 * @manager: an #FlOpenGLManager.
 *
 * Makes the resource rendering context current.
 *
 * Returns: %TRUE if the context made current.
 */
gboolean fl_opengl_manager_make_resource_current(FlOpenGLManager* manager);

/**
 * fl_opengl_manager_make_platform_current:
 * @manager: an #FlOpenGLManager.
 *
 * Makes the platform rendering context current.
 *
 * Returns: %TRUE if the context made current.
 */
gboolean fl_opengl_manager_make_platform_current(FlOpenGLManager* manager);

/**
 * fl_opengl_manager_clear_current:
 * @manager: an #FlOpenGLManager.
 *
 * Clears the current rendering context.
 *
 * Returns: %TRUE if the context cleared.
 */
gboolean fl_opengl_manager_clear_current(FlOpenGLManager* manager);

G_END_DECLS

#endif  // FLUTTER_SHELL_PLATFORM_LINUX_FL_OPENGL_MANAGER_H_

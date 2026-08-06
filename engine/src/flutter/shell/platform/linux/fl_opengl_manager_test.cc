// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/linux/fl_opengl_manager.h"

#include "flutter/shell/platform/linux/testing/fl_test_gtk_logs.h"
#include "flutter/shell/platform/linux/testing/mock_epoxy.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

class FlOpenGLManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    flutter::testing::fl_ensure_gtk_init();
    ON_CALL(epoxy, glGetString(GL_VERSION))
        .WillByDefault(::testing::Return(
            reinterpret_cast<const GLubyte*>("OpenGL ES 2.0")));
    ON_CALL(epoxy, eglMakeCurrent).WillByDefault(::testing::Return(EGL_TRUE));
  }

  ::testing::NiceMock<flutter::testing::MockEpoxy> epoxy;
};

static EGLBoolean reject_config(EGLDisplay display,
                                const EGLint* attributes,
                                EGLConfig* configs,
                                EGLint config_size,
                                EGLint* num_configs) {
  *num_configs = 0;
  return EGL_TRUE;
}

static EGLBoolean accept_config(EGLDisplay display,
                                const EGLint* attributes,
                                EGLConfig* configs,
                                EGLint config_size,
                                EGLint* num_configs) {
  *configs = reinterpret_cast<EGLConfig>(1);
  *num_configs = 1;
  return EGL_TRUE;
}

TEST_F(FlOpenGLManagerTest, UsesSurfacelessContextsWhenSupported) {
  EXPECT_CALL(epoxy, eglCreatePbufferSurface).Times(0);

  g_autoptr(FlOpenGLManager) manager = fl_opengl_manager_new();
  ASSERT_TRUE(fl_opengl_manager_is_valid(manager));
  EXPECT_TRUE(fl_opengl_manager_make_current(manager));
  EXPECT_TRUE(fl_opengl_manager_make_resource_current(manager));
  EXPECT_TRUE(fl_opengl_manager_make_platform_current(manager));
  EXPECT_TRUE(fl_opengl_manager_clear_current(manager));
}

TEST_F(FlOpenGLManagerTest, UsesIndependentPbuffersWithoutSurfacelessContexts) {
  size_t make_current_calls = 0;
  EXPECT_CALL(epoxy, eglMakeCurrent)
      .WillRepeatedly([&make_current_calls](EGLDisplay display, EGLSurface draw,
                                            EGLSurface read,
                                            EGLContext context) {
        if (make_current_calls++ == 0) {
          EXPECT_EQ(draw, EGL_NO_SURFACE);
          EXPECT_EQ(read, EGL_NO_SURFACE);
          EXPECT_NE(context, EGL_NO_CONTEXT);
          return EGL_FALSE;
        }
        return EGL_TRUE;
      });
  EXPECT_CALL(epoxy, eglCreatePbufferSurface)
      .Times(3)
      .WillRepeatedly(::testing::Return(reinterpret_cast<EGLSurface>(1)));

  g_autoptr(FlOpenGLManager) manager = fl_opengl_manager_new();
  ASSERT_TRUE(fl_opengl_manager_is_valid(manager));
  EXPECT_TRUE(fl_opengl_manager_make_current(manager));
  EXPECT_TRUE(fl_opengl_manager_make_resource_current(manager));
  EXPECT_TRUE(fl_opengl_manager_make_platform_current(manager));
}

TEST_F(FlOpenGLManagerTest, RelaxesOptionalEGLConfigChannels) {
  EXPECT_CALL(epoxy, eglChooseConfig)
      .WillOnce(reject_config)
      .WillOnce(reject_config)
      .WillOnce(accept_config);

  g_autoptr(FlOpenGLManager) manager = fl_opengl_manager_new();
  EXPECT_TRUE(fl_opengl_manager_is_valid(manager));
}

TEST_F(FlOpenGLManagerTest, UsesSurfacelessConfigWithoutPbufferSupport) {
  EXPECT_CALL(epoxy, eglChooseConfig)
      .WillOnce(reject_config)
      .WillOnce(reject_config)
      .WillOnce(reject_config)
      .WillOnce(reject_config)
      .WillOnce(accept_config);
  EXPECT_CALL(epoxy, eglCreatePbufferSurface).Times(0);

  g_autoptr(FlOpenGLManager) manager = fl_opengl_manager_new();
  EXPECT_TRUE(fl_opengl_manager_is_valid(manager));
}

TEST_F(FlOpenGLManagerTest, RejectsConfigWithoutAnyCurrentContextPath) {
  EXPECT_CALL(epoxy, eglChooseConfig)
      .WillOnce(reject_config)
      .WillOnce(reject_config)
      .WillOnce(reject_config)
      .WillOnce(reject_config)
      .WillOnce(accept_config);
  EXPECT_CALL(epoxy, eglCreatePbufferSurface).Times(0);

  size_t make_current_calls = 0;
  EXPECT_CALL(epoxy, eglMakeCurrent)
      .WillRepeatedly([&make_current_calls](EGLDisplay display, EGLSurface draw,
                                            EGLSurface read,
                                            EGLContext context) {
        return make_current_calls++ == 0 ? EGL_FALSE : EGL_TRUE;
      });

  g_autoptr(FlOpenGLManager) manager = fl_opengl_manager_new();
  EXPECT_FALSE(fl_opengl_manager_is_valid(manager));
}

TEST_F(FlOpenGLManagerTest, DetectsEGLImageExportSupport) {
  ON_CALL(epoxy, epoxy_has_gl_extension(::testing::StrEq("GL_OES_EGL_image")))
      .WillByDefault(::testing::Return(true));

  g_autoptr(FlOpenGLManager) manager = fl_opengl_manager_new();
  ASSERT_TRUE(fl_opengl_manager_is_valid(manager));
  EXPECT_TRUE(fl_opengl_manager_supports_egl_image(manager));
}

TEST_F(FlOpenGLManagerTest, RejectsEGLImageWithoutGLExtension) {
  ON_CALL(epoxy, epoxy_has_gl_extension(::testing::_))
      .WillByDefault(::testing::Return(false));

  g_autoptr(FlOpenGLManager) manager = fl_opengl_manager_new();
  ASSERT_TRUE(fl_opengl_manager_is_valid(manager));
  EXPECT_FALSE(fl_opengl_manager_supports_egl_image(manager));
}

TEST_F(FlOpenGLManagerTest, RequiresCompleteKHREGLImageSupportBeforeEGL15) {
  ON_CALL(epoxy, epoxy_egl_version(::testing::_))
      .WillByDefault(::testing::Return(14));
  ON_CALL(epoxy, epoxy_has_egl_extension(::testing::_, ::testing::_))
      .WillByDefault([](EGLDisplay display, const char* extension) {
        return g_str_equal(extension, "EGL_KHR_image_base");
      });
  ON_CALL(epoxy, epoxy_has_gl_extension(::testing::StrEq("GL_OES_EGL_image")))
      .WillByDefault(::testing::Return(true));

  g_autoptr(FlOpenGLManager) manager = fl_opengl_manager_new();
  ASSERT_TRUE(fl_opengl_manager_is_valid(manager));
  EXPECT_FALSE(fl_opengl_manager_supports_egl_image(manager));
}

TEST_F(FlOpenGLManagerTest, DetectsCompleteKHREGLImageSupportBeforeEGL15) {
  ON_CALL(epoxy, epoxy_egl_version(::testing::_))
      .WillByDefault(::testing::Return(14));
  ON_CALL(epoxy, epoxy_has_egl_extension(::testing::_, ::testing::_))
      .WillByDefault([](EGLDisplay display, const char* extension) {
        return g_str_equal(extension, "EGL_KHR_image_base") ||
               g_str_equal(extension, "EGL_KHR_gl_texture_2D_image");
      });
  ON_CALL(epoxy, epoxy_has_gl_extension(::testing::StrEq("GL_OES_EGL_image")))
      .WillByDefault(::testing::Return(true));

  g_autoptr(FlOpenGLManager) manager = fl_opengl_manager_new();
  ASSERT_TRUE(fl_opengl_manager_is_valid(manager));
  EXPECT_TRUE(fl_opengl_manager_supports_egl_image(manager));
}

}  // namespace

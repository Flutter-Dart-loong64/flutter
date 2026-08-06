// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/linux/testing/linux_test.h"
#include "gtest/gtest.h"

#include "flutter/shell/platform/linux/fl_framebuffer.h"
#include "flutter/shell/platform/linux/testing/mock_epoxy.h"

class FlFramebufferTest : public flutter::testing::LinuxTest {
 protected:
  ::testing::NiceMock<flutter::testing::MockEpoxy> epoxy;
};

TEST_F(FlFramebufferTest, HasDepthStencil) {
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, FALSE);

  GLint depth_type = GL_NONE;
  glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                        &depth_type);
  EXPECT_NE(depth_type, GL_NONE);

  GLint stencil_type = GL_NONE;
  glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                                        &stencil_type);
  EXPECT_NE(stencil_type, GL_NONE);
}

TEST_F(FlFramebufferTest, ResourcesRemoved) {
  EXPECT_CALL(epoxy, glGenFramebuffers);
  EXPECT_CALL(epoxy, glGenTextures);
  EXPECT_CALL(epoxy, glGenRenderbuffers);
  FlFramebuffer* framebuffer = fl_framebuffer_new(GL_RGB, 100, 100, FALSE);

  EXPECT_CALL(epoxy, glDeleteFramebuffers);
  EXPECT_CALL(epoxy, glDeleteTextures);
  EXPECT_CALL(epoxy, glDeleteRenderbuffers);
  g_object_unref(framebuffer);
}

TEST_F(FlFramebufferTest, Sibling) {
  EXPECT_CALL(epoxy, eglCreateImage)
      .WillOnce(::testing::Return(reinterpret_cast<EGLImage>(1)));
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, TRUE);
  g_autoptr(FlFramebuffer) sibling = fl_framebuffer_create_sibling(framebuffer);
  EXPECT_NE(sibling, nullptr);
}

TEST_F(FlFramebufferTest, RejectsSiblingWhenEGLImageImportFails) {
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, TRUE);
  EXPECT_CALL(epoxy, glGetError)
      .WillOnce(::testing::Return(GL_NO_ERROR))
      .WillOnce(::testing::Return(GL_INVALID_OPERATION));

  g_autoptr(FlFramebuffer) sibling = fl_framebuffer_create_sibling(framebuffer);
  EXPECT_EQ(sibling, nullptr);
}

TEST_F(FlFramebufferTest, RejectsSiblingFromDifferentEGLDisplay) {
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, TRUE);
  ON_CALL(epoxy, eglGetCurrentDisplay)
      .WillByDefault(::testing::Return(reinterpret_cast<EGLDisplay>(2)));
  EXPECT_CALL(epoxy, glEGLImageTargetTexture2DOES).Times(0);

  g_autoptr(FlFramebuffer) sibling = fl_framebuffer_create_sibling(framebuffer);
  EXPECT_EQ(sibling, nullptr);
}

TEST_F(FlFramebufferTest, RejectsIncompleteSiblingFramebuffer) {
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, TRUE);
  EXPECT_CALL(epoxy, glCheckFramebufferStatus)
      .WillOnce(::testing::Return(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT));

  g_autoptr(FlFramebuffer) sibling = fl_framebuffer_create_sibling(framebuffer);
  EXPECT_EQ(sibling, nullptr);
}

TEST_F(FlFramebufferTest, EGLImageCreationFailureIsNotShareable) {
  EXPECT_CALL(epoxy, eglCreateImage)
      .WillOnce(::testing::Return(EGL_NO_IMAGE_KHR));
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, TRUE);
  EXPECT_FALSE(fl_framebuffer_get_shareable(framebuffer));
}

TEST_F(FlFramebufferTest, SynchronizesSharedFramebufferWithCoreEGL) {
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, TRUE);

  EXPECT_CALL(epoxy, eglCreateSync);
  EXPECT_CALL(epoxy, glFinish).Times(0);
  fl_framebuffer_signal_ready(framebuffer);

  EXPECT_CALL(epoxy, eglWaitSync).WillOnce(::testing::Return(EGL_TRUE));
  EXPECT_CALL(epoxy, eglClientWaitSync).Times(0);
  EXPECT_CALL(epoxy, eglDestroySync).Times(1);
  EXPECT_TRUE(fl_framebuffer_wait_ready(framebuffer));
}

TEST_F(FlFramebufferTest, FallsBackToClientWaitWhenCoreServerWaitFails) {
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, TRUE);
  fl_framebuffer_signal_ready(framebuffer);

  EXPECT_CALL(epoxy, eglWaitSync).WillOnce(::testing::Return(EGL_FALSE));
  EXPECT_CALL(epoxy, eglClientWaitSync)
      .WillOnce(::testing::Return(EGL_CONDITION_SATISFIED));
  EXPECT_TRUE(fl_framebuffer_wait_ready(framebuffer));
}

TEST_F(FlFramebufferTest, UsesClientWaitAcrossEGLDisplays) {
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, TRUE);
  fl_framebuffer_signal_ready(framebuffer);
  ON_CALL(epoxy, eglGetCurrentDisplay)
      .WillByDefault(::testing::Return(reinterpret_cast<EGLDisplay>(2)));

  EXPECT_CALL(epoxy, eglWaitSync).Times(0);
  EXPECT_CALL(epoxy, eglClientWaitSync)
      .WillOnce(::testing::Return(EGL_CONDITION_SATISFIED));
  EXPECT_TRUE(fl_framebuffer_wait_ready(framebuffer));
}

TEST_F(FlFramebufferTest, SynchronizesSharedFramebufferWithKHREGL) {
  ON_CALL(epoxy, epoxy_egl_version(::testing::_))
      .WillByDefault(::testing::Return(14));
  ON_CALL(epoxy, epoxy_has_egl_extension(::testing::_, ::testing::_))
      .WillByDefault([](EGLDisplay display, const char* extension) {
        return g_str_equal(extension, "EGL_KHR_image_base") ||
               g_str_equal(extension, "EGL_KHR_gl_texture_2D_image") ||
               g_str_equal(extension, "EGL_KHR_fence_sync") ||
               g_str_equal(extension, "EGL_KHR_wait_sync");
      });
  ON_CALL(epoxy, epoxy_has_gl_extension(::testing::StrEq("GL_OES_EGL_sync")))
      .WillByDefault(::testing::Return(true));
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, TRUE);

  EXPECT_CALL(epoxy, eglCreateSyncKHR);
  fl_framebuffer_signal_ready(framebuffer);

  EXPECT_CALL(epoxy, eglWaitSyncKHR).WillOnce(::testing::Return(EGL_TRUE));
  EXPECT_CALL(epoxy, eglClientWaitSyncKHR).Times(0);
  EXPECT_CALL(epoxy, eglDestroySyncKHR).Times(1);
  EXPECT_TRUE(fl_framebuffer_wait_ready(framebuffer));
}

TEST_F(FlFramebufferTest, UsesKHRClientWaitWithoutServerWaitExtension) {
  ON_CALL(epoxy, epoxy_egl_version(::testing::_))
      .WillByDefault(::testing::Return(14));
  ON_CALL(epoxy, epoxy_has_egl_extension(::testing::_, ::testing::_))
      .WillByDefault([](EGLDisplay display, const char* extension) {
        return g_str_equal(extension, "EGL_KHR_image_base") ||
               g_str_equal(extension, "EGL_KHR_gl_texture_2D_image") ||
               g_str_equal(extension, "EGL_KHR_fence_sync");
      });
  ON_CALL(epoxy, epoxy_has_gl_extension(::testing::StrEq("GL_OES_EGL_sync")))
      .WillByDefault(::testing::Return(true));
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, TRUE);
  fl_framebuffer_signal_ready(framebuffer);

  EXPECT_CALL(epoxy, eglWaitSyncKHR).Times(0);
  EXPECT_CALL(epoxy, eglClientWaitSyncKHR)
      .WillOnce(::testing::Return(EGL_CONDITION_SATISFIED_KHR));
  EXPECT_TRUE(fl_framebuffer_wait_ready(framebuffer));
}

TEST_F(FlFramebufferTest, FinishesWhenEGLFenceIsUnavailable) {
  ON_CALL(epoxy, epoxy_egl_version(::testing::_))
      .WillByDefault(::testing::Return(14));
  ON_CALL(epoxy, epoxy_has_egl_extension(::testing::_, ::testing::_))
      .WillByDefault([](EGLDisplay display, const char* extension) {
        return g_str_equal(extension, "EGL_KHR_image_base") ||
               g_str_equal(extension, "EGL_KHR_gl_texture_2D_image");
      });
  g_autoptr(FlFramebuffer) framebuffer =
      fl_framebuffer_new(GL_RGB, 100, 100, TRUE);

  EXPECT_CALL(epoxy, glFinish).Times(1);
  fl_framebuffer_signal_ready(framebuffer);
  EXPECT_TRUE(fl_framebuffer_wait_ready(framebuffer));
}

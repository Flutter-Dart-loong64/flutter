// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"

#include "flutter/shell/platform/linux/fl_egl_image.h"
#include "flutter/shell/platform/linux/testing/mock_epoxy.h"

TEST(FlEGLImageTest, Test) {
  ::testing::NiceMock<flutter::testing::MockEpoxy> epoxy;

  EXPECT_CALL(epoxy, eglCreateImage(testing::_, testing::_, testing::_,
                                    testing::_, testing::_))
      .WillOnce(testing::Return(reinterpret_cast<EGLImage>(1)));
  EXPECT_CALL(epoxy, eglDestroyImage(testing::_, testing::_)).Times(1);

  GLuint texture_id = 99;
  g_autoptr(FlEGLImage) image = fl_egl_image_new(texture_id);
  EXPECT_NE(fl_egl_image_get_image(image), EGL_NO_IMAGE_KHR);
}

TEST(FlEGLImageTest, CreationFailure) {
  ::testing::NiceMock<flutter::testing::MockEpoxy> epoxy;

  EXPECT_CALL(epoxy, eglCreateImage(testing::_, testing::_, testing::_,
                                    testing::_, testing::_))
      .WillOnce(testing::Return(EGL_NO_IMAGE_KHR));
  EXPECT_CALL(epoxy, eglDestroyImage(testing::_, testing::_)).Times(0);

  EXPECT_EQ(fl_egl_image_new(99), nullptr);
}

TEST(FlEGLImageTest, UsesKHREntryPointsBeforeEGL15) {
  ::testing::NiceMock<flutter::testing::MockEpoxy> epoxy;
  ON_CALL(epoxy, epoxy_egl_version(testing::_))
      .WillByDefault(testing::Return(14));
  ON_CALL(epoxy, epoxy_has_egl_extension(testing::_, testing::_))
      .WillByDefault(testing::Return(true));

  EXPECT_CALL(epoxy, eglCreateImageKHR(testing::_, testing::_, testing::_,
                                       testing::_, testing::_))
      .WillOnce(testing::Return(reinterpret_cast<EGLImageKHR>(1)));
  EXPECT_CALL(epoxy, eglDestroyImageKHR(testing::_, testing::_)).Times(1);

  g_autoptr(FlEGLImage) image = fl_egl_image_new(99);
  EXPECT_NE(fl_egl_image_get_image(image), EGL_NO_IMAGE_KHR);
}

TEST(FlEGLImageTest, FallsBackToKHREntryPointsWhenCoreCreationFails) {
  ::testing::NiceMock<flutter::testing::MockEpoxy> epoxy;
  ON_CALL(epoxy, epoxy_has_egl_extension(testing::_, testing::_))
      .WillByDefault(testing::Return(true));

  EXPECT_CALL(epoxy, eglCreateImage).WillOnce(testing::Return(EGL_NO_IMAGE));
  EXPECT_CALL(epoxy, eglCreateImageKHR)
      .WillOnce(testing::Return(reinterpret_cast<EGLImageKHR>(1)));
  EXPECT_CALL(epoxy, eglDestroyImageKHR(testing::_, testing::_)).Times(1);

  g_autoptr(FlEGLImage) image = fl_egl_image_new(99);
  EXPECT_NE(fl_egl_image_get_image(image), EGL_NO_IMAGE_KHR);
}

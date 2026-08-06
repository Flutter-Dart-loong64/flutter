// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_LINUX_FL_DART_PROJECT_PRIVATE_H_
#define FLUTTER_SHELL_PLATFORM_LINUX_FL_DART_PROJECT_PRIVATE_H_

#include "flutter/shell/platform/linux/public/flutter_linux/fl_dart_project.h"

G_BEGIN_DECLS

// Returns TRUE when the embedder explicitly selected an Impeller mode through
// fl_dart_project_set_enable_impeller().
gboolean fl_dart_project_has_impeller_override(FlDartProject* project);

G_END_DECLS

#endif  // FLUTTER_SHELL_PLATFORM_LINUX_FL_DART_PROJECT_PRIVATE_H_

// Copyright 2014 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../base/file_system.dart';
import '../base/project_migrator.dart';
import '../cmake_project.dart';

/// Adds the snippet to the CMake file that copies the native assets.
///
/// ```cmake
/// # Copy the native assets provided by the build.dart from all packages.
/// set(NATIVE_ASSETS_DIR "${PROJECT_BUILD_DIR}native_assets/linux/")
/// if(EXISTS "${NATIVE_ASSETS_DIR}")
///   install(DIRECTORY "${NATIVE_ASSETS_DIR}"
///     DESTINATION "${INSTALL_BUNDLE_LIB_DIR}"
///     COMPONENT Runtime)
/// endif()
/// ```
class CmakeNativeAssetsMigration extends ProjectMigrator {
  CmakeNativeAssetsMigration(CmakeBasedProject project, this.os, super.logger)
    : _cmakeFile = project.managedCmakeFile;

  final File _cmakeFile;
  final String os;

  @override
  Future<void> migrate() async {
    if (!_cmakeFile.existsSync()) {
      logger.printTrace('CMake project not found, skipping install() NATIVE_ASSETS_DIR migration.');
      return;
    }

    final String originalProjectContents = _cmakeFile.readAsStringSync();

    if (originalProjectContents.contains('set(NATIVE_ASSETS_DIR')) {
      // Command is already present.
      return;
    }

    final copyNativeAssetsCommand =
        '''

# Copy the native assets provided by the build.dart from all packages.
set(NATIVE_ASSETS_DIR "\${PROJECT_BUILD_DIR}native_assets/$os/")
if(EXISTS "\${NATIVE_ASSETS_DIR}" AND INSTALL_BUNDLE_LIB_DIR)
  install(DIRECTORY "\${NATIVE_ASSETS_DIR}"
    DESTINATION "\${INSTALL_BUNDLE_LIB_DIR}"
    COMPONENT Runtime)
endif()
''';

    // Insert the new command after the bundled libraries loop.
    const bundleLibrariesCommand = r'''
foreach(bundled_library ${PLUGIN_BUNDLED_LIBRARIES})
  install(FILES "${bundled_library}"
    DESTINATION "${INSTALL_BUNDLE_LIB_DIR}"
    COMPONENT Runtime)
endforeach(bundled_library)
''';

    var newProjectContents = originalProjectContents;

    newProjectContents = originalProjectContents.replaceFirst(
      bundleLibrariesCommand,
      '$bundleLibrariesCommand$copyNativeAssetsCommand',
    );

    if (originalProjectContents != newProjectContents) {
      logger.printStatus('CMake missing install() NATIVE_ASSETS_DIR command, updating.');
      _cmakeFile.writeAsStringSync(newProjectContents);
    }
  }
}

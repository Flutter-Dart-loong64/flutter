#!/usr/bin/env bash
# Copyright 2014 The Flutter Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

readonly flutter_bin_dir="${FLUTTER_ROOT}/bin"
readonly dart_bin_dir="${flutter_bin_dir}/cache/dart-sdk/bin"

if [[ -x "${flutter_bin_dir}/cache/flutter_tools_loong64" ]]; then
  case "$(uname -m)" in
    loongarch64|loong64)
      target_platform="$1"
      build_mode="$(printf '%s' "$2" | tr '[:upper:]' '[:lower:]')"
      flutter_target="${FLUTTER_TARGET:-lib/main.dart}"
      track_widget_creation="${TRACK_WIDGET_CREATION:-false}"
      tree_shake_icons="${TREE_SHAKE_ICONS:-false}"
      dart_obfuscation="${DART_OBFUSCATION:-false}"

      if [[ "${target_platform}" == darwin* ]]; then
        assemble_target="${build_mode}_macos_bundle_flutter_assets"
      else
        assemble_target="${build_mode}_bundle_${target_platform}_assets"
      fi

      assemble_args=()
      [[ "${VERBOSE_SCRIPT_LOGGING:-false}" == "true" ]] && assemble_args+=("--verbose")
      [[ "${PREFIXED_ERROR_LOGGING:-false}" == "true" ]] && assemble_args+=("--prefixed-errors")
      [[ -n "${FLUTTER_ENGINE:-}" ]] && assemble_args+=("--local-engine-src-path=${FLUTTER_ENGINE}")
      [[ -n "${LOCAL_ENGINE:-}" ]] && assemble_args+=("--local-engine=${LOCAL_ENGINE}")
      [[ -n "${LOCAL_ENGINE_HOST:-}" ]] && assemble_args+=("--local-engine-host=${LOCAL_ENGINE_HOST}")
      assemble_args+=(
        "--suppress-analytics"
        "assemble"
        "--no-version-check"
        "--output=build"
        "-dTargetPlatform=${target_platform}"
        "-dTrackWidgetCreation=${track_widget_creation}"
        "-dBuildMode=${build_mode}"
        "-dTargetFile=${flutter_target}"
        "-dTreeShakeIcons=${tree_shake_icons}"
        "-dDartObfuscation=${dart_obfuscation}"
      )
      [[ -n "${CODE_SIZE_DIRECTORY:-}" ]] && assemble_args+=("-dCodeSizeDirectory=${CODE_SIZE_DIRECTORY}")
      [[ -n "${SPLIT_DEBUG_INFO:-}" ]] && assemble_args+=("-dSplitDebugInfo=${SPLIT_DEBUG_INFO}")
      [[ -n "${DART_DEFINES:-}" ]] && assemble_args+=("--DartDefines=${DART_DEFINES}")
      [[ -n "${EXTRA_GEN_SNAPSHOT_OPTIONS:-}" ]] &&
        assemble_args+=("--ExtraGenSnapshotOptions=${EXTRA_GEN_SNAPSHOT_OPTIONS}")
      [[ -n "${FRONTEND_SERVER_STARTER_PATH:-}" ]] &&
        assemble_args+=("-dFrontendServerStarterPath=${FRONTEND_SERVER_STARTER_PATH}")
      [[ -n "${EXTRA_FRONT_END_OPTIONS:-}" ]] &&
        assemble_args+=("--ExtraFrontEndOptions=${EXTRA_FRONT_END_OPTIONS}")
      assemble_args+=("${assemble_target}")

      cd "${PROJECT_DIR}"
      exec "${flutter_bin_dir}/cache/flutter_tools_loong64" "${assemble_args[@]}"
      ;;
  esac
fi

exec "${dart_bin_dir}/dart" "${FLUTTER_ROOT}/packages/flutter_tools/bin/tool_backend.dart" "${@:1}"

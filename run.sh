#!/usr/bin/env bash
set -euo pipefail

OS_NAME="$(uname -s || echo unknown)"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

IS_WINDOWS=0
case "${OS_NAME}" in
  MINGW*|MSYS*|CYGWIN*|Windows_NT)
    IS_WINDOWS=1
    ;;
esac

if [ "${IS_WINDOWS}" -eq 0 ] && [[ "${ROOT_DIR}" == *:* ]]; then
  echo "Project path contains ':' which the Makefile generator can trip over. Move to a path without ':' and rerun." >&2
  exit 1
fi

# Auto-wire Homebrew Qt (qtbase/qtmultimedia/qt) into CMAKE_PREFIX_PATH.
if command -v brew >/dev/null 2>&1; then
  cmake_paths=()
  for pkg in qtmultimedia qtbase qt; do
    prefix="$(brew --prefix "${pkg}" 2>/dev/null || true)"
    if [ -n "${prefix}" ] && [ -d "${prefix}/lib/cmake" ]; then
      cmake_paths+=("${prefix}/lib/cmake")
      if [ -z "${Qt6_DIR:-}" ] && [ -f "${prefix}/lib/cmake/Qt6/Qt6Config.cmake" ]; then
        export Qt6_DIR="${prefix}/lib/cmake/Qt6"
      fi
      if [ -f "${prefix}/lib/cmake/Qt6Multimedia/Qt6MultimediaConfig.cmake" ]; then
        export Qt6Multimedia_DIR="${prefix}/lib/cmake/Qt6Multimedia"
      fi
      if [ -z "${Qt5_DIR:-}" ] && [ -f "${prefix}/lib/cmake/Qt5/Qt5Config.cmake" ]; then
        export Qt5_DIR="${prefix}/lib/cmake/Qt5"
      fi
    fi
  done
  if [ ${#cmake_paths[@]} -gt 0 ]; then
    joined=$(IFS=";"; echo "${cmake_paths[*]}")
    if [ -z "${CMAKE_PREFIX_PATH:-}" ]; then
      export CMAKE_PREFIX_PATH="${joined}"
    else
      export CMAKE_PREFIX_PATH="${joined};${CMAKE_PREFIX_PATH}"
    fi
  fi
  # Platform plugins
  plugin_paths=()
  for pkg in qtmultimedia qtbase qt; do
    prefix="$(brew --prefix "${pkg}" 2>/dev/null || true)"
    for cand in "${prefix}/share/qt/plugins" "${prefix}/plugins"; do
      if [ -n "${prefix}" ] && [ -d "${cand}" ]; then
        plugin_paths+=("${cand}")
      fi
    done
  done
  plugin_paths+=("/opt/homebrew/share/qt/plugins")
  if [ ${#plugin_paths[@]} -gt 0 ]; then
    export QT_PLUGIN_PATH="$(IFS=:; echo "${plugin_paths[*]}")"
  fi
  if [ -d "/opt/homebrew/opt/qtbase/share/qt/plugins/platforms" ]; then
    export QT_QPA_PLATFORM_PLUGIN_PATH="/opt/homebrew/opt/qtbase/share/qt/plugins/platforms"
  elif [ ${#plugin_paths[@]} -gt 0 ] && [ -d "${plugin_paths[0]}/platforms" ]; then
    export QT_QPA_PLATFORM_PLUGIN_PATH="${plugin_paths[0]}/platforms"
  fi
  if [ -d "/opt/homebrew/share/qt/plugins/permissions" ]; then
    export QT_PERMISSIONS_PLUGIN_PATH="/opt/homebrew/share/qt/plugins/permissions"
  elif [ -d "/opt/homebrew/opt/qtbase/share/qt/plugins/permissions" ]; then
    export QT_PERMISSIONS_PLUGIN_PATH="/opt/homebrew/opt/qtbase/share/qt/plugins/permissions"
  elif [ ${#plugin_paths[@]} -gt 0 ] && [ -d "${plugin_paths[0]}/permissions" ]; then
    export QT_PERMISSIONS_PLUGIN_PATH="${plugin_paths[0]}/permissions"
  fi
fi

echo "CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH:-<empty>}"
  echo "Qt6_DIR=${Qt6_DIR:-<unset>}"
  echo "Qt5_DIR=${Qt5_DIR:-<unset>}"
if command -v cmake >/dev/null 2>&1; then
  cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
  cmake --build "${BUILD_DIR}"
else
  echo "CMake not found; attempting pkg-config + g++ fallback (requires Qt5/6 Widgets+Multimedia)."
  if ! command -v pkg-config >/dev/null 2>&1; then
    echo "pkg-config not found; cannot build." >&2
    exit 1
  fi
  PKG_MODULES="Qt6Widgets Qt6Multimedia"
  if ! pkg-config --exists ${PKG_MODULES}; then
    PKG_MODULES="Qt5Widgets Qt5Multimedia"
  fi
  mkdir -p "${BUILD_DIR}"
  g++ -std=c++17 "${ROOT_DIR}/main.mm" -o "${BUILD_DIR}/note_record" $(pkg-config --cflags --libs ${PKG_MODULES})
fi

BIN_CANDIDATES=()
if [ "${IS_WINDOWS}" -eq 1 ]; then
  BIN_CANDIDATES+=("${BUILD_DIR}/note_record.exe" "${BUILD_DIR}/note_record")
elif [ "${OS_NAME}" = "Darwin" ]; then
  BIN_CANDIDATES+=("${BUILD_DIR}/note_record.app/Contents/MacOS/note_record" "${BUILD_DIR}/note_record")
else
  BIN_CANDIDATES+=("${BUILD_DIR}/note_record")
fi

APP_BINARY=""
for cand in "${BIN_CANDIDATES[@]}"; do
  if [ -x "${cand}" ]; then
    APP_BINARY="${cand}"
    break
  fi
done

if [ -z "${APP_BINARY}" ]; then
  echo "Built binary not found. Checked: ${BIN_CANDIDATES[*]}" >&2
  exit 1
fi

echo "Launching..."
"${APP_BINARY}"

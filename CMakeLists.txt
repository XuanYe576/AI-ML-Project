cmake_minimum_required(VERSION 3.14)

option(AUTO_INSTALL_QT "Bootstrap Qt (via vcpkg on Windows) when it is missing" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(WIN32 AND AUTO_INSTALL_QT)
  include(EnsureVcpkg)
  nr_setup_vcpkg()
endif()

# Clear any stale Objective-C++ settings from prior mac builds when configuring on non-Apple hosts.
if(NOT APPLE)
  unset(CMAKE_OBJCXX_COMPILER CACHE)
  unset(CMAKE_OBJCXX_COMPILER_AR CACHE)
  unset(CMAKE_OBJCXX_COMPILER_RANLIB CACHE)
endif()

project(NoteRecord LANGUAGES CXX)
if(APPLE)
  # Only probe Objective-C++ when actually on macOS.
  enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

if(APPLE)
  # Homebrew defaults for this machine (qtbase + qtmultimedia).
  list(APPEND CMAKE_PREFIX_PATH 
    "/opt/homebrew/opt/qtbase/lib/cmake"
    "/opt/homebrew/opt/qtmultimedia/lib/cmake"
    "/opt/homebrew/opt/qt/lib/cmake")
  if(NOT Qt6_DIR)
    set(Qt6_DIR "/opt/homebrew/opt/qtbase/lib/cmake/Qt6" CACHE PATH "" FORCE)
  endif()
  if(NOT Qt6Multimedia_DIR)
    set(Qt6Multimedia_DIR "/opt/homebrew/opt/qtmultimedia/lib/cmake/Qt6Multimedia" CACHE PATH "" FORCE)
  endif()
elseif(WIN32)
  # Common Qt install locations on Windows (override with CMAKE_PREFIX_PATH if needed).
  set(_qt_win_roots
      "$ENV{QTDIR}"
      "$ENV{Qt6_DIR}"
      "C:/Qt/6.7.2/msvc2019_64"
      "C:/Qt/6.6.3/msvc2019_64"
      "C:/Qt/6.5.3/msvc2019_64")
  foreach(_qt_root IN LISTS _qt_win_roots)
    if(_qt_root AND EXISTS "${_qt_root}/lib/cmake")
      list(APPEND CMAKE_PREFIX_PATH "${_qt_root}/lib/cmake")
    endif()
  endforeach()
endif()

set(QT_VERSION_MAJOR 6)
set(_qt_required_components Widgets Multimedia)
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS ${_qt_required_components})
if(NOT Qt${QT_VERSION_MAJOR}_FOUND AND WIN32 AND AUTO_INSTALL_QT)
  # Ensure Qt is present via vcpkg when building on Windows.
  nr_vcpkg_install(qtbase qtmultimedia)
  find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS ${_qt_required_components})
endif()
if(NOT Qt${QT_VERSION_MAJOR}_FOUND)
  message(FATAL_ERROR "Qt${QT_VERSION_MAJOR} Widgets+Multimedia not found. Set CMAKE_PREFIX_PATH or enable AUTO_INSTALL_QT on Windows.")
endif()
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Permissions QUIET)

set(SOURCES
    main.cpp
)

if(APPLE)
  # Allow macOS-specific Objective-C++ sections inside the main file.
  set_source_files_properties(main.cpp PROPERTIES LANGUAGE OBJCXX)
endif()

add_executable(note_record ${SOURCES})

target_link_libraries(note_record PRIVATE
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Multimedia
)
if(TARGET Qt${QT_VERSION_MAJOR}::Permissions)
  target_link_libraries(note_record PRIVATE Qt${QT_VERSION_MAJOR}::Permissions)
endif()

target_compile_definitions(note_record PRIVATE
    QT_DISABLE_DEPRECATED_BEFORE=0x050F00
)

if(APPLE)
  set_target_properties(note_record PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.noterecord"
    MACOSX_BUNDLE_BUNDLE_NAME "NoteRecord"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
  )
  target_link_libraries(note_record PRIVATE "-framework AVFoundation" "-framework Foundation")
elseif(WIN32)
  set_target_properties(note_record PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

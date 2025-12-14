cmake_minimum_required(VERSION 3.14)
project(NoteRecord LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()


# Homebrew defaults for this machine (qtbase + qtmultimedia).
if(APPLE)
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
endif()

# Force Qt6; adjust paths above if your install differs.
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)
find_package(Qt6 COMPONENTS Permissions QUIET)
set(QT_VERSION_MAJOR 6)

set(SOURCES
    main.mm
    audio_processor.cpp
    ml_model.cpp
)

add_executable(note_record ${SOURCES})

target_link_libraries(note_record PRIVATE
  Qt6::Widgets
  Qt6::Multimedia
)
if(TARGET Qt6::Permissions)
  target_link_libraries(note_record PRIVATE Qt6::Permissions)
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

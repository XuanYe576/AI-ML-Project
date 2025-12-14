cmake_minimum_required(VERSION 3.14)

macro(nr_setup_vcpkg)
  if(NOT WIN32)
    return()
  endif()

  if(DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    message(STATUS "Using existing toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    return()
  endif()

  if(NOT DEFINED VCPKG_ROOT OR VCPKG_ROOT STREQUAL "")
    if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
      set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
    else()
      set(VCPKG_ROOT "${CMAKE_BINARY_DIR}/vcpkg")
    endif()
  endif()
  set(VCPKG_ROOT "${VCPKG_ROOT}" CACHE PATH "vcpkg root directory")

  set(_vcpkg_exe "${VCPKG_ROOT}/vcpkg.exe")
  if(NOT EXISTS "${_vcpkg_exe}")
    message(STATUS "Bootstrapping vcpkg into ${VCPKG_ROOT}")
    if(NOT EXISTS "${VCPKG_ROOT}/.git")
      execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${VCPKG_ROOT}")
      execute_process(
        COMMAND git clone --depth 1 https://github.com/microsoft/vcpkg "${VCPKG_ROOT}"
        RESULT_VARIABLE _git_res
      )
      if(NOT _git_res EQUAL 0)
        message(FATAL_ERROR "Failed to clone vcpkg (exit ${_git_res}). Set VCPKG_ROOT or clone manually.")
      endif()
    endif()
    execute_process(
      COMMAND "${VCPKG_ROOT}/bootstrap-vcpkg.bat"
      WORKING_DIRECTORY "${VCPKG_ROOT}"
      RESULT_VARIABLE _bootstrap_res
    )
    if(NOT _bootstrap_res EQUAL 0)
      message(FATAL_ERROR "vcpkg bootstrap failed (exit ${_bootstrap_res}).")
    endif()
  else()
    message(STATUS "Reusing vcpkg at ${VCPKG_ROOT}")
  endif()

  if(NOT DEFINED VCPKG_TARGET_TRIPLET OR VCPKG_TARGET_TRIPLET STREQUAL "")
    # Default to a 64-bit toolchain unless explicitly overridden.
    set(VCPKG_TARGET_TRIPLET "x64-windows")
    if(DEFINED CMAKE_SIZEOF_VOID_P AND CMAKE_SIZEOF_VOID_P EQUAL 4)
      set(VCPKG_TARGET_TRIPLET "x86-windows")
    endif()
    set(VCPKG_TARGET_TRIPLET "${VCPKG_TARGET_TRIPLET}" CACHE STRING "vcpkg target triplet" FORCE)
  endif()

  set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      CACHE FILEPATH "vcpkg toolchain file" FORCE)
  set(VCPKG_FEATURE_FLAGS "binarycaching,manifests" CACHE STRING "Enable vcpkg features" FORCE)
endmacro()

macro(nr_vcpkg_install)
  if(NOT WIN32)
    return()
  endif()
  if(NOT DEFINED VCPKG_ROOT OR VCPKG_ROOT STREQUAL "")
    message(FATAL_ERROR "VCPKG_ROOT is not set. Call nr_setup_vcpkg() first.")
  endif()
  set(_vcpkg_exe "${VCPKG_ROOT}/vcpkg.exe")
  if(NOT EXISTS "${_vcpkg_exe}")
    message(FATAL_ERROR "vcpkg executable not found at ${_vcpkg_exe}")
  endif()

  if(NOT ARGN)
    return()
  endif()

  message(STATUS "Installing packages via vcpkg: ${ARGN} (triplet: ${VCPKG_TARGET_TRIPLET})")
  execute_process(
    COMMAND "${_vcpkg_exe}" install ${ARGN} --triplet ${VCPKG_TARGET_TRIPLET}
    WORKING_DIRECTORY "${VCPKG_ROOT}"
    RESULT_VARIABLE _install_res
  )
  if(NOT _install_res EQUAL 0)
    message(FATAL_ERROR "vcpkg failed to install packages (${_install_res}).")
  endif()
endmacro()

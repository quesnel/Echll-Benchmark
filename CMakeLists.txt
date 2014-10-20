project(Echll_Benchmark)
cmake_minimum_required(VERSION 2.8.7)
set(Echll_Benchmark_MAJOR 0)
set(Echll_Benchmark_MINOR 0)
set(Echll_Benchmark_PATCH 99)
set(Echll_Benchmark_VERSION "${Echll_Benchmark_MAJOR}.${Echll_Benchmark_MINOR}.${Echll_Benchmark_PATCH}")
set(Echll_Benchmark_STABLEVERSION "${Echll_Benchmark_MAJOR}.${Echll_Benchmark_MINOR}")

if (CMAKE_BUILD_TYPE STREQUAL "")
  set (CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build" FORCE)
endif ()

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++1y")

  if ("${CMAKE_BUILD_TYPE}" EQUAL "Debug" OR "${CMAKE_BUILD_TYPE}" EQUAL "RelWithDebInfo")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb3")
  endif ()
endif ()

find_package(PkgConfig REQUIRED)
pkg_check_modules(ECHLL REQUIRED Echll-0.0)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_BINARY_DIR}
  ${ECHLL_INCLUDE_DIRS})

message(STATUS "checking for a (p)thread library")
find_package(Threads REQUIRED)
if (NOT Threads_FOUND)
  message(ERROR " failed to found a (p)thread support.")
else ()
  message(STATUS " threads support enabled (${CMAKE_THREAD_LIBS_INIT})")
  set(Echll_Benchmark_LINK_LIBRARIES ${Echll_Benchmark_LINK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT})
endif ()

message(STATUS "checking for a C++ compiler with MPI support")
find_package(MPI REQUIRED)
if (NOT MPI_CXX_FOUND)
  message(ERROR " failed to found a C++ compiler with MPI support.
  Please assign MPI_CXX_COMPILER to the MPI wrapper (mpicc, etc.) and
  rerun CMake. If this fails, set both MPI_CXX_LIBRARIES and
  MPI_CXX_INCLUDE_PATH and rerun CMake.")
else()
  message(STATUS " MPI support enabled")
  include_directories(${MPI_CXX_INCLUDE_PATH})
  set(Echll_Benchmark_LINK_LIBRARIES ${Echll_Benchmark_LINK_LIBRARIES}
    ${MPI_CXX_LIBRARIES})
endif ()

message(STATUS "checking for Boost headers and libraries")
find_package(Boost 1.55 REQUIRED COMPONENTS mpi serialization)
if (NOT Boost_FOUND)
  message(ERROR " failed to found Boost headers or Boost MPI library.
  Please install them or assign BOOST_ROOT, BOOST_INCLUDREDIR,
  BOOST_LIBRARYDIR and rerun CMake. Read the installed FindBoost in
  CMAke's directory too.")
else ()
  message(STATUS " Boost found")
  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
  set(Echll_Benchmark_LINK_LIBRARIES ${Echll_Benchmark_LINK_LIBRARIES}
    ${Boost_LIBRARIES})
endif ()

### # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
## Executable
#

add_executable(Echll_Benchmark linpackc.c linpackc.h defs.hpp
  linpackc.cpp linpackc.hpp main.cpp)

target_link_libraries(Echll_Benchmark ${Echll_Benchmark_LINK_LIBRARIES})

if (MPI_CXX_COMPILE_FLAGS)
  set_target_properties(Echll_Benchmark PROPERTIES
    COMPILE_FLAGS "${MPI_CXX_COMPILE_FLAGS}")
endif ()

if (MPI_CXX_LINK_FLAGS)
  set_target_properties(Echll_Benchmark PROPERTIES
    LINK_FLAGS "${MPI_CXX_LINK_FLAGS}")
endif ()

set_target_properties(Echll_Benchmark PROPERTIES
  COMPILE_FLAGS "-fvisibility=hidden -fvisibility-inlines-hidden")

install(TARGETS Echll_Benchmark DESTINATION bin)

### # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
## Testing
#

enable_testing()

message(STATUS "checking for 'catch.hpp'")
find_path(CATCH_INCLUDE_DIR catch.hpp PATHS /usr/include /usr/local/include ENV CATCH_INCLUDE_DIR)

if (CATCH_INCLUDE_DIR)
  message(STATUS " found 'catch.hpp' in ${CATCH_INCLUDE_DIR}")
  include_directories(${CATCH_INCLUDE_DIR})
else ()
  message(STATUS " not found catch.hpp. Unit test disabled")
endif ()


### # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # #
## CPack
#

include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Echll_Benchmark")
set(CPACK_PACKAGE_VENDOR "INRA")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_NAME "Echll_Benchmark")
set(CPACK_PACKAGE_VERSION_MAJOR "${Echll_Benchmark_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${Echll_Benchmark_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${Echll_Benchmark_PATCH}")
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "Echll_Benchmark ${Echll_Benchmark_MAJOR}.${Echll_Benchmark_MINOR}")
set(CPACK_STRIP_FILES "1")
set(CPACK_PACKAGE_CONTACT "Gauthier Quesnel <gauthier.quesnel@toulouse.inra.fr>")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY "1")

if (WIN32)
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY "0")
  set (CPACK_GENERATOR "ZIP;NSIS")
  set (CPACK_SOURCE_GENERATOR "ZIP")
else ()
  set (CPACK_GENERATOR "TGZ;DEB;RPM")
  set (CPACK_SOURCE_GENERATOR "TGZ;TBZ2")
endif ()

set(CPACK_SOURCE_PACKAGE_FILE_NAME "Echll_Benchmark-${Echll_Benchmark_VERSION}")
set(CPACK_SOURCE_IGNORE_FILES
  "/\\\\.git/"
  "\\\\.#"
  "/#"
  "/\\\\.gitignore$"
  "/\\\\.gitattributes$"
  "/build-debug-clang/"
  "/build-debug-gcc/"
  "/build-release-clang/"
  "/build-release-gcc/"
  "/build-cross/"
  "/coverage/"
  )

if (WIN32 AND NOT UNIX)
  set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}\\\\Echll_Benchmark.ico")
  set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}\\\\Echll_Benchmark.ico")
  set(CPACK_PACKAGE_ICON "${PROJECT_SOURCE_DIR}\\\\Echll_Benchmark.bmp")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\Echll_Benchmark.exe")
  set(CPACK_NSIS_DISPLAY_NAME "${CPACK_PACKAGE_INSTALL_DIRECTORY} Echll_Benchmark ${Echll_Benchmark_VERSION}")
  set(CPACK_NSIS_HELP_LINK "http:\\\\\\\\https://github.com/quesnel/Echll_Benchmark")
  set(CPACK_NSIS_URL_INFO_ABOUT "http:\\\\\\\\https://github.com/quesnel/Echll_Benchmark")
  set(CPACK_NSIS_CONTACT "gauthier.quesnel@toulouse.inra.fr")
  set(CPACK_NSIS_MODIFY_PATH ON)
else ()
  set(CPACK_STRIP_FILES "bin/Echll_Benchmark")
endif ()

INCLUDE(CPack)

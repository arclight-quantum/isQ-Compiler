cmake_minimum_required(VERSION 3.16)
project(quantum_ir)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()
option(BUILD_DOC "Build documentation" ON)
message(STATUS "QIR")
if(POLICY CMP0116)
  cmake_policy(SET CMP0116 NEW)
endif()


string(TOUPPER "${CMAKE_BUILD_TYPE}" uppercase_CMAKE_BUILD_TYPE)

if(uppercase_CMAKE_BUILD_TYPE STREQUAL "DEBUG" )
    option(ISQ_OPT_ENABLE_ASSERTIONS "Enable assertions" ON)
else()
    option(ISQ_OPT_ENABLE_ASSERTIONS "Enable assertions" OFF)
endif()


if(ISQ_OPT_ENABLE_ASSERTIONS)
    if(NOT MSVC)
        add_definitions(-D_DEBUG)
    endif()
    # On non-Debug builds cmake automatically defines NDEBUG, so we explicitly undefine it:
    if(NOT uppercase_CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        # NOTE: use `add_compile_options` rather than `add_definitions` since
        # `add_definitions` does not support generator expressions.
        add_compile_options($<$<OR:$<COMPILE_LANGUAGE:C>,$<COMPILE_LANGUAGE:CXX>>:-UNDEBUG>)

        # Also remove /D NDEBUG to avoid MSVC warnings about conflicting defines.
        foreach (flags_var_to_scrub
                CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS_MINSIZEREL
                CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_C_FLAGS_MINSIZEREL)
            string (REGEX REPLACE "(^| )[/-]D *NDEBUG($| )" " "
                    "${flags_var_to_scrub}" "${${flags_var_to_scrub}}")
        endforeach()
    endif()
endif()


# SET(CMAKE_BUILD_TYPE "Release")
set (CMAKE_CXX_STANDARD 20)
add_compile_options(-fno-rtti)
add_compile_options(-g)
find_package(LLVM 15.0.7 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
find_package(MLIR REQUIRED CONFIG)
message(STATUS "Found MLIR ${MLIR_PACKAGE_VERSION}")
message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")
set(CMAKE_MODULE_PATH ${LLVM_CMAKE_DIR} ${MLIR_CMAKE_DIR} ${PROJECT_SOURCE_DIR}/cmake ${PROJECT_SOURCE_DIR}/vendor/eigen/cmake)
# Resolve absolute path of mlir-tblgen.
# CMake on Nix does not work on relative path.
execute_process(COMMAND which ${MLIR_TABLEGEN_EXE}  OUTPUT_VARIABLE MLIR_TABLEGEN_EXE_ABSOLUTE)
string(STRIP "${MLIR_TABLEGEN_EXE_ABSOLUTE}" MLIR_TABLEGEN_EXE_ABSOLUTE)
set(MLIR_TABLEGEN_EXE ${MLIR_TABLEGEN_EXE_ABSOLUTE})
#include(UseGold)
# Find Eigen
# set(ENV{EIGEN3_ROOT} ${PROJECT_SOURCE_DIR}/vendor/eigen)
find_package(Eigen3 REQUIRED 3.4.0)

include(TableGen)
include(AddMLIR)

include_directories(${MLIR_INCLUDE_DIRS} "include")
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include/ ${EIGEN3_INCLUDE_DIR})
add_subdirectory(include)
add_definitions(${LLVM_DEFINITIONS})
add_subdirectory(lib)
get_property(mlir_libs GLOBAL PROPERTY MLIR_ALL_LIBS)
add_subdirectory(tools)

#add_custom_target(qir-doc ALL)
#add_dependencies(qir-doc mlir-dialect-doc)
#if (BUILD_DOC)
#add_subdirectory(docs)
#install(TARGETS doc_doxygen DESTINATION share/doc)
#endif (BUILD_DOC)

install(TARGETS isqir DESTINATION lib)
# add_subdirectory(frontends)

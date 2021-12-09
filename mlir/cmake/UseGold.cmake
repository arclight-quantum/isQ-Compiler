option(USE_MOLD "Use mold linker" ON)
execute_process(COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/toolchain)
if(USE_MOLD)
    execute_process(COMMAND ln -s ${PROJECT_SOURCE_DIR}/vendor/mold/x86_64-linux-unknown-mold ${CMAKE_CURRENT_BINARY_DIR}/toolchain/ld)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -B${CMAKE_CURRENT_BINARY_DIR}/toolchain")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -B${CMAKE_CURRENT_BINARY_DIR}/toolchain")
    message(STATUS "Using mold linker.")
endif()
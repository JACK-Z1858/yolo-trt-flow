# SPDX-License-Identifier: AGPL-3.0-only

cmake_minimum_required(VERSION 3.18)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" YOLO_TRT_FLOW_VERSION)
string(STRIP "${YOLO_TRT_FLOW_VERSION}" YOLO_TRT_FLOW_VERSION)
project(yolo_trt_flow VERSION "${YOLO_TRT_FLOW_VERSION}" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Threads REQUIRED)
include(CTest)
option(YOLO_BUILD_GPU "Build the TensorRT library and CLI" ON)

function(yolo_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(flow_common STATIC src/common/log.cpp)
target_include_directories(flow_common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(flow_common PUBLIC Threads::Threads)
yolo_warnings(flow_common)

add_executable(callback_demo examples/callback_demo.cpp)
target_link_libraries(callback_demo PRIVATE Threads::Threads)
yolo_warnings(callback_demo)

if(BUILD_TESTING)
    add_executable(queue_tests tests/common/queue_tests.cpp)
    target_include_directories(queue_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(queue_tests PRIVATE Threads::Threads)
    yolo_warnings(queue_tests)
    add_test(NAME queue_tests COMMAND queue_tests)
    set_tests_properties(queue_tests PROPERTIES TIMEOUT 10)
endif()

if(YOLO_BUILD_GPU)
    find_package(CUDAToolkit REQUIRED)
    find_package(OpenCV REQUIRED)
    find_package(TensorRT REQUIRED)

    add_library(yolo_trt_core STATIC src/yolo/trt_engine.cpp src/yolo/pipeline.cpp)
    target_include_directories(yolo_trt_core PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        ${OpenCV_INCLUDE_DIRS}
        ${TensorRT_INCLUDE_DIRS}
    )
    target_link_directories(yolo_trt_core PUBLIC ${TensorRT_LIB_DIRS})
    target_link_libraries(yolo_trt_core PUBLIC
        CUDA::cudart ${OpenCV_LIBS} ${TensorRT_LIBRARIES} Threads::Threads flow_common
    )
    yolo_warnings(yolo_trt_core)

    add_library(flow_cli_support STATIC
        src/app/cli/config.cpp src/app/cli/reader.cpp src/app/cli/renderer.cpp
    )
    target_include_directories(flow_cli_support PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include" ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(flow_cli_support PUBLIC ${OpenCV_LIBS} flow_common)
    yolo_warnings(flow_cli_support)

    add_executable(yolo_trt_flow src/app/cli/main.cpp src/app/cli/cli.cpp)
    target_link_libraries(yolo_trt_flow PRIVATE yolo_trt_core flow_cli_support)
    yolo_warnings(yolo_trt_flow)

    if(BUILD_TESTING)
        add_executable(frontend_tests tests/app/cli/frontend_tests.cpp)
        target_link_libraries(frontend_tests PRIVATE flow_cli_support)
        yolo_warnings(frontend_tests)
        add_test(NAME frontend_tests COMMAND frontend_tests)
        set_tests_properties(frontend_tests PROPERTIES TIMEOUT 10)
    endif()
endif()

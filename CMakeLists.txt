cmake_minimum_required(VERSION 2.8)
project(LFCPA)

find_package(LLVM REQUIRED CONFIG)

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR})
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR})
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fno-rtti")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include_directories(include)
add_llvm_loadable_module(
    lfcpa
    TestPass.cpp
    lib/CallString.cpp
    lib/LivenessBasedAA.cpp
    lib/LivenessPointsTo.cpp
    lib/LivenessSet.cpp
    lib/PointsToData.cpp
    lib/PointsToNode.cpp
    lib/PointsToNodeFactory.cpp
    lib/PointsToRelation.cpp)

cmake_minimum_required(VERSION 3.20)

project(CapsLockPass LANGUAGES CXX)

find_package(LLVM 18.1.4 EXACT REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake from ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

separate_arguments(
    LLVM_DEFINITIONS_LIST
    NATIVE_COMMAND
    "${LLVM_DEFINITIONS}"
)

add_definitions(${LLVM_DEFINITIONS_LIST})
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_llvm_pass_plugin(CapsLockPass
    src/CapsLockPass.cpp
)

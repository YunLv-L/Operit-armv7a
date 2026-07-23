cmake_minimum_required(VERSION 3.22.1)

project("llama_jni")

# ====== 针对 32 位 ARM 添加 Neon FPU 支持并禁用 LlamaFile ======
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    # 添加 FPU 指令集标志
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=neon-vfpv4")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfpu=neon-vfpv4")
    # 确保 LlamaFile 被禁用（避免 AArch64 专有指令）
    add_definitions(-DGGML_USE_LLAMAFILE=0)
endif()
# ==============================================================

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/operit_git_source.cmake")

set(LLAMA_BUILD_COMMON ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)

operit_prepare_git_source(
    OPERIT_LLAMA_CPP_SOURCE_DIR
    OPERIT_LLAMA_CPP_BINARY_DIR
    llama_cpp
    "https://github.com/ggml-org/llama.cpp.git"
    "master"
)

add_subdirectory("${OPERIT_LLAMA_CPP_SOURCE_DIR}" "${OPERIT_LLAMA_CPP_BINARY_DIR}" EXCLUDE_FROM_ALL)

add_library(
    LlamaWrapper
    SHARED
    src/main/cpp/llama_jni_stub.cpp
)

target_compile_definitions(LlamaWrapper PRIVATE OPERIT_HAS_LLAMA_CPP=1)
target_include_directories(LlamaWrapper PRIVATE "${OPERIT_LLAMA_CPP_SOURCE_DIR}")
target_include_directories(LlamaWrapper PRIVATE "${OPERIT_LLAMA_CPP_SOURCE_DIR}/common")
target_include_directories(LlamaWrapper PRIVATE "${OPERIT_LLAMA_CPP_SOURCE_DIR}/include")
target_include_directories(LlamaWrapper PRIVATE "${OPERIT_LLAMA_CPP_SOURCE_DIR}/ggml/include")
target_include_directories(LlamaWrapper PRIVATE "${OPERIT_LLAMA_CPP_SOURCE_DIR}/vendor")
target_link_libraries(LlamaWrapper llama llama-common)

target_link_libraries(
    LlamaWrapper
    android
    log
)

# 16KB page size support (Android 15+ requirement)
target_link_options(LlamaWrapper PRIVATE "-Wl,-z,max-page-size=16384")

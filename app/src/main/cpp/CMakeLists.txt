cmake_minimum_required(VERSION 3.22.1)

project(streamnative)

set(TOOLPKG_PROTECTION_SECRET_VALUE "$ENV{OPERIT_TOOLPKG_PROTECTION_SECRET}")

if("${TOOLPKG_PROTECTION_SECRET_VALUE}" STREQUAL "")
        set(TOOLPKG_PROTECTION_ENV_FILES
                "${CMAKE_CURRENT_LIST_DIR}/../../../../.env.local"
                "${CMAKE_CURRENT_LIST_DIR}/../../../../.env"
                "${CMAKE_CURRENT_LIST_DIR}/../../../.env.local"
                "${CMAKE_CURRENT_LIST_DIR}/../../../.env"
        )
        foreach(TOOLPKG_PROTECTION_ENV_FILE IN LISTS TOOLPKG_PROTECTION_ENV_FILES)
                if("${TOOLPKG_PROTECTION_SECRET_VALUE}" STREQUAL "" AND EXISTS "${TOOLPKG_PROTECTION_ENV_FILE}")
                        file(STRINGS "${TOOLPKG_PROTECTION_ENV_FILE}" TOOLPKG_PROTECTION_ENV_LINES
                                REGEX "^[ \t]*(export[ \t]+)?OPERIT_TOOLPKG_PROTECTION_SECRET[ \t]*="
                        )
                        list(LENGTH TOOLPKG_PROTECTION_ENV_LINES TOOLPKG_PROTECTION_ENV_LINE_COUNT)
                        if(TOOLPKG_PROTECTION_ENV_LINE_COUNT GREATER 0)
                                list(GET TOOLPKG_PROTECTION_ENV_LINES 0 TOOLPKG_PROTECTION_ENV_LINE)
                                string(REGEX REPLACE "^[ \t]*(export[ \t]+)?OPERIT_TOOLPKG_PROTECTION_SECRET[ \t]*=[ \t]*" "" TOOLPKG_PROTECTION_SECRET_VALUE "${TOOLPKG_PROTECTION_ENV_LINE}")
                                string(REGEX REPLACE "[ \t]*#.*$" "" TOOLPKG_PROTECTION_SECRET_VALUE "${TOOLPKG_PROTECTION_SECRET_VALUE}")
                                string(STRIP "${TOOLPKG_PROTECTION_SECRET_VALUE}" TOOLPKG_PROTECTION_SECRET_VALUE)
                                string(REGEX REPLACE "^\"(.*)\"$" "\\1" TOOLPKG_PROTECTION_SECRET_VALUE "${TOOLPKG_PROTECTION_SECRET_VALUE}")
                                string(REGEX REPLACE "^'(.*)'$" "\\1" TOOLPKG_PROTECTION_SECRET_VALUE "${TOOLPKG_PROTECTION_SECRET_VALUE}")
                        endif()
                endif()
        endforeach()
endif()

string(REPLACE "\\" "\\\\" TOOLPKG_PROTECTION_SECRET_VALUE "${TOOLPKG_PROTECTION_SECRET_VALUE}")
string(REPLACE "\"" "\\\"" TOOLPKG_PROTECTION_SECRET_VALUE "${TOOLPKG_PROTECTION_SECRET_VALUE}")

function(force_release_flags_for_debug target_name)
    if(TARGET ${target_name})
        # Keep app variant as Debug, but compile heavy native libs with release-like optimization.
        target_compile_definitions(${target_name} PRIVATE $<$<CONFIG:Debug>:NDEBUG>)
        target_compile_options(${target_name} PRIVATE $<$<CONFIG:Debug>:-O3 -g0>)
    endif()
endfunction()

function(enable_16kb_page_alignment target_name)
    if(TARGET ${target_name})
        target_link_options(${target_name} PRIVATE
                "-Wl,-z,max-page-size=16384"
                "-Wl,-z,common-page-size=16384"
        )
    endif()
endfunction()

if(ANDROID)
    # Build sherpa-ncnn as part of Android Studio externalNativeBuild.
    # Use local submodules to avoid downloading a different ncnn build.
    list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/sherpa-ncnn/cmake")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/sherpa-ncnn")

    set(FETCHCONTENT_SOURCE_DIR_NCNN "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ncnn" CACHE PATH "" FORCE)

    set(SHERPA_NCNN_ENABLE_PORTAUDIO OFF CACHE BOOL "" FORCE)
    set(SHERPA_NCNN_ENABLE_BINARY OFF CACHE BOOL "" FORCE)
    set(SHERPA_NCNN_ENABLE_TEST OFF CACHE BOOL "" FORCE)
    set(SHERPA_NCNN_ENABLE_C_API OFF CACHE BOOL "" FORCE)
    set(SHERPA_NCNN_ENABLE_JNI ON CACHE BOOL "" FORCE)
    set(SHERPA_NCNN_ENABLE_PYTHON OFF CACHE BOOL "" FORCE)
    set(SHERPA_NCNN_ENABLE_FFMPEG_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SHERPA_NCNN_ENABLE_GENERATE_INT8_SCALE_TABLE OFF CACHE BOOL "" FORCE)

    # Keep sherpa/ncnn static inside libsherpa-ncnn-jni and avoid OpenMP abort issues.
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(NCNN_OPENMP OFF CACHE BOOL "" FORCE)

    add_subdirectory(
            "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/sherpa-ncnn"
            "${CMAKE_BINARY_DIR}/sherpa_ncnn_build"
    )

    force_release_flags_for_debug(ncnn)
    force_release_flags_for_debug(sherpa-ncnn-core)
    force_release_flags_for_debug(sherpa-ncnn-jni)
endif()

add_library(
        streamnative
        SHARED
        streamnative/native_xml_splitter.cpp
        streamnative/native_markdown_splitter.cpp
        streamnative/native_toolpkg_protection.cpp
        streamnative/StreamOperators.cpp
        streamnative/plugins/StreamXmlPlugin.cpp
        streamnative/plugins/BaseJsonPlugin.cpp
        streamnative/plugins/StreamJsonPlugin.cpp
        streamnative/plugins/StreamPureJsonPlugin.cpp
        streamnative/plugins/StreamMarkdownPlugin.cpp
        streamnative/StreamBuilders.cpp
        streamnative/HotStream.cpp
        streamnative/StringExtensions.cpp
)

target_include_directories(
        streamnative
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(
        streamnative
        PRIVATE
        TOOLPKG_PROTECTION_SECRET="${TOOLPKG_PROTECTION_SECRET_VALUE}"
)

find_library(
        log-lib
        log
)

target_link_libraries(
        streamnative
        ${log-lib}
)

enable_16kb_page_alignment(streamnative)

cmake_minimum_required(VERSION 3.20)

project(CortexEMSpectrumFoundation
        VERSION 0.1.0
        DESCRIPTION "Electromagnetic spectrum compute foundation (141-decimal precision)"
        LANGUAGES CXX
)

# Guard against invalid build type strings like "Debug/Release"
get_property(isMulti GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT isMulti)
    if(CMAKE_BUILD_TYPE MATCHES ".*/.*")
        message(FATAL_ERROR "CMAKE_BUILD_TYPE must be a single value (Debug or Release), not '${CMAKE_BUILD_TYPE}'. Clean your build dir and reconfigure.")
    endif()
endif()

# -------- Build mode selection --------
# dev (default), test, perf, release
set(BUILD_MODE "dev" CACHE STRING "Build mode: dev, test, perf, release")
set_property(CACHE BUILD_MODE PROPERTY STRINGS dev test perf release)
message(STATUS "BUILD_MODE = ${BUILD_MODE}")

# Sensible defaults for build type per mode (can still be overridden)
if(NOT CMAKE_BUILD_TYPE)
    if(BUILD_MODE STREQUAL "dev" OR BUILD_MODE STREQUAL "test")
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    else()
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
endif()
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

# -------- Output directories --------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# -------- Compiler settings --------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CORTEX_EM_SPECTRUM_BUILD_CLI "Build CLI sample app(s)" ON)
option(CORTEX_EM_SPECTRUM_ENABLE_CUDA "Enable CUDA (stub, for future GPU integration)" OFF)

set(CORTEX_EM_SPECTRUM_PRECISION 141 CACHE STRING "Number of decimal digits for CosmicPrecision")

find_package(Threads REQUIRED)
find_package(Boost 1.70 CONFIG QUIET)
if(NOT Boost_FOUND)
    find_package(Boost 1.70 REQUIRED)
endif()

# -------- Warnings and optimizations by mode --------
if(MSVC)
    add_compile_options($<$<CONFIG:Debug>:/W4 /permissive-> $<$<CONFIG:Release>:/W4 /permissive->)
    if(BUILD_MODE STREQUAL "perf" OR BUILD_MODE STREQUAL "release")
        add_compile_options(/O2 /DNDEBUG /DNOMINMAX)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        add_link_options(/LTCG)
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(BUILD_MODE STREQUAL "perf" OR BUILD_MODE STREQUAL "release")
        add_compile_options(-O3 -DNDEBUG -march=native)
        include(CheckIPOSupported)
        check_ipo_supported(RESULT lto_supported OUTPUT lto_msg)
        if(lto_supported)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        endif()
    endif()
endif()

# -------- Source root and helper --------
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

function(first_existing OUT_VAR)
    foreach(p IN LISTS ARGN)
        if(EXISTS "${p}")
            set(${OUT_VAR} "${p}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${OUT_VAR} "" PARENT_SCOPE)
endfunction()

# Resolve main.cpp and context_overflow_guard.cpp in common locations
first_existing(CORTEX_MAIN_SRC
        "${PROJECT_ROOT}/main.cpp"
        "${PROJECT_ROOT}/cpp-foundation/src/main.cpp"
)

first_existing(CONTEXT_CPP
        "${PROJECT_ROOT}/context_overflow_guard.cpp"
        "${PROJECT_ROOT}/cpp-foundation/src/context_overflow_guard.cpp"
)

if(NOT CORTEX_MAIN_SRC)
    message(FATAL_ERROR "main.cpp not found in: ${PROJECT_ROOT} or cpp-foundation/src")
endif()
if(NOT CONTEXT_CPP)
    message(FATAL_ERROR "context_overflow_guard.cpp not found in: ${PROJECT_ROOT} or cpp-foundation/src")
endif()

# -------- Header-only interface (precision macro + includes) --------
add_library(cortex_em_spectrum INTERFACE)
target_compile_definitions(cortex_em_spectrum
        INTERFACE CORTEX_EM_SPECTRUM_PRECISION=${CORTEX_EM_SPECTRUM_PRECISION}
)
target_include_directories(cortex_em_spectrum
        INTERFACE
        ${PROJECT_ROOT}/cpp-foundation/include
        ${PROJECT_ROOT}
)
if(TARGET Boost::headers)
    target_link_libraries(cortex_em_spectrum INTERFACE Boost::headers)
elseif(DEFINED Boost_INCLUDE_DIRS)
    target_include_directories(cortex_em_spectrum INTERFACE ${Boost_INCLUDE_DIRS})
endif()

# -------- Core library (add .cpp sources as they appear) --------
add_library(cortex_core STATIC
        "${CONTEXT_CPP}"
)
target_include_directories(cortex_core PUBLIC
        ${PROJECT_ROOT}/cpp-foundation/include
        ${PROJECT_ROOT}
)
# Inherit precision macro + includes
target_link_libraries(cortex_core PUBLIC cortex_em_spectrum Threads::Threads)

target_compile_definitions(cortex_core PUBLIC
        $<$<STREQUAL:${BUILD_MODE},dev>:CORTEX_BUILD_DEV=1>
        $<$<STREQUAL:${BUILD_MODE},test>:CORTEX_BUILD_TEST=1>
        $<$<STREQUAL:${BUILD_MODE},perf>:CORTEX_BUILD_PERF=1>
        $<$<STREQUAL:${BUILD_MODE},release>:CORTEX_BUILD_RELEASE=1>
)

# -------- Executables --------
if(CORTEX_EM_SPECTRUM_BUILD_CLI)
    add_executable(cortex_em_cli
            "${CORTEX_MAIN_SRC}"
    )
    target_link_libraries(cortex_em_cli PRIVATE cortex_core)

    if(EXISTS "${PROJECT_ROOT}/context_overflow_integration.cpp")
        add_executable(context_overflow_integration
                "${PROJECT_ROOT}/context_overflow_integration.cpp"
        )
        target_link_libraries(context_overflow_integration PRIVATE cortex_core)
    endif()

    if(EXISTS "${PROJECT_ROOT}/dynamic_performance_integration.cpp")
        add_executable(dynamic_performance_integration
                "${PROJECT_ROOT}/dynamic_performance_integration.cpp"
        )
        target_link_libraries(dynamic_performance_integration PRIVATE cortex_core)
    endif()

    if(WIN32)
        target_link_libraries(cortex_em_cli PRIVATE user32 gdi32)
        if(TARGET context_overflow_integration)
            target_link_libraries(context_overflow_integration PRIVATE user32 gdi32)
        endif()
        if(TARGET dynamic_performance_integration)
            target_link_libraries(dynamic_performance_integration PRIVATE user32 gdi32)
        endif()
    endif()
endif()

# -------- CUDA (optional) --------
if(CORTEX_EM_SPECTRUM_ENABLE_CUDA)
    find_package(CUDAToolkit REQUIRED)
    enable_language(CUDA)

    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR CMAKE_CUDA_ARCHITECTURES STREQUAL "")
        if(CMAKE_VERSION VERSION_LESS "3.24")
            set(CMAKE_CUDA_ARCHITECTURES "61" CACHE STRING "CUDA archs" FORCE)  # GTX 1060 baseline
        else()
            set(CMAKE_CUDA_ARCHITECTURES "61" CACHE STRING "CUDA archs" FORCE)
        endif()
    endif()

    if(EXISTS "${PROJECT_ROOT}/cpp-foundation/src/spectrum_cuda.cu")
        add_library(cortex_em_cuda STATIC
                "${PROJECT_ROOT}/cpp-foundation/src/spectrum_cuda.cu"
        )
        target_include_directories(cortex_em_cuda PRIVATE ${PROJECT_ROOT}/cpp-foundation/include)
        target_compile_definitions(cortex_em_cuda PRIVATE CUDA_AVAILABLE=1)
        target_compile_options(cortex_em_cuda PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Wno-deprecated-gpu-targets>)
        set_target_properties(cortex_em_cuda PROPERTIES
                CUDA_SEPARABLE_COMPILATION ON
                CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}"
        )
        target_link_libraries(cortex_em_cuda PUBLIC CUDA::cudart)
    endif()

    if(TARGET cortex_em_cli)
        target_compile_definitions(cortex_em_cli PRIVATE CUDA_AVAILABLE=1)
        set_target_properties(cortex_em_cli PROPERTIES
                CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}"
        )
        if(TARGET cortex_em_cuda)
            target_link_libraries(cortex_em_cli PRIVATE
                    $<$<TARGET_EXISTS:CUDA::cudart>:CUDA::cudart>
                    $<$<TARGET_EXISTS:CUDA::cudadevrt>:CUDA::cudadevrt>
                    $<$<TARGET_EXISTS:CUDA::cublas>:CUDA::cublas>
                    $<$<TARGET_EXISTS:CUDA::cublasLt>:CUDA::cublasLt>
            )
        endif()
    endif()

    message(STATUS "CUDA enabled: archs=${CMAKE_CUDA_ARCHITECTURES}")
endif()

# -------- Tests --------
if(BUILD_MODE STREQUAL "test")
    file(GLOB TEST_SOURCES "${PROJECT_ROOT}/tests/*.cpp")
    if(TEST_SOURCES)
        add_executable(run_tests ${TEST_SOURCES})
        target_include_directories(run_tests PRIVATE
                "${PROJECT_ROOT}/cpp-foundation/include"
                "${PROJECT_ROOT}"
        )
        target_link_libraries(run_tests PRIVATE cortex_core)
        if(WIN32)
            target_link_libraries(run_tests PRIVATE user32 gdi32)
        endif()

        add_custom_target(quick_tests
                COMMAND "$<TARGET_FILE:run_tests>"
                DEPENDS run_tests
                COMMENT "Running unit tests..."
                VERBATIM
        )
    else()
        message(STATUS "No tests/*.cpp found; skipping test target.")
    endif()
endif()

# -------- Install (only for release) --------
if(BUILD_MODE STREQUAL "release")
    include(GNUInstallDirs)
    install(TARGETS cortex_em_cli
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# -------- Summary --------
message(STATUS "Summary:")
message(STATUS "  Mode: ${BUILD_MODE}")
message(STATUS "  Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  IPO/LTO: ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
message(STATUS "  CORTEX_EM_SPECTRUM_PRECISION = ${CORTEX_EM_SPECTRUM_PRECISION}")
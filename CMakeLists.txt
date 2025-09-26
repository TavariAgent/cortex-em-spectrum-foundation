cmake_minimum_required(VERSION 3.20)

project(CortexEMSpectrumFoundation
        VERSION 0.1.0
        DESCRIPTION "Electromagnetic spectrum compute foundation"
        LANGUAGES CXX
)

# ---------------- Build Mode Selection ----------------
set(BUILD_MODE "dev" CACHE STRING "Build mode: dev, test, perf, release")
set_property(CACHE BUILD_MODE PROPERTY STRINGS dev test perf release)
message(STATUS "BUILD_MODE = ${BUILD_MODE}")

if(NOT CMAKE_BUILD_TYPE)
    if(BUILD_MODE STREQUAL "dev" OR BUILD_MODE STREQUAL "test")
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    else()
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    endif()
endif()
message(STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")

# --------------- Output Dirs ----------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# --------------- Compiler Settings ----------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CORTEX_EM_SPECTRUM_BUILD_CLI "Build CLI executable" ON)
option(CORTEX_EM_SPECTRUM_ENABLE_CUDA "Enable CUDA support (optional)" OFF)

set(CORTEX_EM_SPECTRUM_PRECISION 141 CACHE STRING "Number of decimal digits for CosmicPrecision")

find_package(Threads REQUIRED)
find_package(Boost 1.70 CONFIG QUIET)
if(NOT Boost_FOUND)
    find_package(Boost 1.70 REQUIRED)
endif()

# --------------- Warnings / Opt Flags ----------------
if(MSVC)
    add_compile_options(
            $<$<CONFIG:Debug>:/W4>
            $<$<CONFIG:Debug>:/permissive->
            $<$<CONFIG:Release>:/W4>
            $<$<CONFIG:Release>:/permissive->
            $<$<CONFIG:RelWithDebInfo>:/W4>
            $<$<CONFIG:RelWithDebInfo>:/permissive->
            $<$<CONFIG:MinSizeRel>:/W4>
            $<$<CONFIG:MinSizeRel>:/permissive->
    )
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

# --------------- Paths & Sources ----------------
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

# Explicitly point to main.cpp at repo root (adjust if you move it)
set(CORTEX_MAIN_SRC "${PROJECT_ROOT}/main.cpp")
if(NOT EXISTS "${CORTEX_MAIN_SRC}")
    message(FATAL_ERROR "main.cpp not found at ${CORTEX_MAIN_SRC}")
endif()

# Core .cpp sources (add more here later; keep main.cpp OUT of this list)
set(CORE_SOURCES
        cpp-foundation/src/runtime_adaptive_guard.cpp
        # Add any future non-header-only sources here
)

# --------------- Interface Precision Layer ----------------
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

# --------------- Core Library ----------------
add_library(cortex_core STATIC ${CORE_SOURCES})
target_include_directories(cortex_core PUBLIC
        ${PROJECT_ROOT}/cpp-foundation/include
        ${PROJECT_ROOT}
)
target_link_libraries(cortex_core PUBLIC cortex_em_spectrum Threads::Threads)

target_compile_definitions(cortex_core PUBLIC
        $<$<STREQUAL:${BUILD_MODE},dev>:CORTEX_BUILD_DEV=1>
        $<$<STREQUAL:${BUILD_MODE},test>:CORTEX_BUILD_TEST=1>
        $<$<STREQUAL:${BUILD_MODE},perf>:CORTEX_BUILD_PERF=1>
        $<$<STREQUAL:${BUILD_MODE},release>:CORTEX_BUILD_RELEASE=1>
)

if (WIN32)
    # psapi needed if you use GetProcessMemoryInfo in process_memory.hpp
    target_link_libraries(cortex_core PUBLIC user32 gdi32 psapi)
endif()

# --------------- Executable ----------------
if(CORTEX_EM_SPECTRUM_BUILD_CLI)
    add_executable(cortex_em_cli "${CORTEX_MAIN_SRC}")
    target_link_libraries(cortex_em_cli PRIVATE cortex_core)
endif()

# --------------- CUDA (Optional Stub) ----------------
if(CORTEX_EM_SPECTRUM_ENABLE_CUDA)
    find_package(CUDAToolkit REQUIRED)
    enable_language(CUDA)
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR CMAKE_CUDA_ARCHITECTURES STREQUAL "")
        if(CMAKE_VERSION VERSION_LESS "3.24")
            set(CMAKE_CUDA_ARCHITECTURES "61" CACHE STRING "CUDA archs" FORCE)
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
            target_link_libraries(cortex_em_cli PRIVATE CUDA::cudart)
        endif()
    endif()

    message(STATUS "CUDA enabled: archs=${CMAKE_CUDA_ARCHITECTURES}")
endif()

# --------------- Tests ----------------
if(BUILD_MODE STREQUAL "test")
    file(GLOB TEST_SOURCES "${PROJECT_ROOT}/tests/*.cpp")
    if(TEST_SOURCES)
        add_executable(run_tests ${TEST_SOURCES})
        target_include_directories(run_tests PRIVATE
                ${PROJECT_ROOT}/cpp-foundation/include
                ${PROJECT_ROOT}
        )
        target_link_libraries(run_tests PRIVATE cortex_core)
        if (WIN32)
            target_link_libraries(run_tests PRIVATE user32 gdi32 psapi)
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

# --------------- Install (Release Only) ----------------
if(BUILD_MODE STREQUAL "release")
    include(GNUInstallDirs)
    if(TARGET cortex_em_cli)
        install(TARGETS cortex_em_cli
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()
endif()

# --------------- Summary ----------------
message(STATUS "Summary:")
message(STATUS "  Mode: ${BUILD_MODE}")
message(STATUS "  Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  IPO/LTO: ${CMAKE_INTERPROCEDURAL_OPTIMIZATION}")
message(STATUS "  CORTEX_EM_SPECTRUM_PRECISION = ${CORTEX_EM_SPECTRUM_PRECISION}")
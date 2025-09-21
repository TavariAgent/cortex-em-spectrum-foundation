cmake_minimum_required(VERSION 3.20)

project(CortexEMSpectrumFoundation
        VERSION 0.1.0
        DESCRIPTION "Electromagnetic spectrum compute foundation (141-decimal precision)"
        LANGUAGES CXX
)

# Options
option(CORTEX_EM_SPECTRUM_BUILD_CLI "Build CLI sample app in cpp-foundation/src/main.cpp" ON)
option(CORTEX_EM_SPECTRUM_BUILD_VIZ "Build visualization sample app in ElectromagneticSpectrumFoundation" ON)
option(CORTEX_EM_SPECTRUM_ENABLE_CUDA "Enable CUDA (stub, for future GPU integration)" OFF)

# Precision (matches repo default)
set(CORTEX_EM_SPECTRUM_PRECISION 141 CACHE STRING "Number of decimal digits for CosmicPrecision")

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Threads (portable)
find_package(Threads REQUIRED)

# Boost headers: prefer vcpkg config mode, fallback to module mode
# With vcpkg toolchain: -DCMAKE_TOOLCHAIN_FILE=C:/Users/tavar/vcpkg/scripts/buildsystems/vcpkg.cmake
# Ensure boost-multiprecision and boost-math (or boost) are installed for your triplet.
find_package(Boost 1.70 CONFIG QUIET)
if(NOT Boost_FOUND)
    find_package(Boost 1.70 REQUIRED) # module mode
endif()

# Header-only core library: expose include dirs and precision define
add_library(cortex_em_spectrum INTERFACE)
target_compile_definitions(cortex_em_spectrum
        INTERFACE CORTEX_EM_SPECTRUM_PRECISION=${CORTEX_EM_SPECTRUM_PRECISION}
)
target_include_directories(cortex_em_spectrum
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp-foundation/include
)

# Link/include Boost headers depending on how they were found
if(TARGET Boost::headers)
    target_link_libraries(cortex_em_spectrum INTERFACE Boost::headers)
elseif(DEFINED Boost_INCLUDE_DIRS)
    target_include_directories(cortex_em_spectrum INTERFACE ${Boost_INCLUDE_DIRS})
endif()

# Common warnings
function(set_common_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# IMPORTANT:
# Do NOT compile cpp-foundation/src/static_frame_generator.cpp here to avoid conflicts
# with the header-based StaticFrameGenerator (inline implementation).

# CLI sample
if(CORTEX_EM_SPECTRUM_BUILD_CLI)
    # Add a static library for the non-inline implementation of StaticFrameGenerator
    add_library(cortex_em_spectrum_static STATIC
        cpp-foundation/src/static_frame_generator.cpp
    )
    target_link_libraries(cortex_em_spectrum_static PUBLIC cortex_em_spectrum)
    target_include_directories(cortex_em_spectrum_static PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp-foundation/include
    )
    set_common_warnings(cortex_em_spectrum_static)

    add_executable(cortex_em_cli
        cpp-foundation/src/main.cpp
        # Add other non-conflicting sample sources as needed
        # e.g., cpp-foundation/src/precision_threading_integration.cpp
    )
    # Optional; redundant if CMAKE_CXX_STANDARD is already 17
    target_compile_features(cortex_em_cli PRIVATE cxx_std_17)

    target_link_libraries(cortex_em_cli PRIVATE cortex_em_spectrum_static Threads::Threads)
    set_common_warnings(cortex_em_cli)
endif()

# Visualization sample (Visual Studio-style main)
if(CORTEX_EM_SPECTRUM_BUILD_VIZ)
    add_executable(ElectromagneticSpectrumFoundation
            ElectromagneticSpectrumFoundation/ElectromagneticSpectrumFoundation.cpp
    )
    # Optional; redundant if CMAKE_CXX_STANDARD is already 17
    target_compile_features(ElectromagneticSpectrumFoundation PRIVATE cxx_std_17)

    target_link_libraries(ElectromagneticSpectrumFoundation PRIVATE cortex_em_spectrum Threads::Threads)
    set_common_warnings(ElectromagneticSpectrumFoundation)
endif()

# Optional CUDA scaffolding
if(CORTEX_EM_SPECTRUM_ENABLE_CUDA)
    enable_language(CUDA)
    message(STATUS "CUDA enabled: add CUDA kernels/targets and link against cortex_em_spectrum")
    # Example:
    # add_library(cortex_em_cuda STATIC src/gpu/kernels.cu)
    # target_link_libraries(cortex_em_cuda PRIVATE cortex_em_spectrum)
endif()

# Install (optional)
include(GNUInstallDirs)
if(CORTEX_EM_SPECTRUM_BUILD_CLI)
    install(TARGETS cortex_em_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(CORTEX_EM_SPECTRUM_BUILD_VIZ)
    install(TARGETS ElectromagneticSpectrumFoundation RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

message(STATUS "CORTEX_EM_SPECTRUM_PRECISION = ${CORTEX_EM_SPECTRUM_PRECISION}")
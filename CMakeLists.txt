cmake_minimum_required(VERSION 3.20)

project(CortexEMSpectrumFoundation
        VERSION 0.1.0
        DESCRIPTION "Electromagnetic spectrum compute foundation (141-decimal precision)"
        LANGUAGES CXX
)

option(CORTEX_EM_SPECTRUM_BUILD_CLI "Build CLI sample app in cpp-foundation/src/main.cpp" ON)
option(CORTEX_EM_SPECTRUM_BUILD_VIZ "Build visualization sample app in ElectromagneticSpectrumFoundation" ON)
option(CORTEX_EM_SPECTRUM_ENABLE_CUDA "Enable CUDA (stub, for future GPU integration)" OFF)

set(CORTEX_EM_SPECTRUM_PRECISION 141 CACHE STRING "Number of decimal digits for CosmicPrecision")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Boost 1.70 CONFIG QUIET)
if(NOT Boost_FOUND)
    find_package(Boost 1.70 REQUIRED)
endif()

add_library(cortex_em_spectrum INTERFACE)
target_compile_definitions(cortex_em_spectrum
        INTERFACE CORTEX_EM_SPECTRUM_PRECISION=${CORTEX_EM_SPECTRUM_PRECISION}
)
target_include_directories(cortex_em_spectrum
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp-foundation/include
)

if(TARGET Boost::headers)
    target_link_libraries(cortex_em_spectrum INTERFACE Boost::headers)
elseif(DEFINED Boost_INCLUDE_DIRS)
    target_include_directories(cortex_em_spectrum INTERFACE ${Boost_INCLUDE_DIRS})
endif()

function(set_common_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# Do not compile static_frame_generator.cpp here (header-based)

if(CORTEX_EM_SPECTRUM_BUILD_CLI)
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
    )
    target_compile_features(cortex_em_cli PRIVATE cxx_std_17)
    target_link_libraries(cortex_em_cli PRIVATE cortex_em_spectrum_static Threads::Threads)

    # NEW: Link GDI32 for screen capture on Windows
    if(WIN32)
        target_link_libraries(cortex_em_cli PRIVATE gdi32)
    endif()

    set_common_warnings(cortex_em_cli)
endif()

if(CORTEX_EM_SPECTRUM_BUILD_VIZ)
    add_executable(ElectromagneticSpectrumFoundation
            ElectromagneticSpectrumFoundation/ElectromagneticSpectrumFoundation.cpp
    )
    target_compile_features(ElectromagneticSpectrumFoundation PRIVATE cxx_std_17)
    target_link_libraries(ElectromagneticSpectrumFoundation PRIVATE cortex_em_spectrum Threads::Threads)
    set_common_warnings(ElectromagneticSpectrumFoundation)
endif()

if(CORTEX_EM_SPECTRUM_ENABLE_CUDA)
    enable_language(CUDA)
    message(STATUS "CUDA enabled: add CUDA kernels/targets and link against cortex_em_spectrum")
endif()

include(GNUInstallDirs)
if(CORTEX_EM_SPECTRUM_BUILD_CLI)
    install(TARGETS cortex_em_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(CORTEX_EM_SPECTRUM_BUILD_VIZ)
    install(TARGETS ElectromagneticSpectrumFoundation RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

message(STATUS "CORTEX_EM_SPECTRUM_PRECISION = ${CORTEX_EM_SPECTRUM_PRECISION}")
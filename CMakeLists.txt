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
    target_link_libraries(cortex_em_cli PRIVATE gdi32 user32)
    target_sources(cortex_em_cli PRIVATE cpp-foundation/src/screen_capture_shim.cpp)

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

    # ===== CUDA: compile kernel if present, set archs, link runtimes =====
    if(CORTEX_EM_SPECTRUM_ENABLE_CUDA)
        find_package(CUDAToolkit REQUIRED)
        enable_language(CUDA)

    # Choose archs: default to GTX 1060 (Pascal, sm_61). You can override at configure time.
    if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR CMAKE_CUDA_ARCHITECTURES STREQUAL "")
        if(CMAKE_VERSION VERSION_LESS "3.24")
            set(CMAKE_CUDA_ARCHITECTURES "61" CACHE STRING "CUDA archs" FORCE)  # GTX 1060
        else()
            # If you prefer auto-detect on newer CMake, uncomment next line:
            # set(CMAKE_CUDA_ARCHITECTURES "native" CACHE STRING "CUDA archs" FORCE)
            set(CMAKE_CUDA_ARCHITECTURES "61" CACHE STRING "CUDA archs" FORCE)
        endif()
    endif()

    # Optional kernel library if you have spectrum_cuda.cu
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cpp-foundation/src/spectrum_cuda.cu)
        add_library(cortex_em_cuda STATIC
                cpp-foundation/src/spectrum_cuda.cu
                cpp-foundation/src/screen_capture_shim.cpp
        )
        target_include_directories(cortex_em_cuda PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp-foundation/include)
        target_compile_definitions(cortex_em_cuda PRIVATE CUDA_AVAILABLE=1)
        target_compile_options(cortex_em_cuda PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Wno-deprecated-gpu-targets>)
        set_target_properties(cortex_em_cuda PROPERTIES
                CUDA_SEPARABLE_COMPILATION ON
                CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}"
        )
        target_link_libraries(cortex_em_cuda PUBLIC CUDA::cudart)
    endif()

    # The final exe does device link; it must also have CUDA_ARCHITECTURES
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

    message(STATUS "CUDA enabled: archs=${CMAKE_CUDA_ARCHITECTURES}")
endif()

include(GNUInstallDirs)
if(CORTEX_EM_SPECTRUM_BUILD_CLI)
    install(TARGETS cortex_em_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(CORTEX_EM_SPECTRUM_BUILD_VIZ)
    install(TARGETS ElectromagneticSpectrumFoundation RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

message(STATUS "CORTEX_EM_SPECTRUM_PRECISION = ${CORTEX_EM_SPECTRUM_PRECISION}")
        endif()
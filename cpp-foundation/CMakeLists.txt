cmake_minimum_required(VERSION 3.16)
project(CortexEMSpectrumFoundation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 🔧 Find Boost (required for 141-decimal precision)
find_package(Boost REQUIRED)

# 📁 Include directories
include_directories(cpp-foundation/include)

# 🎯 Source files
set(SOURCES
    cpp-foundation/src/precision_threading_integration.cpp
    cpp-foundation/src/context_overflow_integration.cpp
    cpp-foundation/src/dynamic_performance_integration.cpp
)

# 🧠 Header files
set(HEADERS
    cpp-foundation/include/cortex_baseline_processor.hpp
    cpp-foundation/include/cortex_buffer_baseline.hpp
    cpp-foundation/include/intelligent_term_delegator.hpp
    cpp-foundation/include/precision_safe_threading.hpp
    cpp-foundation/include/context_overflow_guard.hpp
)

# 🚀 Create executable
add_executable(cortex_foundation ${SOURCES} ${HEADERS})

# 🔗 Link Boost libraries
target_link_libraries(cortex_foundation ${Boost_LIBRARIES})

# 🎯 Compiler flags for optimization
target_compile_options(cortex_foundation PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/O2 /bigobj>
    $<$<CXX_COMPILER_ID:GNU>:-O3 -march=native>
    $<$<CXX_COMPILER_ID:Clang>:-O3 -march=native>
)
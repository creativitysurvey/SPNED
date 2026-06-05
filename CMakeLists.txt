cmake_minimum_required(VERSION 3.14)
project(SPNED_Benchmarks)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -funroll-loops -ffast-math")

include(FetchContent)
FetchContent_Declare(googlebenchmark GIT_REPOSITORY https://github.com/google/benchmark.git GIT_TAG v1.8.3)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

# 关闭 uBPF 的测试选项，防止它在后台继续套娃下载其它库
set(UBPF_ENABLE_TESTS OFF CACHE BOOL "Disable uBPF tests" FORCE)
add_subdirectory(ubpf EXCLUDE_FROM_ALL)

add_executable(run_benchmarks main.cpp)
# 修正头文件路径为本地的 ubpf 目录
target_include_directories(run_benchmarks PRIVATE $ENV{WASMTIME_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/ubpf/inc)
target_link_libraries(run_benchmarks PRIVATE benchmark::benchmark pthread dl ubpf $ENV{WASMTIME_DIR}/lib/libwasmtime.so)

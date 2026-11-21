cmake_minimum_required(VERSION 3.20)
project(calculator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
endif()

set(EVAL_SOURCES
  src/eval/value.cpp
  src/eval/tokenize.cpp
  src/eval/parse.cpp
  src/eval/builtins.cpp
  src/eval/session.cpp
)

add_executable(calculator WIN32
  src/main.cpp
  src/ui/window.cpp
  ${EVAL_SOURCES}
)

target_include_directories(calculator PRIVATE src)
target_compile_definitions(calculator PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(calculator PRIVATE user32 gdi32)

add_executable(launch_bench
  tools/launch_bench/main.cpp
)

target_compile_definitions(launch_bench PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(launch_bench PRIVATE user32 gdi32)

add_executable(eval_tests
  tests/eval_tests.cpp
  ${EVAL_SOURCES}
)

target_include_directories(eval_tests PRIVATE src)

enable_testing()
add_test(NAME eval_tests COMMAND eval_tests)

install(TARGETS calculator launch_bench RUNTIME DESTINATION bin)

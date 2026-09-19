cmake_minimum_required(VERSION 3.17)
project(treemap_parallel_cpp)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(ENABLE_ASAN "Enable AddressSanitizer" ON)
if (ENABLE_ASAN)
    add_compile_options(-fsanitize=address)
    add_link_options(-fsanitize=address)
endif ()

option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" ON)
if (ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
endif ()

add_executable(treemap_cpp src/090_main.cpp)
target_include_directories(treemap_cpp PRIVATE src)
target_link_libraries(treemap_cpp PRIVATE pthread)

add_executable(core_tests tests/core_tests.cpp)
target_include_directories(core_tests PRIVATE src)
target_link_libraries(core_tests PRIVATE pthread)

enable_testing()
add_test(NAME core_tests COMMAND core_tests)

# PGE3 GUI (vendored header in third_party/; PNG linked only because the
# header's image loader references it — the app itself loads no images)
find_package(X11 REQUIRED COMPONENTS Xi)
find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
add_executable(treemap_gui src/095_gui_main.cpp)
target_include_directories(treemap_gui PRIVATE src third_party)
target_link_libraries(treemap_gui PRIVATE X11::X11 X11::Xi OpenGL::GL PNG::PNG pthread)

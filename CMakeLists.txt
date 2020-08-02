cmake_minimum_required(VERSION 3.8)

project(nanotts)

add_compile_options(-Wall -Wextra -pedantic -Werror)

find_package(cxxopts REQUIRED)
find_package(fmt REQUIRED)
add_subdirectory(svoxpico)
add_subdirectory(src)

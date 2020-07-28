cmake_minimum_required(VERSION 3.8)

project(nanotts)

find_package(cxxopts REQUIRED)
add_subdirectory(svoxpico)
add_subdirectory(src)

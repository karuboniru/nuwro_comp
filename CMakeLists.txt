cmake_minimum_required(VERSION 3.15)
project(nuwro_analyze CXX)

find_package(NuWro REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Core Tree Hist Graf)

add_executable(analyze analyze.cpp)
target_link_libraries(analyze PRIVATE NuWro::event ROOT::Core ROOT::Tree ROOT::Hist ROOT::Graf)

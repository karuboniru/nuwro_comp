cmake_minimum_required(VERSION 3.15)
project(nuwro_analyze CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(NuWro REQUIRED)
find_package(ROOT REQUIRED COMPONENTS Core Tree Hist Graf ROOTDataFrame)
find_package(Boost REQUIRED COMPONENTS program_options)

add_executable(compare compare.cpp)
target_link_libraries(compare PRIVATE
    NuWro::event
    ROOT::Core ROOT::Tree ROOT::Hist ROOT::Graf ROOT::ROOTDataFrame
    Boost::program_options)

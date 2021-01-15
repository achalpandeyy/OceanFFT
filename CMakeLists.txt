cmake_minimum_required(VERSION 3.14.5)

project("OceanFFT")

add_subdirectory(External/Ogle)

add_executable(OceanFFT Source/Main.cpp)

target_link_libraries(OceanFFT Ogle)
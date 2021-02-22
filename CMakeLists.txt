cmake_minimum_required(VERSION 3.14.5)

project("OceanFFT")

add_subdirectory(External/Ogle)

add_executable(OceanFFT Source/Main.cpp)

target_link_libraries(OceanFFT Ogle)

add_custom_command(TARGET OceanFFT POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/Source/Shaders/ ${CMAKE_BINARY_DIR}/Shaders/)
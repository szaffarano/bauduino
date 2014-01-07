set(CMAKE_TOOLCHAIN_FILE cmake/cmake/ArduinoToolchain.cmake)

cmake_minimum_required(VERSION 2.8)

project(Bauduino C CXX)

set(ARDUINO_DEBUG False)
set(ARDUINO_DEFAULT_BOARD nano328)
set(ARDUINO_DEFAULT_PORT /dev/ttyUSB0)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libraries)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libraries/Bounce)

add_subdirectory(sketch)

cmake_minimum_required(VERSION 3.21.0)
set(CMAKE_TOOLCHAIN_FILE /home/ash/vcpkg/scripts/buildsystems/vcpkg.cmake)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
if(CMAKE_COMPILER_IS_GNUCXX)
  # needed for gcc 4.6+
  string(APPEND CMAKE_CXX_FLAGS " -fpermissive")
endif()

project(LanderS)

#main target
add_subdirectory(src)

#unit testing target
add_subdirectory(unit_tests)
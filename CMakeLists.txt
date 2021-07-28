cmake_minimum_required(VERSION 3.21.0)
set(CMAKE_TOOLCHAIN_FILE /home/ash/vcpkg/scripts/buildsystems/vcpkg.cmake)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
if(CMAKE_COMPILER_IS_GNUCXX)
  # needed for gcc 4.6+
  string(APPEND CMAKE_CXX_FLAGS " -fpermissive")
endif()

project(LanderS)

file(GLOB SOURCES CONFIGURE_DEPENDS 
    src/**.cpp
    src/Domain/**.cpp
    src/Ui/**.cpp
    src/Vk/**.cpp
    src/Service/**.cpp

    src/**.h
    src/Domain/**.h
    src/Ui/**.h
    src/Vk/**.h
    src/Service/**.h
)

include_directories(src/Ui src/Domain src/Vk src/Service)

add_executable(main ${SOURCES})

find_package(tinyobjloader CONFIG REQUIRED)
target_link_libraries(main PRIVATE tinyobjloader::tinyobjloader)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(main PRIVATE imgui::imgui)

find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(main PRIVATE Catch2::Catch2)

find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
target_link_libraries(main PRIVATE unofficial::vulkan-memory-allocator::vulkan-memory-allocator)

find_path(STB_INCLUDE_DIRS "stb.h")
target_include_directories(main PRIVATE ${STB_INCLUDE_DIRS})

find_package(Vulkan REQUIRED)
target_link_libraries(main PRIVATE Vulkan::Vulkan)

find_package(Bullet CONFIG REQUIRED)
target_include_directories(main PRIVATE ${BULLET_INCLUDE_DIR})
target_link_directories(main PRIVATE ${BULLET_LIBRARY_DIRS})
#target_link_libraries(main PRIVATE BulletDynamics BulletCollision LinearMath)
#target_link_libraries(main PRIVATE BulletSoftBody BulletDynamics BulletCollision LinearMath Bullet3Common)
target_link_libraries(main PRIVATE BulletDynamics BulletCollision LinearMath)

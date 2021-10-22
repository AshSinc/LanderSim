set(BulletExtrasPaths Service/BulletExtras/BulletFileLoader Service/BulletExtras/BulletWorldImporter)

file(GLOB SOURCES CONFIGURE_DEPENDS 
  main.cpp
  App/**.cpp
  Domain/**.cpp
  Domain/Objects/**.cpp
  Domain/Data/**.cpp
  Domain/Ai/**.cpp
  Ui/**.cpp
  Vk/**.cpp
  Service/**.cpp
  Service/BulletExtras/BulletFileLoader/**.cpp
  Service/BulletExtras/BulletWorldImporter/**.cpp

  App/**.h
  Domain/**.h
  Domain/Objects/**.h
  Domain/Data/**.h
  Domain/Ai/**.h
  Ui/**.h
  Vk/**.h
  Service/**.h
  Service/BulletExtras/BulletFileLoader/**.h
  Service/BulletExtras/BulletWorldImporter/**.h
)

include_directories(App Ui Domain Domain/Objects Domain/Data Domain/Ai Vk Service BulletExtrasPaths)

add_executable(LSApp ${SOURCES})

find_package(tinyobjloader CONFIG REQUIRED)
target_link_libraries(LSApp PRIVATE tinyobjloader::tinyobjloader)
find_package(imgui CONFIG REQUIRED)
target_link_libraries(LSApp PRIVATE imgui::imgui)

find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
target_link_libraries(LSApp PRIVATE unofficial::vulkan-memory-allocator::vulkan-memory-allocator)

find_path(STB_INCLUDE_DIRS "stb.h")
target_include_directories(LSApp PRIVATE ${STB_INCLUDE_DIRS})

find_package(Vulkan REQUIRED)
target_link_libraries(LSApp PRIVATE Vulkan::Vulkan)

find_package(Bullet CONFIG REQUIRED)
target_include_directories(LSApp PRIVATE ${BULLET_INCLUDE_DIR})
target_link_directories(LSApp PRIVATE ${BULLET_LIBRARY_DIRS})
target_link_libraries(LSApp PRIVATE BulletDynamics BulletCollision LinearMath)

#copies resources to the build path (so it doesnt break the code resource paths)
add_custom_command(TARGET LSApp POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:LSApp>/resources)
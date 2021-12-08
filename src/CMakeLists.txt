set(BulletExtrasPaths Service/BulletExtras/BulletFileLoader Service/BulletExtras/BulletWorldImporter)

#set( OpenCV_DIR "/home/ash/vcpkg/packages/opencv4_x64-linux/share/opencv" )

set( IMGUI_DIR 3rdParty/ImGuiPatch )

file(GLOB SOURCES CONFIGURE_DEPENDS 
  main.cpp
  App/**.cpp
  Domain/**.cpp
  Domain/Objects/**.cpp
  Domain/Data/**.cpp
  Domain/Lander/**.cpp
  Ui/**.cpp
  Vk/**.cpp
  Service/**.cpp
  Service/BulletExtras/BulletFileLoader/**.cpp
  Service/BulletExtras/BulletWorldImporter/**.cpp
  

  App/**.h
  Domain/**.h
  Domain/Objects/**.h
  Domain/Data/**.h
  Domain/Lander/**.h
  Ui/**.h
  Vk/**.h
  Service/**.h
  Service/BulletExtras/BulletFileLoader/**.h
  Service/BulletExtras/BulletWorldImporter/**.h

  3rdParty/ImGuiPatch/**.cpp
  3rdParty/ImGuiPatch/**.h

  #/opt/imgui-vulkan/**.cpp
  #/opt/imgui-vulkan/**.h
)

include_directories(App Ui Domain Domain/Objects Domain/Data Domain/Lander Vk Service BulletExtrasPaths 3rdParty/ImGuiPatch )

add_executable(LSApp ${SOURCES})

find_package(tinyobjloader CONFIG REQUIRED)
target_link_libraries(LSApp PRIVATE tinyobjloader::tinyobjloader)

find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
target_link_libraries(LSApp PRIVATE unofficial::vulkan-memory-allocator::vulkan-memory-allocator)

find_path(STB_INCLUDE_DIRS "stb.h")
#target_include_directories(LSApp PRIVATE ${STB_INCLUDE_DIRS})

find_package(Vulkan REQUIRED)
target_link_libraries(LSApp PRIVATE Vulkan::Vulkan)

#set(IMGUI_PATH  "/opt/imgui-vulkan/")
# Compile as static library 
#file(GLOB IMGUI_SOURCES ${IMGUI_PATH}/*.cpp) 
#add_library("ImGui" STATIC ${IMGUI_SOURCES})
#target_include_directories("ImGui" PRIVATE ${IMGUI_PATH})
#target_link_libraries(LSApp PRIVATE IMGUI_DIR)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(LSApp PRIVATE glfw)

find_package(OpenCV REQUIRED)
#target_include_directories(LSApp PRIVATE ${OpenCV_INCLUDE_DIRS}) # Not needed for CMake >= 2.8.11
target_link_libraries(LSApp PRIVATE ${OpenCV_LIBS})

find_package(Bullet CONFIG REQUIRED)
target_include_directories(LSApp PRIVATE ${BULLET_INCLUDE_DIR})
target_link_directories(LSApp PRIVATE ${BULLET_LIBRARY_DIRS})
target_link_libraries(LSApp PRIVATE BulletDynamics BulletCollision LinearMath)

#copies resources to the build path (so it doesnt break the code resource paths)
add_custom_command(TARGET LSApp POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:LSApp>/resources)
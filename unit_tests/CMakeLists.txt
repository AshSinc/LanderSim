set( IMGUI_DIR ../src/3rdParty/ImGuiPatch )

file(GLOB SOURCES CONFIGURE_DEPENDS 
  ../src/App/**.cpp
  ../src/Domain/**.cpp
  ../src/Domain/Objects/**.cpp
  ../src/Domain/Lander/**.cpp
  ../src/Ui/**.cpp
  ../src/Vk/**.cpp
  ../src/Service/**.cpp

  ../src/App/**.h
  ../src/Domain/**.h
  ../src/Domain/Objects/**.h
  ../src/Domain/Data/**.h
  ../src/Domain/Lander/**.h
  ../src/Ui/**.h
  ../src/Vk/**.h
  ../src/Service/**.h

  **.cpp

  ../src/3rdParty/ImGuiPatch/**.cpp
  ../src/3rdParty/ImGuiPatch/**.h
)

include_directories(../src/App/ ../src/Ui ../src/Domain ../src/Domain/Objects ../src/Domain/Lander ../src/Domain/Data ../src/Vk ../src/Service IMGUI_DIR)

add_executable(UnitTestsLS ${SOURCES})

find_package(tinyobjloader CONFIG REQUIRED)
target_link_libraries(UnitTestsLS PRIVATE tinyobjloader::tinyobjloader)

#find_package(imgui CONFIG REQUIRED)
#target_link_libraries(UnitTestsLS PRIVATE imgui::imgui)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(UnitTestsLS PRIVATE glfw)

find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(UnitTestsLS PRIVATE Catch2::Catch2)

find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
target_link_libraries(UnitTestsLS PRIVATE unofficial::vulkan-memory-allocator::vulkan-memory-allocator)

find_path(STB_INCLUDE_DIRS "stb.h")
target_include_directories(UnitTestsLS PRIVATE ${STB_INCLUDE_DIRS})

find_package(Vulkan REQUIRED)
target_link_libraries(UnitTestsLS PRIVATE Vulkan::Vulkan)

find_package(Bullet CONFIG REQUIRED)
target_include_directories(UnitTestsLS PRIVATE ${BULLET_INCLUDE_DIR})
target_link_directories(UnitTestsLS PRIVATE ${BULLET_LIBRARY_DIRS})
target_link_libraries(UnitTestsLS PRIVATE BulletDynamics BulletCollision LinearMath)

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

INCLUDES = \
	-I$(ROOT_DIR)/src/world \
	-I$(ROOT_DIR)/src/vulkan \
	-I/opt/bullet3-master/src \
	-I/opt/vk_mem_alloc \
	-I/opt/stb_image \
	-I/opt/tiny_obj_loader/ \
	-I/opt/imgui-vulkan/ \

BULLET_INCLUDE_PATHS_LIBS = -L/opt/bullet3-master/src/BulletCollision/ \
							-L/opt/bullet3-master/src/BulletDynamics/ \
							-L/opt/bullet3-master/src/LinearMath/ \
							-lBulletDynamics -lBulletCollision -lLinearMath

VULKAN_SDK_PATH = /opt/Vulkan_SDK/1.2.162.1/x86_64

CFLAGS = -std=c++17 -I$(VULKAN_SDK_PATH)/include $(INCLUDES)
LDFLAGS = -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan $(BULLET_INCLUDE_PATHS_LIBS)

IMGUI_CPP_PATHS = /opt/imgui-vulkan/*.cpp

OWN_CPP_PATHS = src/*.cpp src/world/*.cpp src/vulkan/*.cpp

VulkanTest: $(OWN_CPP_PATHS) $(IMGUI_CPP_PATHS)
	g++ $(CFLAGS) -o VulkanTest $(OWN_CPP_PATHS) $(IMGUI_CPP_PATHS) $(LDFLAGS)

VulkanDebug: $(OWN_CPP_PATHS) $(IMGUI_CPP_PATHS)
	g++ $(CFLAGS) -g -o VulkanDebug $(OWN_CPP_PATHS) $(IMGUI_CPP_PATHS) $(LDFLAGS)

VulkanOpt: $(OWN_CPP_PATHS) $(IMGUI_CPP_PATHS)
	g++ $(CFLAGS) -O3 -o VulkanOpt $(OWN_CPP_PATHS) $(IMGUI_CPP_PATHS) $(LDFLAGS)

.PHONY: test clean

test: VulkanTest
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib
	VK_LAYER_PATH=$(VULKAN_SDK_PATH)/etc/vulkan/explicit_layer.d 
	./VulkanTest

debug: VulkanDebug
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib
	VK_LAYER_PATH=$(VULKAN_SDK_PATH)/etc/vulkan/explicit_layer.d 

optimise: VulkanOpt
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib
	VK_LAYER_PATH=$(VULKAN_SDK_PATH)/etc/vulkan/explicit_layer.d 
	./VulkanOpt

7 clean:
	rm -f VulkanTest
	rm -f VulkanDebug
	rm -f VulkanOpt
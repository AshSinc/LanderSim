ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

INCLUDES = \
	-I$(ROOT_DIR)/src/world \
	-I$(ROOT_DIR)/src/vulkan \


BULLET_INCLUDE_PATH = /opt/bullet3-master/src
VULKAN_SDK_PATH = /opt/Vulkan_SDK/1.2.162.1/x86_64
VMA_INCLUDE_PATH = /opt/vk_mem_alloc
STB_INCLUDE_PATH = /opt/stb_image
TINYOBJ_INCLUDE_PATH = /opt/tiny_obj_loader/
CFLAGS = -std=c++17 -I$(BULLET_INCLUDE_PATH) -I$(VULKAN_SDK_PATH)/include -I$(VMA_INCLUDE_PATH) -I$(STB_INCLUDE_PATH) -I$(TINYOBJ_INCLUDE_PATH) $(INCLUDES)
LDFLAGS = -L$(VULKAN_SDK_PATH)/lib `pkg-config --static --libs glfw3` -lvulkan
LDFLAGS += -L$(BULLET_INCLUDE_PATH)/BulletCollision/ -L$(BULLET_INCLUDE_PATH)/BulletDynamics/ -L$(BULLET_INCLUDE_PATH)/LinearMath/ -lBulletDynamics -lBulletCollision -lLinearMath

VulkanTest: src/*.cpp src/world/*.cpp src/vulkan/*.cpp
	g++ $(CFLAGS) -o VulkanTest src/*.cpp src/world/*.cpp src/vulkan/*.cpp $(LDFLAGS)

VulkanDebug: src/*.cpp src/world/*.cpp src/vulkan/*.cpp
	g++ $(CFLAGS) -g -o VulkanDebug src/*.cpp src/world/*.cpp src/vulkan/*.cpp $(LDFLAGS)

VulkanOpt: src/*.cpp src/world/*.cpp src/vulkan/*.cpp
	g++ $(CFLAGS) -O3 -o VulkanOpt src/*.cpp src/world/*.cpp src/vulkan/*.cpp $(LDFLAGS)

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
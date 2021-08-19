#pragma once
#include "obj.h"
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>

struct WorldLightObject : virtual WorldObject{
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular{1,1,1};
    VkImage shadowmapImage;
    VmaAllocation shadowMapImageAllocation;
    VkImageView shadowMapImageView;
    ~WorldLightObject(){};
};
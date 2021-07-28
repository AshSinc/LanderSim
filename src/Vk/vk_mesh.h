#pragma once

#include "vk_types.h"
#include <vector>
#include <glm/vec3.hpp>
#include "world_state.h"

//this all needs cleaner up or moved into relevant classes/namespaces?



struct VertexInputDescription {
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attributes;
	VkPipelineVertexInputStateCreateFlags flags = 0;
};

//struct utilising glm library for storing pos and colour of a vertex
struct Vertex {
    glm::vec3 pos;
    glm::vec3 colour;
    glm::vec2 texCoord;
    glm::vec3 normal;
    static VertexInputDescription get_vertex_description();
    bool operator==(const Vertex& other) const;
};

struct Mesh {
    uint32_t indexCount; //how many indices in this mesh
    uint32_t indexBase; //what was the starting  index of this mesh
    int id;  //id used to store similar objects in the same bucket in unnordered map, not implemented atm
};

//will hold a descriptor set for each material
struct Material {
    glm::vec3 diffuse; //will be passed to GPUMaterialData
    glm::vec3 extra;
    glm::vec3 specular{1,1,1};
    
    VkDescriptorSet specularSet{VK_NULL_HANDLE}; //texture defaulted to null
    VkDescriptorSet diffuseSet{VK_NULL_HANDLE}; //texture defaulted to null

    std::vector<VkDescriptorSet> _multiTextureSets;
    int numDscSet = 0; // number of descriptor sets 
    VkDescriptorSet materialSet; //texture defaulted to null
    VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
    int propertiesId;
};

//this shouldnt be here really
struct RenderObject {
    WorldObject& rWorldObject; //pointer to the WorldObject this is rendering
	Material* material;
	glm::mat4 transformMatrix; //base transform, not rotations they will need to be held seperately i think, per object and then 
    uint32_t indexCount; //how many indices in this mesh (used for referencing )
    uint32_t indexBase; //what was the starting  index of this mesh
    int meshId; //id used to identify which mesh to index in indexed draw from storage buffer
    
};

//ccpreference.com recommends the following approach to hashing, combining the fields of a struct to create a decent quality hash function
//a hash function for Vertex is implemented by specifying a template specialisation for std::hash<T>
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
                //followed the pattern to add normal, not sure if it works need to check
                return ((((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.colour) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1)) >> 1) ^ (hash<glm::vec3>()(vertex.normal) << 1);
            }
    };
 }
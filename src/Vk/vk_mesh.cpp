#include "vk_mesh.h"

VertexInputDescription Vertex::get_vertex_description(){
    VertexInputDescription description;

    //we now need to tell Vulkan how to pass this data format to the vertex shader once its been uploaded to GPU memory
    //there are 2 types of structs needed for this, the first is VkVertexInputBindingDescription, which we will add a member function here to populate it
    VkVertexInputBindingDescription bindingDescription{};
    //a vertex binding describes at which rate to load data from memory throughout the vertices. Specifies the number of bytes between data entries,
    //and wether to move to the next data entry after each vertex or after each instance
    //all our per-vertex data is packed together in one array, so we're only going to have one binding. 
    //Binding parameter specifies index of the binding in the array of bindings.
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex); //stride specifies the number of bytes from one entry to the next
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    description.bindings.push_back(bindingDescription);

    //Attribute descriptions
    //the second structure that describes how to handle vertex input is VkVertexInputAttributeDescription
    //we have two attributes, position and colour so we need two attribute description structs
    //we added a third, texcoord
    VkVertexInputAttributeDescription positionAttribute{};
    positionAttribute.binding = 0; //tells vulkan from which binding the per-vertex data comes
    positionAttribute.location = 0; 
    positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT; //implicitly defines the byte size of attribute data, match vec3
    positionAttribute.offset = offsetof(Vertex, pos); 

    //now we describe the colour attribute for the vertex shader. ie layout(location = 1) in vec3 inColor;
    VkVertexInputAttributeDescription colourAttribute{};
    colourAttribute.binding = 0;
    colourAttribute.location = 1; 
    colourAttribute.format = VK_FORMAT_R32G32B32_SFLOAT; //to match vec3
    colourAttribute.offset = offsetof(Vertex, colour); //offest with colour

    //we have added a third attribute description for the texture coordinates, which will be used in image sampling
    //this allows us to access texture coordinates as input in the vertex shader, this is necessary to be able to pass them to the 
    //fragment shader
    VkVertexInputAttributeDescription textureCoordAttribute{};
    textureCoordAttribute.binding = 0;
    textureCoordAttribute.location = 2;
    textureCoordAttribute.format = VK_FORMAT_R32G32_SFLOAT;
    textureCoordAttribute.offset = offsetof(Vertex, texCoord);

    //Normal will be stored at Location 3
    VkVertexInputAttributeDescription normalAttribute{};
	normalAttribute.binding = 0;
	normalAttribute.location = 3;
	normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	normalAttribute.offset = offsetof(Vertex, normal);

    description.attributes.push_back(positionAttribute);
    description.attributes.push_back(colourAttribute);
    description.attributes.push_back(textureCoordAttribute);
    description.attributes.push_back(normalAttribute);

    return description;
};

bool Vertex::operator==(const Vertex& other) const { //used for testing equality when loading obj and storing unique vertices for indexing
    return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
}
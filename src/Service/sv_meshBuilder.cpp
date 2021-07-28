#include "sv_meshBuilder.h"
#include "vk_mesh.h"
#include <unordered_map>
#define GLM_FORCE_RADIANS //makes sure GLM uses radians to avoid confusion
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES //forces GLM to use a version of vec2 and mat4 that have the correct alignment requirements for Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE //forces GLM to use depth range of 0 to 1, instead of -1 to 1 as in OpenGL
#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL //using glm to hash Vertex attributes using a template specialisation of std::hash<T>
#include <glm/gtx/hash.hpp> //used in unnordered_map to compare vertices
#include <tiny_obj_loader.h> //we will use tiny obj loader to load our meshes from file (could use fast_obj_loader.h if this is too slow)


//this is where we will load the model data into vertices and indices member variables
/*void Service::MeshBuilder::loadModels(){
    //because we are using an unnordered_map with a custom type, we need to define equality and hash functons in Vertex
    std::unordered_map<Vertex, uint32_t> uniqueVertices{}; //store unique vertices in here, and check for repeating verticesto push index

    for(ModelInfo info : MODEL_INFOS){
        glm::vec3 colour = glm::vec3(0,0,1);//temp colour code
        if(info.modelName == "sphere")//temp colour code
            colour = glm::vec3(1,1,0.5f);//temp colour code
        else
            colour = glm::vec3(0,0,1);//temp colour code
        populateVerticesIndices(info.filePath, uniqueVertices, colour);
        _meshes[info.modelName] = _loadedMeshes[_loadedMeshes.size()-1];
    }
    //std::cout << "Vertices count for all " << allVertices.size() << "\n";
    //std::cout << "Indices count for all " << allIndices.size() << "\n";
}

void Service::MeshBuilder::populateVerticesIndices(std::string path, std::unordered_map<Vertex, uint32_t>& uniqueVertices, glm::vec3 baseColour){
    Mesh mesh;
    _loadedMeshes.push_back(mesh);
    int numMeshes = _loadedMeshes.size() - 1;
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if(!tinyobj::LoadObj(&attrib, &shapes, & materials, & warn, & err, path.c_str())){
        throw std::runtime_error(warn + err);
    }

    _loadedMeshes[numMeshes].id = numMeshes; //used in identify and indexing for draw calls into storage buffer
    _loadedMeshes[numMeshes].indexBase = allIndices.size(); //start of indices will be 0 for first object

    for(const auto& shape : shapes){
        for(const auto& index : shape.mesh.indices){
            Vertex vertex{};
            //the index variable is of type tinyob::index_t which coontains vertex_index, normal_index and texcoord_index members
            //we need to use these indexes to look up the actual vertex attributes in the attrib arrays
            vertex.pos = { 
                //because attrib vertices is an  array of float instead of a format we want like glm::vec3, we multiply the index by 3
                //ie skipping xyz per vertex index. 
                attrib.vertices[3 * index.vertex_index + 0], //offset 0 is X
                attrib.vertices[3 * index.vertex_index + 1], //offset 1 is Y
                attrib.vertices[3 * index.vertex_index + 2] //offset 2 is Z
            };

            vertex.texCoord = {
                //Similarly there are two texture coordinate components per entry ie, U and V
                attrib.texcoords[2 * index.texcoord_index + 0], //offset 0 is U
                //because OBJ format assumes a coordinate system where a vertical coordinate of 0 means the bottom of the image,
                1 - attrib.texcoords[2 * index.texcoord_index + 1] //offset 1 is V
            };

            //default vertex colour
            vertex.colour = baseColour;

            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
                
            };

            //store unique vertices in an unnordered map
            if(uniqueVertices.count(vertex) == 0){ //if vertices stored in uniqueVertices matching vertex == 0 
                uniqueVertices[vertex] = static_cast<uint32_t>(allVertices.size()); //add the count of vertices to the map matching the vertex
                allVertices.push_back(vertex); //add the vertex to vertices
            }
            allIndices.push_back(uniqueVertices[vertex]);
        }
    }
    _loadedMeshes[numMeshes].indexCount = allIndices.size() - _loadedMeshes[numMeshes].indexBase; //how many indices in this model
}*/
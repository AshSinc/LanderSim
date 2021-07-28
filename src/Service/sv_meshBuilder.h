#pragma once
#include <string>
#include <vector>
#include <unordered_map>

//class Vertex;
//namespace glm{class vec3;}

//model identifier and path pairs, for assigning to unnordered map, loading code needs cleaned and moved
/*const std::vector<ModelInfo> MODEL_INFOS = {
    {"satellite", "models/Satellite.obj"},
    {"asteroid", "models/asteroid.obj"},
    //{"asteroid", "models/Bennu.obj"},
    {"sphere", "models/sphere.obj"},
    {"box", "models/box.obj"}
};

struct Mesh {
    uint32_t indexCount; //how many indices in this mesh
    uint32_t indexBase; //what was the starting  index of this mesh
    int id;  //id used to store similar objects in the same bucket in unnordered map, not implemented atm
};

struct ModelInfo{
    std::string modelName;
    std::string filePath;
};*/

namespace Service{
    class MeshBuilder{
        

        //void loadModels();
        //void populateVerticesIndices(std::string path, std::unordered_map<Vertex, uint32_t>& uniqueVertices, glm::vec3 baseColour);
    };
}
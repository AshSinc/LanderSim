#pragma once
#include "obj.h"
#include "vk_mesh.h"

struct RenderObject : WorldObject {
    //WorldObject& rWorldObject; //pointer to the WorldObject this is rendering
	Material* material;
	glm::mat4 transformMatrix; //base transform, not rotations they will need to be held seperately i think, per object and then 
    uint32_t indexCount; //how many indices in this mesh (used for referencing )
    uint32_t indexBase; //what was the starting  index of this mesh
    int meshId; //id used to identify which mesh to index in indexed draw from storage buffer
};
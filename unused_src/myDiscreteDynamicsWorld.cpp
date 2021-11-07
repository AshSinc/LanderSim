#include "myDiscreteDynamicsWorld.h"
#include <iostream>
#include "mediator.h"
#include "obj.h"
#include <glm/gtx/fast_square_root.hpp>
#include "obj_lander.h"

void MyDynamicsWorld::init(Mediator& mediator){
	p_lander = mediator.scene_getLanderObject();
}

void MyDynamicsWorld::applyGravity(){
    for (int i = 0; i < m_nonStaticRigidBodies.size(); i++){
		btRigidBody* body = m_nonStaticRigidBodies[i];
		if (body->isActive()){
			//Hacky,  i == 1 is our lander collision object
			//if(i == 1){
				//but by using this to apply our own gravity within a child of DynamicsWorld, 
				//we can hopefully fix gravity problems when timestep is high 
			//	btVector3 direction = -btVector3(p_lander->pos.x, p_lander->pos.y, p_lander->pos.z); //asteroid is always at origin so dir of gravity is always towards 0
			//	float gravitationalForce = p_lander->asteroidGravForceMultiplier*glm::fastInverseSqrt(direction.distance(btVector3(0,0,0)));
			//	body->applyCentralForce(direction.normalize()*gravitationalForce);
			//}
			//else
				//body->applyGravity();
				//std::cout <<" testing \n";
		}
	}
}
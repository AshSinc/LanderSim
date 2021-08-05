#include <catch.hpp>
#include "dmn_myScene.h"
#include "obj_lander.h"
#include "obj_asteroid.h"
#include "mediator.h"
#include "world_physics.h"

IScene* p_newScene;
Mediator med = Mediator();

std::vector<std::shared_ptr<WorldObject>> objects;
std::vector<std::shared_ptr<CollisionRenderObj>> collisionObjects;
std::vector<std::shared_ptr<RenderObject>> renderObjects;

TEST_CASE("MyScene_Construct_and_destroy") {
    p_newScene = new MyScene(med);
    CHECK(p_newScene);
    delete p_newScene;
}

void initObjects(){
    std::shared_ptr<RenderObject> test1 = std::shared_ptr<RenderObject>(new RenderObject());
    test1->pos.y = 999;
    objects.push_back(test1);
    renderObjects.push_back(test1);

    std::shared_ptr<CollisionRenderObj> test2 = std::shared_ptr<CollisionRenderObj>(new CollisionRenderObj());
    test2->pos.y = 998;
    objects.push_back(test2);
    collisionObjects.push_back(test2);
    renderObjects.push_back(test2);

    std::shared_ptr<CollisionRenderObj> test3 = std::shared_ptr<CollisionRenderObj>(new CollisionRenderObj());
    test3->pos.y = 997;
    objects.push_back(test3);
    collisionObjects.push_back(test3);
    renderObjects.push_back(test3);

    std::shared_ptr<RenderObject> test4 = std::shared_ptr<RenderObject>(new RenderObject());
    test4->pos.y = 996;
    test4->indexCount = 1040;
    objects.push_back(test4);
    renderObjects.push_back(test4);

    std::shared_ptr<LanderObj> lander = std::shared_ptr<LanderObj>(new LanderObj());
    lander->pos.y = 995;
    lander->indexCount = 2010;
    objects.push_back(lander);
    collisionObjects.push_back(lander);
    renderObjects.push_back(lander);

    std::shared_ptr<AsteroidObj> asteroid = std::shared_ptr<AsteroidObj>(new AsteroidObj());
    asteroid->pos.y = 994;
    asteroid->indexCount = 10;
    objects.push_back(asteroid);
    collisionObjects.push_back(asteroid);
    renderObjects.push_back(asteroid);
}

TEST_CASE("MyScene_Objects_Shareing") {
    initObjects();
    CHECK(objects.at(0)->pos.y == 999);
    CHECK(objects.at(2)->pos.y == 997);
    CHECK(renderObjects.at(3)->indexCount == 1040);
    CHECK(collisionObjects.at(2)->indexCount == 2010);
    CHECK(collisionObjects.at(3)->indexCount == 10);
    CHECK(collisionObjects.at(3)->pos.y == 994);
}
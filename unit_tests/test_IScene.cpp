#include <catch.hpp>
#include "dmn_myScene.h"

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

void initObjects(){   //defining the materials in here, should have a seperate materials class?
    std::shared_ptr<RenderObject> test1 = std::shared_ptr<RenderObject>(new RenderObject());
    test1->pos.y = 999;
    objects.push_back(test1);
    renderObjects.push_back(test1);

    std::shared_ptr<CollisionRenderObj> test2 = std::shared_ptr<CollisionRenderObj>(new CollisionRenderObj());
    test2->boxShape = true;
    objects.push_back(test2);
    collisionObjects.push_back(test2);
    renderObjects.push_back(test2);

    std::shared_ptr<CollisionRenderObj> test3 = std::shared_ptr<CollisionRenderObj>(new CollisionRenderObj());
    test3->pos.z = 888;
    test3->concaveTriangleShape = true;
    objects.push_back(test3);
    collisionObjects.push_back(test3);
    renderObjects.push_back(test3);

    std::shared_ptr<RenderObject> test4 = std::shared_ptr<RenderObject>(new RenderObject());
    test4->indexCount = 1040;
    objects.push_back(test4);
    renderObjects.push_back(test4);
}

TEST_CASE("MyScene_Objects_Shareing") {
    initObjects();
    CHECK(collisionObjects.at(0)->boxShape);
    CHECK(collisionObjects.at(1)->concaveTriangleShape);
    CHECK(objects.at(0)->pos.y == 999);
    CHECK(objects.at(2)->pos.z == 888);
    CHECK(renderObjects.at(3)->indexCount == 1040);
    CHECK(renderObjects.at(3)->indexCount == 1040);
}
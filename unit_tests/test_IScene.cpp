#include <catch.hpp>
#include "dmn_myScene.h"

TEST_CASE("MyScene_Construct") {
    Domain::IScene* p_newScene = new Domain::MyScene();
    CHECK(p_newScene);
    CHECK(p_newScene);
    delete p_newScene;
}
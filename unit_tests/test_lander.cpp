#include <catch.hpp>
#include "world_physics.h"
#include "obj_lander.h"
#include "obj_landingSite.h"

//#include "mediator.h"
//Mediator mediator = Mediator();
//std::shared_ptr<LandingSiteObj> p_landingSite = std::shared_ptr<LandingSiteObj>(new LandingSiteObj(&mediator));

std::shared_ptr<LanderObj> p_lander = std::shared_ptr<LanderObj>(new LanderObj());

TEST_CASE("SubmitRetrieveBoostCommand") {
    p_lander->addImpulseToLanderQueue(1.0f, 1, 0, 0, false);
    LanderBoostCommand boost = p_lander->popLanderImpulseQueue();
    CHECK(boost.vector.x == 1);
}
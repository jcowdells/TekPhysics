#pragma once

#include "../tekgl.h"
#include "../core/exception.h"
#include "body.h"
#include "../core/vector.h"

exception tekTestForCollision(TekBody* body_a, TekBody* body_b, flag* collision, Vector* contact_manifolds);
exception tekApplyCollision(TekBody* body_a, TekBody* body_b, const Vector* contact_manifolds, float delta_time);
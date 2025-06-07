#pragma once

#include "../tekgl.h"
#include "../core/exception.h"
#include "body.h"

#include <cglm/vec3.h>

typedef struct  TekColliderNode {
    flag type;
    union {
        struct {
            vec3 centre;
            float radius;
            struct TekColliderNode* left;
            struct TekColliderNode* right;
        } node;
        struct {
            uint indices[3];
        } leaf;
    } data;
} TekColliderNode;

typedef TekColliderNode* TekCollider;

#include "collider.h"

#include <stdio.h>

#include "geometry.h"
#include "../core/priorityqueue.h"
#include "../core/bitset.h"
#include "../core/vector.h"

/**
 * Find a sphere that completely encloses a triangle given its vertices.
 * @note Order of triangle vertices is unimportant.
 * @param[in] point_a First point of the triangle.
 * @param[in] point_b Second point of the triangle.
 * @param[in] point_c Third point of the triangle.
 * @param[out] centre The returned centre of the sphere.
 * @param[out] radius A pointer to where the radius of the sphere will be stored.
 */
static void tekCreateBoundingSphere(vec3 point_a, vec3 point_b, vec3 point_c, vec3 centre, float* radius) {
    // centroid of a triangle is the average of all 3 vertices.
    vec3 centroid;
    sumVec3(centroid, point_a, point_b, point_c);
    glm_vec3_scale(centroid, 1.0f / 3.0f, centre);

    float largest_distance = -HUGE_VALF;
    uint largest_index = 0; // in the event they are all the same, just use the first one.
    const float distances[3] = {
        glm_vec3_distance2(centre, point_a),
        glm_vec3_distance2(centre, point_b),
        glm_vec3_distance2(centre, point_c)
    };

    // now determine which point is furthest from the centroid
    for (uint i = 0; i < 3; i++) {
        if (distances[i] > largest_distance) {
            largest_distance = distances[i];
            largest_index = i;
        }
    }

    *radius = sqrtf(distances[largest_index]);
}

/**
 * Find the centre and radius of a sphere that tightly encloses two other spheres.
 * @param[in] centre_a The centre of the first sphere.
 * @param[in] radius_a The radius of the first sphere.
 * @param[in] centre_b The centre of the second sphere.
 * @param[in] radius_b The radius of the second sphere.
 * @param[out] centre The centre of the new sphere.
 * @param[out] radius A pointer to where the radius of the new sphere will be stored.
 */
static void tekCreateCombinedSphere(vec3 centre_a, const float radius_a, vec3 centre_b, const float radius_b, vec3 centre, float* radius) {
    // compute the radius of the new sphere
    // rC = (rA + rB + |AB|) / 2
    const float distance = glm_vec3_distance(centre_a, centre_b);
    *radius = (distance + radius_a + radius_b) * 0.5f;

    // find normalised(AB) using the distance found earlier
    vec3 direction;
    glm_vec3_sub(centre_b, centre_a, direction);
    glm_vec3_scale(direction, 1.0f / distance, direction);

    // compute the centre of the new sphere
    // C = midpoint(farA, farB)
    // where farA and farB are the furthest away points on the surface of each sphere that lie on the line connecting the two centres.
    // farA = A - normalised(AB) * rA
    // farB = B + normalised(AB) * rB
    vec3 far_a, far_b;
    glm_vec3_mulsubs(direction, radius_a, far_a);
    glm_vec3_muladds(direction, radius_b, far_b);
    glm_vec3_add(far_a, far_b, centre);
    glm_vec3_scale(centre, 0.5f, centre);
}

/**
 * Calculate the cost of merging two spheres. Cost is proportional to the volume of the new sphere.
 * @param sphere_a A collider node containing the first sphere.
 * @param sphere_b A collider node containing the second sphere.
 * @return The cost of merging the two spheres.
 */
static double tekFindSphereMergeCost(TekColliderNode* sphere_a, TekColliderNode* sphere_b) {
    const double distance = (double)glm_vec3_distance(sphere_a->centre, sphere_b->centre);
    const double radius = distance + (double)sphere_a->radius + (double)sphere_b->radius;

    // volume = (4/3)pi r^3, but as we are just comparing, we can remove the factor.
    return radius * radius * radius;
}

/**
 * Fill a priority queue with a list of TekColliderNode* pairs.
 * @param body The body for which to generate the initial spheres from.
 * @param spheres A vector to store a list of pointers to all created spheres, indexed by sphere ID.
 * @param queue The queue which to append the generated spheres to.
 * @param sphere_id The next available sphere id that can be used / the number of spheres created. Set to NULL if not needed.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @note If an exception occurs, the caller is responsible for freeing the queue and the vector, by calling free on each item.
 */
static exception tekGenSphereQueue(TekBody* body, Vector* spheres, PriorityQueue* queue, uint* sphere_id) {
    const uint num_triangles = body->num_indices / 3;

    // firstly, generate a sphere for each of the vertices of the mesh that will completely enclose it.
    // will be stored as a TekColliderNode*, as these will eventually be the leaf nodes of the collider tree
    // stored in a vector temporarily for easy indexing and iteration.
    for (uint i = 0; i < num_triangles; i++) {
        TekColliderNode* sphere = (TekColliderNode*)malloc(sizeof(TekColliderNode));
        if (!sphere)
            tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for sphere.");
        tekCreateBoundingSphere(
            body->vertices[body->indices[i * 3]],
            body->vertices[body->indices[i * 3 + 1]],
            body->vertices[body->indices[i * 3 + 2]],
            sphere->centre,
            &sphere->radius
        );
        sphere->type = COLLIDER_LEAF;
        sphere->id = i;
        sphere->data.leaf.indices[0] = body->indices[i * 3];
        sphere->data.leaf.indices[1] = body->indices[i * 3 + 1];
        sphere->data.leaf.indices[2] = body->indices[i * 3 + 2];
        tekChainThrowThen(vectorAddItem(spheres, &sphere), {
            free(sphere);
        });
    }

    // now, loop over all pairs of indices and add to priority queue, ignoring doubles and repeats, e.g. ignore index 1,1 or 0,2 if 2,0 is already in there.
    // this is so we can calculate the cost of merging any two spheres together
    for (uint i = 0; i < num_triangles; i++) {
        for (uint j = 0; j < i; j++) {
            TekColliderNode* sphere_i;
            TekColliderNode* sphere_j;
            vectorGetItem(spheres, i, &sphere_i);
            vectorGetItem(spheres, j, &sphere_j);
            const double merge_cost = tekFindSphereMergeCost(sphere_i, sphere_j);
            TekColliderNode** pair = (TekColliderNode**)malloc(2 * sizeof(TekColliderNode*));
            if (!pair)
                tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory to store pair of spheres.");
            pair[0] = sphere_i;
            pair[1] = sphere_j;
            tekChainThrow(priorityQueueEnqueue(queue, merge_cost, pair));
        }
    }

    // if needed, set sphere id 
    if (sphere_id) *sphere_id = num_triangles;

    return SUCCESS;
}

/// Helper function, will clean up any pointers in priority queue or in the vector.
#define createColliderCleanup() \
struct Sphere** _pair; \
while (priorityQueueDequeue(&sphere_queue, &_pair)) { \
    free(_pair); \
} \
priorityQueueDelete(&sphere_queue); \
for (uint _i = 0; _i < sphere_vector.length; _i++) { \
    void* _free_node; \
    vectorGetItem(&sphere_vector, _i, &_free_node); \
    free(_free_node); \
} \
vectorDelete(&sphere_vector) \

/**
 * Helper function, will print out the collider tree to the terminal.
 * @param node The root node of the tree.
 * @param indent The initial indent (used for recursion, set to 0)
 */
static void tekPrintCollider(const TekColliderNode* node, const uint indent) {
    // recursive algorithm, using in order traversal of tree
    if (!node) {
        for (uint i = 0; i < indent; i++)
            printf("   ");
        printf("NULL_PTR!\n");
    } else if (node->type == COLLIDER_NODE) {
        tekPrintCollider(node->data.node.left, indent + 1);
        for (uint i = 0; i < indent; i++)
            printf("   ");
        printf("X=>\n");
        tekPrintCollider(node->data.node.right, indent + 1);
    } else {
        for (uint i = 0; i < indent; i++)
            printf("   ");
        printf("leaf id=%u\n", node->id);
    }
}

/**
 * Create a collider based on a body. This will take each triangle in the mesh, and construct a binary tree of spheres that surround each triangle. This allows for optimised collision detection by testing simple sphere collisions to see if the expensive triangle collision is required.
 * @param[in] body The body of which to base the collider on.
 * @param[out] collider The outputted collider tree.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception tekCreateCollider(TekBody* body, TekCollider* collider) {
    // firstly, set up our data structures
    PriorityQueue sphere_queue = {};
    priorityQueueCreate(&sphere_queue);

    Vector sphere_vector = {};
    tekChainThrow(vectorCreate(body->num_indices * 2 / 3, sizeof(TekColliderNode*), &sphere_vector));

    // fill up the queue with the initial spheres.
    uint sphere_id = 0;
    tekChainThrowThen(tekGenSphereQueue(body, &sphere_vector, &sphere_queue, &sphere_id), {
        createColliderCleanup();
    });

    // bitset to keep track of which spheres have already been merged together.
    BitSet sphere_bitset = {};
    bitsetCreate(sphere_id * 2, 1, &sphere_bitset);

    TekColliderNode* newest_node = 0;
    TekColliderNode** pair;

    // iterate until there are no pairs spheres left to merge (e.g. have been combined into a single sphere)
    while (priorityQueueDequeue(&sphere_queue, &pair)) {
	// ignore any spheres that have already been merged.
        flag sphere_inactive;
        bitsetGet(&sphere_bitset, pair[0]->id, &sphere_inactive);
        if (sphere_inactive) {
		free(pair);
		continue;
	}
        bitsetGet(&sphere_bitset, pair[1]->id, &sphere_inactive);
        if (sphere_inactive) {
		free(pair);
		continue;
	}

	// update so that both spheres we merge are marked as such.
        bitsetSet(&sphere_bitset, pair[0]->id);
        bitsetSet(&sphere_bitset, pair[1]->id);

	// create a new node in the tree that will store the new sphere.
        TekColliderNode* new_node = (TekColliderNode*)malloc(sizeof(TekColliderNode));
        if (!new_node) {
	    free(pair);
            createColliderCleanup();
            bitsetDelete(&sphere_bitset);
            tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for new collider node.");
        }
        tekCreateCombinedSphere(
            pair[0]->centre, pair[0]->radius,
            pair[1]->centre, pair[1]->radius,
            new_node->centre, &new_node->radius
            );
        new_node->id = sphere_id;
        new_node->type = COLLIDER_NODE;
        new_node->data.node.left = pair[0];
        new_node->data.node.right = pair[1];

        tekChainThrowThen(vectorAddItem(&sphere_vector, &new_node), {
            free(pair);
	    createColliderCleanup();
            bitsetDelete(&sphere_bitset);
        });

        newest_node = new_node;

	// calculate costs of merging the new sphere with all existing spheres.
	// add these to the priority queue
        for (uint i = 0; i < sphere_id; i++) {
            bitsetGet(&sphere_bitset, i, &sphere_inactive);
            if (!sphere_inactive) {
                TekColliderNode* loop_node;
                vectorGetItem(&sphere_vector, i, &loop_node);
                TekColliderNode** new_pair = (TekColliderNode**)malloc(2 * sizeof(TekColliderNode*));
                if (!new_pair) {
		    free(pair);
                    createColliderCleanup();
                    bitsetDelete(&sphere_bitset);
                    tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for new collider node pair.");
                }
                new_pair[0] = new_node;
                new_pair[1] = loop_node;
                const double merge_cost = tekFindSphereMergeCost(new_node, loop_node);
                tekChainThrowThen(priorityQueueEnqueue(&sphere_queue, merge_cost, new_pair), {
                    free(pair);
	 	    createColliderCleanup();
                    bitsetDelete(&sphere_bitset);
                });
            }
        }

        sphere_id = sphere_vector.length;

        free(pair);
    }

    *collider = newest_node;
    createColliderCleanup();
    bitsetDelete(&sphere_bitset);

    return SUCCESS;
}

/**
 * Recursively free a TekColliderNode by traversing its children.
 * @param collider The collider node to free.
 */
static void tekDeleteColliderNode(TekColliderNode* collider) {
    if (collider->type == COLLIDER_NODE) {
	tekDeleteColliderNode(collider->data.node.left);
	tekDeleteColliderNode(collider->data.node.right);
    }
    free(collider);
}

/**
 * Delete a collider by freeing all its data.
 * @param collider The collider to delete.
 */
void tekDeleteCollider(TekCollider* collider) {
    tekDeleteColliderNode(*collider);
    *collider = 0;
}

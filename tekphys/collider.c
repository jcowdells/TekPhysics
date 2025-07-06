#include "collider.h"

#include <stdio.h>
#include <string.h>

#include "geometry.h"
#include "body.h"
#include "../core/vector.h"
#include "cglm/mat3.h"

/**
 * Single-precision symmetric matrix compute eigenvalues (and optionally eigenvectors)\.
 * @param[in] jobz The type of operation, 'N' for only eigenvalues, 'V' for both.
 * @param[in] uplo The triangle (of the matrix) to keep, 'U' for upper, 'L' for lower.
 * @param[in] n The order of the matrix (n x n).
 * @param[in/out] a Used to input the matrix, and to output the eigenvectors.
 * @param[in] lda The leading dimension of the matrix.
 * @param[out] w The outputted array of eigenvalues.
 * @param[in] work The workspace array.
 * @param[in] lwork The size of the workspace array (recommended size is max(1, 3n-1)).
 * @param[out] info The output of the function, 0 on success, <0 on error, >0 on no solution.
 * @note External function, interfaces Fortran subprocedure in LAPACK.
 */
extern void ssyev_(char* jobz, char* uplo, int* n, float* a, int* lda, float* w, float* work, float* lwork, int* info);

/**
 * Compute the eigenvectors and eigenvalues of a symmetric matrix.
 * @param[in] matrix The matrix to compute.
 * @param[in/out] eigenvectors An empty but already allocated array of 3*sizeof(vec3) that will store the eigenvectors.
 * @param[in/out] eigenvalues An empty but already allocated array of 3*sizeof(float) that will store the eigenvalues.
 * @note Uses LAPACK / ssyev_(...) internally.
 */
static exception symmetricMatrixCalculateEigenvectors(mat3 matrix, vec3* eigenvectors, float* eigenvalues) {
    // create copy of matrix as LAPACKE will destroy the input.
    mat3 matrix_copy;
    glm_mat3_copy(matrix, matrix_copy);

    char jobz = 'V'; // compute both eigenvalues and eigenvectors.
    char uplo = 'U'; // use the upper triangle of matrix.
    int n = 3;       // matrix order
    float w[3];      // eigenvalues storage
    float work[8];   // workspace buffer
    int lwork = 8;   // size of aforementioned buffer
    int info;        // the output

    // call Fortran function...
    // took 4 hours of messing with cmakelists.txt to make this work.
    ssyev_(&jobz, &uplo, &n, &matrix_copy, &n, &w, &work, &lwork, &info);
    if (info)
        tekThrow(FAILURE, "Failed to calculate eigenvectors.");

    // now, copy eigenvalues into output array.
    for (uint i = 0; i < 3; i++) {
        glm_vec3_copy(matrix_copy[i], eigenvectors[i]);
    }
    return SUCCESS;
}

struct Triangle {
    vec3 vertices[3];
    vec3 centroid;
    float area;
};

/**
 * Calculate the area of a triangle given three points in space.
 * @param point_a The first point of the triangle.
 * @param point_b The second point of the triangle.
 * @param point_c The third point of the triangle.
 * @return The area of the triangle formed by those three points.
 */
static float tekCalculateTriangleArea(vec3 point_a, vec3 point_b, vec3 point_c) {
    // area = 0.5 * | (b - a) x (c - a) |
    // magnitude of cross product = area of parallelogram between two vectors.
    // area of triangle = half area of parallelogram.
    vec3 b_a, c_a;
    glm_vec3_sub(point_b, point_a, b_a);
    glm_vec3_sub(point_c, point_a, c_a);
    vec3 cross;
    glm_vec3_cross(b_a, c_a, cross);
    return 0.5f * glm_vec3_norm(cross);
}

#define tekCheckIndex(index) \
if (index >= num_vertices) \
tekThrowThen(FAILURE, "Vertex does not exist.", { \
    vectorDelete(triangles); \
}); \

/**
 * Fill a Vector with the data for all triangles of a mesh. This means the vertices and area of each triangle.
 * @param vertices An array of vertices, use body.vertices.
 * @param num_vertices The number of vertices, use body.num_vertices.
 * @param indices An array of indices, use body.indices.
 * @param num_indices The number of indices, use body.num_indices.
 * @param triangles An existing Vector struct, will have vectorCreate(...) called, so you must call vectorDelete(...) after finishing with the Vector.
 * @note 'triangles' is freed if the function fails.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekGenerateTriangleArray(const vec3* vertices, const uint num_vertices, const uint* indices, const uint num_indices, Vector* triangles) {
    tekChainThrow(vectorCreate(num_indices / 3, sizeof(struct Triangle), triangles));
    struct Triangle triangle = {};
    for (uint i = 0; i < num_indices; i += 3) {
        tekCheckIndex(indices[i]);
        glm_vec3_copy(vertices[indices[i]], triangle.vertices[0]);
        tekCheckIndex(indices[i + 1]);
        glm_vec3_copy(vertices[indices[i + 1]], triangle.vertices[1]);
        tekCheckIndex(indices[i + 2]);
        glm_vec3_copy(vertices[indices[i + 2]], triangle.vertices[2]);
        triangle.area = tekCalculateTriangleArea(triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]);
        vec3 centroid;
        glm_vec3_zero(centroid);
        sumVec3(
            centroid,
            triangle.vertices[0],
            triangle.vertices[1],
            triangle.vertices[2]
        );
        glm_vec3_scale(centroid, 1.0f / 3.0f, triangle.centroid);
        tekChainThrowThen(vectorAddItem(triangles, &triangle), {
            vectorDelete(triangles);
        });
    }
    return SUCCESS;
}

static void tekCalculateConvexHullMean(const Vector* triangles, const uint* indices, const uint num_indices, vec3 mean) {
    // https://www.cs.unc.edu/techreports/96-013.pdf
    // mean of convex hull = (1/(6n)) * SUM((1/area)(a + b + c))
    // where n = num triangles
    // a, b, c = vertices of each triangle.

    // perform summation
    glm_vec3_zero(mean);
    for (uint i = 0; i < num_indices; i++) {
        vec3 sum;
        glm_vec3_zero(sum);
        const struct Triangle* triangle;
        vectorGetItemPtr(triangles, indices[i], &triangle);
        sumVec3(
            sum,
            triangle->vertices[0],
            triangle->vertices[1],
            triangle->vertices[2]
        );
        glm_vec3_muladds(sum, 1.0f / triangle->area, mean);
    }

    const float num_triangles = (float)(6 * triangles->length);
    glm_vec3_scale(mean, 1.0f / num_triangles, mean);
}

static void tekCalculateCovarianceMatrix(const Vector* triangles, const uint* indices, const uint num_indices, const vec3 mean, mat3 covariance) {
    // https://www.cs.unc.edu/techreports/96-013.pdf
    glm_mat3_zero(covariance);
    for (uint i = 0; i < num_indices; i++) {
        struct Triangle* triangle;
        vectorGetItemPtr(triangles, indices[i], &triangle);
        for (uint j = 0; j < 3; j++) {
            for (uint k = 0; k < 3; k++) {
                const float pj = triangle->vertices[0][j] - mean[0];
                const float pk = triangle->vertices[0][k] - mean[0];
                const float qj = triangle->vertices[1][j] - mean[1];
                const float qk = triangle->vertices[1][k] - mean[1];
                const float rj = triangle->vertices[2][j] - mean[2];
                const float rk = triangle->vertices[2][k] - mean[2];
                covariance[k][j] += triangle->area * ((pj + qj + rj) * (pk + qk + rk) + pj * pk + qj * qk + rj * rk);
            }
        }
    }
    glm_mat3_scale(covariance, 1.0f / (float)triangles->length);
}

static void tekFindProjections(const Vector* triangles, const uint* indices, const uint num_indices, vec3 axis, float* min_p, float* max_p) {
    float min_proj = INFINITY, max_proj = -INFINITY;
    for (uint i = 0; i < num_indices; i++) {
        const struct Triangle* triangle;
        vectorGetItemPtr(triangles, indices[i], &triangle);
        for (uint j = 0; j < 3; j++) {
            const float proj = glm_vec3_dot(triangle->vertices[j], axis);
            if (proj < min_proj) {
                min_proj = proj;
            }
            if (proj > max_proj) {
                max_proj = proj;
            }
        }
    }
    *min_p = min_proj;
    *max_p = max_proj;
}

static exception tekCreateOBB(const Vector* triangles, const uint* indices, const uint num_indices, struct OBB* obb) {
    vec3 mean;
    tekCalculateConvexHullMean(triangles, indices, num_indices, mean);
    mat3 covariance;
    tekCalculateCovarianceMatrix(triangles, indices, num_indices, mean, covariance);
    vec3 eigenvectors[3];
    float eigenvalues[3];
    tekChainThrow(symmetricMatrixCalculateEigenvectors(covariance, eigenvectors, eigenvalues));

    vec3 centre = {0.0f, 0.0f, 0.0f};
    for (uint i = 0; i < 3; i++) {
        float min_proj, max_proj;
        tekFindProjections(triangles, indices, num_indices, eigenvectors[i], &min_proj, &max_proj);
        const float half_extent = 0.5f * (max_proj - min_proj);
        const float centre_proj = 0.5f * (max_proj + min_proj);
        glm_vec3_muladds(eigenvectors[i], centre_proj, centre);
        glm_vec3_copy(eigenvectors[i], obb->axes[i]);
        obb->half_extents[i] = half_extent;
    }
    glm_vec3_copy(centre, obb->centre);

    printf("Created OBB: %f %f %f\n", EXPAND_VEC3(centre));

    return SUCCESS;
}

static exception tekCreateColliderNode(const flag type, const uint id, const Vector* triangles, uint* indices, const uint num_indices, TekColliderNode** collider_node) {
    *collider_node = (TekColliderNode*)malloc(sizeof(TekColliderNode));
    if (!*collider_node)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for collider node.");
    tekChainThrowThen(tekCreateOBB(triangles, indices, num_indices, &(*collider_node)->obb), {
        free(*collider_node);
        *collider_node = 0;
    });
    (*collider_node)->id = id;
    (*collider_node)->type = type;
    if (type == COLLIDER_NODE) {
        (*collider_node)->data.node.indices = indices;
        (*collider_node)->data.node.num_indices = num_indices;
        (*collider_node)->data.node.left = 0;
        (*collider_node)->data.node.right = 0;
    }
    return SUCCESS;
}

#define tekColliderCleanup() \
{ \
vectorDelete(&triangles); \
vectorDelete(&collider_stack); \
tekDeleteCollider(collider); \
} \

static void tekPrintCollider(TekColliderNode* collider, uint indent) {
    
    if (collider->type == COLLIDER_NODE) {
        tekPrintCollider(collider->data.node.left, indent + 1);
        for (uint i = 0; i < indent; i++) {
            printf("  ");
        }
        printf("%u > (%f %f %f)\n", collider->id, EXPAND_VEC3(collider->obb.half_extents));
        tekPrintCollider(collider->data.node.right, indent + 1);
    } else if (collider->type == COLLIDER_LEAF){
        for (uint i = 0; i < indent; i++) {
            printf("  ");
        }
        printf("Leaf %u : %u triangle(s)", collider->id, collider->data.leaf.num_vertices / 3);
        printf(" %f %f %f\n", EXPAND_VEC3(collider->obb.half_extents));
    }
}

exception tekCreateCollider(TekBody* body, TekCollider* collider) {
    Vector collider_stack = {};
    tekChainThrow(vectorCreate(16, sizeof(TekColliderNode*), &collider_stack));

    Vector triangles = {};
    tekChainThrowThen(tekGenerateTriangleArray(body->vertices, body->num_vertices, body->indices, body->num_indices, &triangles), {
        vectorDelete(&collider_stack);
    });

    TekColliderNode* collider_node;
    uint node_id = 0;
    uint* all_indices = (uint*)malloc(triangles.length * sizeof(uint));
    if (!all_indices) {
        vectorDelete(&triangles);
        vectorDelete(&collider_stack);
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for indices.");
    }
    for (uint i = 0; i < triangles.length; i++) {
        all_indices[i] = i;
    }

    tekChainThrowThen(tekCreateColliderNode(COLLIDER_NODE, node_id++, &triangles, all_indices, triangles.length, &collider_node), {
        vectorDelete(&triangles);
        vectorDelete(&collider_stack);
    });

    *collider = collider_node;
    tekChainThrowThen(vectorAddItem(&collider_stack, &collider_node), {
        tekColliderCleanup();
    });

    while (vectorPopItem(&collider_stack, &collider_node)) {
        uint* indices_buffer = (uint*)malloc(collider_node->data.node.num_indices * sizeof(uint));
        if (!indices_buffer) {
            tekColliderCleanup();
            tekThrow(MEMORY_EXCEPTION, "Failed to allocate buffer for indices.");
        }
        uint axis = 3;
        uint num_left = 0, num_right = 0;
        for (uint i = 0; i < 3; i++) {
            num_left = 0;
            num_right = 0;
            const uint num_indices = collider_node->data.node.num_indices;
            for (uint j = 0; j < num_indices; j++) {
                const uint index = collider_node->data.node.indices[j];
                const struct Triangle* triangle;
                vectorGetItemPtr(&triangles, index, &triangle);
                vec3 delta;
                glm_vec3_sub(triangle->centroid, collider_node->obb.centre, delta);
                const float side = glm_vec3_dot(delta, collider_node->obb.axes[i]);
                if (side < 0) {
                    indices_buffer[num_left] = index;
                    printf("Writing at %u/%u\n", num_left, num_indices);
                    num_left++;
                } else {
                    indices_buffer[num_indices - num_right - 1] = index;
                    printf("Writing at %u/%u\n", num_indices - num_right - 1, num_indices);
                    num_right++;
                }
                printf("l:r = %u:%u\n", num_left, num_right);
            }
            for (uint j = 0; j < num_indices; j++) {
                printf("indices_buffer[%u] = %u\n", j, indices_buffer[j]);
            }
            if ((num_left != 0) && (num_right != 0)) {
                axis = i;
                break;
            }
        }
        if (axis == 3) {
            // indivisible - overwrite current node to be a leaf
            printf("Indivisible node with %u triangles, centre=%f,%f,%f.", num_left + num_right, EXPAND_VEC3(collider_node->obb.centre));
            collider_node->type = COLLIDER_LEAF;
            const uint num_indices = collider_node->data.node.num_indices;
            const uint num_vertices = num_indices * 3;
            vec3* vertices = (vec3*)malloc(num_vertices * sizeof(vec3));
            if (!vertices) {
                tekColliderCleanup();
                tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for vertices.");
            }
            for (uint i = 0; i < num_indices; i++) {
                const struct Triangle* triangle;
                vectorGetItemPtr(&triangles, i, &triangle);
                glm_vec3_copy(triangle->vertices[0], vertices[i * 3]);
                glm_vec3_copy(triangle->vertices[1], vertices[i * 3 + 1]);
                glm_vec3_copy(triangle->vertices[2], vertices[i * 3 + 2]);
            }
            collider_node->data.leaf.num_vertices = num_vertices;
            collider_node->data.leaf.vertices = vertices;
        } else {
            // divisible, create two new nodes and set as children.
            for (uint right = 0; right < 2; right++) {
                uint* indices = right ? &indices_buffer[num_left] : indices_buffer;
                const uint num_indices = right ? num_right : num_left;
                TekColliderNode* new_collider_node;
                tekChainThrowThen(tekCreateColliderNode(COLLIDER_NODE, node_id++, &triangles, indices, num_indices, &new_collider_node), {
                    tekColliderCleanup();
                });
                if (right) {
                    collider_node->data.node.right = new_collider_node;
                } else {
                    collider_node->data.node.left = new_collider_node;
                }
                tekChainThrowThen(vectorAddItem(&collider_stack, &new_collider_node), {
                    tekColliderCleanup();
                });
            }
        }
    }


    tekPrintCollider(*collider, 0);

    return SUCCESS;
}

void tekDeleteCollider(TekCollider* collider) {

}

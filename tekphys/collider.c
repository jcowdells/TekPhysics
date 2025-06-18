#include "collider.h"

#include "geometry.h"
#include "../core/vector.h"
#include "cglm/mat3.h"

struct Triangle {
    vec3 vertices[3];
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

static exception tekGenerateTriangleArray(const vec3* vertices, const uint num_vertices, const uint* indices, const uint num_indices, Vector* triangles) {
    tekChainThrow(vectorCreate(num_indices / 3, sizeof(struct Triangle), triangles));
    struct Triangle triangle = {};
    for (uint i = 0; i < num_indices; i += 3) {
        tekCheckIndex(indices[i]);
        glm_vec3_copy(vertices[indices[i]], triangle.vertices[0]);
        tekCheckIndex(indices[i + 1]);
        glm_vec3_copy(vertices[indices[i + 1]], triangle.vertices[0]);
        tekCheckIndex(indices[i + 2]);
        glm_vec3_copy(vertices[indices[i + 2]], triangle.vertices[0]);
        triangle.area = tekCalculateTriangleArea(triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]);
        tekChainThrow(vectorAddItem(triangles, &triangle));
    }
    return SUCCESS;
}

static void tekCalculateConvexHullMean(const Vector* triangles, vec3 mean) {
    // https://www.cs.unc.edu/techreports/96-013.pdf
    // mean of convex hull = (1/(6n)) * SUM((1/area)(a + b + c))
    // where n = num triangles
    // a, b, c = vertices of each triangle.

    // perform summation
    for (uint i = 0; i < triangles->length; i++) {
        vec3 sum;
        const struct Triangle* triangle;
        vectorGetItemPtr(triangles, i, &triangle);
        sumVec3(
            sum,
            triangle->vertices[0],
            triangle->vertices[1],
            triangle->vertices[2]
        );
        glm_vec3_muladds(sum, 1.0f / triangle->area, mean);
    }

    const float num_triangles = (float)(2 * triangles->length);
    glm_vec3_scale(mean, 1.0f / num_triangles, mean);
}

static void tekCalculateCovarianceMatrix(const Vector* triangles, const vec3 mean, mat3 covariance) {
    // https://www.cs.unc.edu/techreports/96-013.pdf
    for (uint i = 0; i < triangles->length; i++) {
        struct Triangle* triangle;
        vectorGetItemPtr(triangles, i, &triangle);
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


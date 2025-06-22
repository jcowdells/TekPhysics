#include "collider.h"

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
        tekChainThrowThen(vectorAddItem(triangles, &triangle), {
            vectorDelete(triangles);
        });
    }
    return SUCCESS;
}

static void tekCalculateConvexHullMean(const Vector* triangles, vec3 mean) {
    // https://www.cs.unc.edu/techreports/96-013.pdf
    // mean of convex hull = (1/(6n)) * SUM((1/area)(a + b + c))
    // where n = num triangles
    // a, b, c = vertices of each triangle.

    // perform summation
    glm_vec3_zero(mean);
    for (uint i = 0; i < triangles->length; i++) {
        vec3 sum;
        glm_vec3_zero(sum);
        const struct Triangle* triangle;
        vectorGetItemPtr(triangles, i, &triangle);
        sumVec3(
            sum,
            triangle->vertices[0],
            triangle->vertices[1],
            triangle->vertices[2]
        );
        glm_vec3_muladds(sum, 1.0f / triangle->area, mean);
        printf("Sum = %f %f %f Area = %f\n", EXPAND_VEC3(sum), triangle->area);
    }

    const float num_triangles = (float)(6 * triangles->length);
    glm_vec3_scale(mean, 1.0f / num_triangles, mean);
}

static void tekCalculateCovarianceMatrix(const Vector* triangles, const vec3 mean, mat3 covariance) {
    // https://www.cs.unc.edu/techreports/96-013.pdf
    glm_mat3_zero(covariance);
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

exception tekCreateCollider(TekBody* body, TekCollider* collider) {
    Vector triangles = {};
    tekChainThrow(tekGenerateTriangleArray(body->vertices, body->num_vertices, body->indices, body->num_indices, &triangles));
    vec3 mean;
    tekCalculateConvexHullMean(&triangles, mean);
    printf("mean = %f %f %f\n", EXPAND_VEC3(mean));
    mat3 covariance;
    tekCalculateCovarianceMatrix(&triangles, mean, covariance);
    for (uint i = 0; i < 3; i++) {
        for (uint j = 0; j < 3; j++) {
            printf("%f ", covariance[i][j]);
        }
        printf("\n");
    }
    vec3 eigenvectors[3];
    float eigenvalues[3];
    tekChainThrow(symmetricMatrixCalculateEigenvectors(covariance, eigenvectors, eigenvalues));
    for (uint i = 0; i < 3; i++) {
        printf("eigenvector: %f %f %f\neigenvalue: %f\n", EXPAND_VEC3(eigenvectors[i]), eigenvalues[i]);
    }

    return SUCCESS;
}

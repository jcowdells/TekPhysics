#include "collider.h"

#include <stdio.h>
#include <string.h>

#include <cglm/vec3-ext.h>
#include <cglm/mat3.h>

#include "geometry.h"
#include "body.h"
#include "engine.h"
#include "../tekgl/manager.h"

#define LEFT  0
#define RIGHT 1

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
 * Single-precision compute solution to a real system of linear equations.
 * @param n[in] The number of linear equations.
 * @param nrhs[in] The number of right hand sides.
 * @param a[in,out] The NxN coefficient matrix A.
 * @param lda[in] The leading dimension of the array A.
 * @param ipiv[out] The pivot indices that define the permutation matrix P.
 * @param b[in,out] The NxNRHS matrix B. Also outputs the NxNRHS solution matrix X.
 * @param ldb[in] The leading dimension of the array B.
 * @param info[out] The output of the function, 0 on success, <0 on error, >0 on no solution.
 * @note External function, interfaces Fortran subprocedure in LAPACK.
 */
extern void sgesv_(int* n, int* nrhs, float* a, int* lda, int* ipiv, float* b, int* ldb, int* info);

/**
 * Compute the eigenvectors and eigenvalues of a symmetric matrix.
 * @param[in] matrix The matrix to compute.
 * @param[in/out] eigenvectors An empty but already allocated array of 3*sizeof(vec3) that will store the eigenvectors.
 * @param[in/out] eigenvalues An empty but already allocated array of 3*sizeof(float) that will store the eigenvalues.
 * @note Uses LAPACK / ssyev_(...) internally.
 */
static exception symmetricMatrixCalculateEigenvectors(mat3 matrix, vec3 eigenvectors[3], float eigenvalues[3]) {
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
        eigenvalues[i] = w[i];
    }
    return SUCCESS;
}

static exception solveSimultaneous3Vec3(vec3 lhs_vectors[3], vec3 rhs_vector, vec3 result) {
    // copy lhs and rhs vectors into a buffer so it doesn't get overwritten by LAPACK
    vec3 lhs_buffer[3];
    memcpy(lhs_buffer, lhs_vectors, 3 * sizeof(vec3));
    vec3 rhs_buffer;
    glm_vec3_copy(rhs_vector, rhs_buffer);

    int n = 3; // 3 lhs vectors
    int nrhs = 1; // 1 rhs vector
    int lda = 3; // 3 components in lhs vectors
    int ldb = 3; // 3 components in rhs vector
    int info; // return value
    int ipiv[3]; // ???

    sgesv_(&n, &nrhs, lhs_buffer, &lda, &ipiv, rhs_buffer, &ldb, &info);
    if (info)
        tekThrow(FAILURE, "Failed to solve simultaneous equation system.");

    // result should now be stored in the rhs buffer, return the answer
    glm_vec3_copy(rhs_buffer, result);

    return SUCCESS;
}

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
    // 1 triangle : 3 vertices, so triangle list is a third of the size of vertices array.
    tekChainThrow(vectorCreate(num_indices / 3, sizeof(struct Triangle), triangles));

    // temp struct to fill with triangle data to copy into the vector.
    struct Triangle triangle = {};
    for (uint i = 0; i < num_indices; i += 3) {
        // copy each vertex into the triangle struct.
        tekCheckIndex(indices[i]);
        glm_vec3_copy(vertices[indices[i]], triangle.vertices[0]);
        tekCheckIndex(indices[i + 1]);
        glm_vec3_copy(vertices[indices[i + 1]], triangle.vertices[1]);
        tekCheckIndex(indices[i + 2]);
        glm_vec3_copy(vertices[indices[i + 2]], triangle.vertices[2]);

        triangle.area = tekCalculateTriangleArea(triangle.vertices[0], triangle.vertices[1], triangle.vertices[2]);

        // centroid = (sum of vertices) / 3
        vec3 centroid;
        glm_vec3_zero(centroid);
        sumVec3(
            centroid,
            triangle.vertices[0],
            triangle.vertices[1],
            triangle.vertices[2]
        );
        glm_vec3_scale(centroid, 1.0f / 3.0f, triangle.centroid);

        // add to vector
        tekChainThrowThen(vectorAddItem(triangles, &triangle), {
            vectorDelete(triangles);
        });
    }
    return SUCCESS;
}

/**
 * Calculate the mean point of a set of triangles interpreted as a convex hull.
 * @param[in] triangles A vector containing a list of triangles (3 vertices, centroid and area).
 * @param[in] indices An array of indices that determine which triangles from the triangles vector will be used in the calculation.
 * @param[in] num_indices The number of indices contained in the array.
 * @param[out] mean The calculated mean of the convex hull.
 */
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

        // (a + b + c)
        sumVec3(
            sum,
            triangle->vertices[0],
            triangle->vertices[1],
            triangle->vertices[2]
        );

        // SUM(1 / area)(a + b + c)
        glm_vec3_muladds(sum, 1.0f / triangle->area, mean);
    }

    // divide by 6n to complete calculation
    const float num_triangles = (float)(6 * triangles->length);
    glm_vec3_scale(mean, 1.0f / num_triangles, mean);
}

/**
 * Calculate the covariance matrix of a convex hull. This is a statistical measure of the spread of "matter" in the rigidbody, or like the standard deviation in 3 axes.
 * @param[in] triangles A vector of triangle data (1 triangle = 3 vertices, centroid and area).
 * @param[in] indices An array of indices that determine which triangles are used from the vector.
 * @param[in] num_indices The length of the indices array.
 * @param[in] mean The mean point of the convex hull, calculated using \ref tekCalculateConvexHullMean
 * @param[out] covariance The calculated covariance matrix of the convex hull.
 */
static void tekCalculateCovarianceMatrix(const Vector* triangles, const uint* indices, const uint num_indices, const vec3 mean, mat3 covariance) {
    // https://www.cs.unc.edu/techreports/96-013.pdf
    // Covariance[x, y] = (1/n) * SUM 1->n(area[i] * ((am[x] + bm[x] + cm[x]) * (am[y] + bm[y] + cm[y]) + am[i]am[j] + bm[i]bm[j] + cm[i]cm[j]))
    // a = a - mean, b = b - mean, c = c - mean
    // a, b, c = vertices of triangle
    // x, y are row and column of covariance matrix in range 0..2
    glm_mat3_zero(covariance);
    for (uint i = 0; i < num_indices; i++) {
        struct Triangle* triangle;
        vectorGetItemPtr(triangles, indices[i], &triangle);
        for (uint j = 0; j < 3; j++) {
            for (uint k = 0; k < 3; k++) {
                // calculate vertex - mean as variable for easier reading
                const float pj = triangle->vertices[0][j] - mean[0];
                const float pk = triangle->vertices[0][k] - mean[0];
                const float qj = triangle->vertices[1][j] - mean[1];
                const float qk = triangle->vertices[1][k] - mean[1];
                const float rj = triangle->vertices[2][j] - mean[2];
                const float rk = triangle->vertices[2][k] - mean[2];
                // perform calculation for each element of matrix
                covariance[k][j] += triangle->area * ((pj + qj + rj) * (pk + qk + rk) + pj * pk + qj * qk + rj * rk);
            }
        }
    }

    // divide by number of triangles to complete the calculation.
    glm_mat3_scale(covariance, 1.0f / (float)triangles->length);
}

/**
 * Find the minimum and maximum point of a set of triangles projected onto a single axis. Used to find the half extents of an OBB.
 * @param[in] triangles A vector containing the triangles of a mesh.
 * @param[in] indices An array containing the indices of the triangles that should be included in the calculation.
 * @param[in] num_indices The length of the indices array.
 * @param[in] axis The axis to project along. Should be one of the eigenvectors of the covariance matrix.
 * @param[out] min_p A pointer to where the minimum projection should be stored.
 * @param[out] max_p A pointer to where the maximum projection should be stored.
 */
static void tekFindProjections(const Vector* triangles, const uint* indices, const uint num_indices, vec3 axis, float* min_p, float* max_p) {
    float min_proj = INFINITY, max_proj = -INFINITY;
    for (uint i = 0; i < num_indices; i++) {
        const struct Triangle* triangle;
        vectorGetItemPtr(triangles, indices[i], &triangle);
        for (uint j = 0; j < 3; j++) {
            // dot product = find the projection of point along the axis.
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

/**
 * Create an OBB based on a set of triangles. This will be the smallest rectangular prism that contains all points of the triangles.
 * @param[in] triangles A vector containing all the triangles of a mesh.
 * @param[in] indices An array containing the indices of triangles which should be used in the calculations.
 * @param[in] num_indices The number of indices that are in the array.
 * @param[out] obb The obb struct that will be filled with the calculated data.
 * @throws FAILURE if the calculation of eigenvectors fails.
 */
static exception tekCreateOBB(const Vector* triangles, const uint* indices, const uint num_indices, struct OBB* obb) {
    // calculate mean and covariance of the set of triangles. This represents the central tendency and spread of the points
    vec3 mean;
    tekCalculateConvexHullMean(triangles, indices, num_indices, mean);
    mat3 covariance;
    tekCalculateCovarianceMatrix(triangles, indices, num_indices, mean, covariance);

    // using this, we can find the eigenvectors which represent three orthogonal vectors of greatest spread.
    // these eigenvectors are used to construct the OBB by producing a face perpendicular to each eigenvector.
    vec3 eigenvectors[3];
    float eigenvalues[3];
    tekChainThrow(symmetricMatrixCalculateEigenvectors(covariance, eigenvectors, eigenvalues));

    // iterate over each eigenvector to find the centre of the OBB and the half extents
    // the half extent is the shortest distance between the centre and one of the faces (2 * half extent = full extent = height of OBB in one axis)
    vec3 centre = {0.0f, 0.0f, 0.0f};
    for (uint i = 0; i < 3; i++) {
        float min_proj, max_proj;
        tekFindProjections(triangles, indices, num_indices, eigenvectors[i], &min_proj, &max_proj);
        const float half_extent = 0.5f * (max_proj - min_proj);
        const float centre_proj = 0.5f * (max_proj + min_proj);
        glm_vec3_muladds(eigenvectors[i], centre_proj, centre);
        glm_vec3_copy(eigenvectors[i], obb->axes[i]);
        glm_vec3_copy(eigenvectors[i], obb->w_axes[i]);
        obb->half_extents[i] = half_extent;
        obb->w_half_extents[i] = half_extent;
    }

    glm_vec3_copy(centre, obb->centre);
    glm_vec3_copy(centre, obb->w_centre);

    return SUCCESS;
}

/**
 * Create a collider node struct given some information. Mostly just a helper function to create the internal OBB and set some values rather than copy-pasting a 10 line statement.
 * @param[in] type The type of collider node, should be either COLLIDER_NODE or COLLIDER_LEAF.
 * @param[in] id The id of the collider node (unused for now, may be needed).
 * @param[in] triangles A vector of triangles that make up a mesh (triangle = 3 vertices, centroid and area).
 * @param[in] indices An array of indices specifying which triangles should be used from the vector.
 * @param[in] num_indices The number of indices in the array.
 * @param[out] collider_node The collider node that was produced. Should point to an empty pointer that will be set to the memory address where the collider node was creatd at.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws FAILURE if there is an error while calculating eigenvectors.
 */
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

    // setting left & right pointers to NULL, in case someone tries to dereference them with junk in there.
    (*collider_node)->indices = indices;
    (*collider_node)->num_indices = num_indices;
    if (type == COLLIDER_NODE) {
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

/**
 * Helper function to print out a collider tree.
 * @param collider The collider structure to print out.
 * @param indent The indent to print at (set to 0 if calling this, recursive calls set to indent + 1)
 */
static void tekPrintCollider(TekColliderNode* collider, uint indent) {
    if (collider->type == COLLIDER_NODE) {
        // using in order traversal, prints bottom left item of tree as the first line, and branches from there.
        tekPrintCollider(collider->data.node.left, indent + 1);
        for (uint i = 0; i < indent; i++) {
            printf("  ");
        }
        printf("%u > (%f %f %f)\n", collider->id, EXPAND_VEC3(collider->obb.half_extents));
        tekPrintCollider(collider->data.node.right, indent + 1);
    } else if (collider->type == COLLIDER_LEAF){
        // if leaf node, stop recursing and print something.
        for (uint i = 0; i < indent; i++) {
            printf("  ");
        }
        printf("Leaf %u : %u triangle(s)", collider->id, collider->data.leaf.num_vertices / 3);
        printf(" %f %f %f\n", EXPAND_VEC3(collider->obb.half_extents));
    }
}

/**
 * Create a collider structure given a body. The body must be initialised and contain the vertex and index data for the mesh.
 * @param[in] body The body to calculate the collider for.
 * @param[out] collider A pointer to where the collider structure pointer should be written to.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws FAILURE if there was an exception during calculations.
 */
exception tekCreateCollider(const TekBody* body, TekCollider* collider) {
    // just set up some vectors and whatever
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

    // bogus code, basically just tell the create collider node function to use every index in the triangle vector
    for (uint i = 0; i < triangles.length; i++) {
        all_indices[i] = i;
    }

    tekChainThrowThen(tekCreateColliderNode(COLLIDER_NODE, node_id++, &triangles, all_indices, triangles.length, &collider_node), {
        vectorDelete(&triangles);
        vectorDelete(&collider_stack);
    });

    // add initial node to the stack so we can start traversing
    *collider = collider_node;
    tekChainThrowThen(vectorAddItem(&collider_stack, &collider_node), {
        tekColliderCleanup();
    });

    // TODO: remove some debugging prints
    while (vectorPopItem(&collider_stack, &collider_node)) {
        // create an indices buffer, this will organise indices into left and right of the dividing axis.
        uint* indices_buffer = (uint*)malloc(collider_node->num_indices * sizeof(uint));
        if (!indices_buffer) {
            tekColliderCleanup();
            tekThrow(MEMORY_EXCEPTION, "Failed to allocate buffer for indices.");
        }

        // starting with an impossible axis (3), so if it doesn't change we can tell that the polygons are indivisible.
        uint axis = 3;
        uint num_left = 0, num_right = 0;

        // check each axis and count the number of triangles left and right of the dividing axis.
        for (uint i = 0; i < 3; i++) {
            num_left = 0;
            num_right = 0;
            const uint num_indices = collider_node->num_indices;
            for (uint j = 0; j < num_indices; j++) {
                const uint index = collider_node->indices[j];
                const struct Triangle* triangle;
                vectorGetItemPtr(&triangles, index, &triangle);
                vec3 delta;
                glm_vec3_sub(triangle->centroid, collider_node->obb.centre, delta);
                const float side = glm_vec3_dot(delta, collider_node->obb.axes[i]);
                if (side < 0) {
                    indices_buffer[num_left] = index;
                    num_left++;
                } else {
                    indices_buffer[num_indices - num_right - 1] = index;
                    num_right++;
                }
            }
            // once a divisible axis is found (e.g. not all on the one side) then stop.
            if ((num_left != 0) && (num_right != 0)) {
                axis = i;
                break;
            }
        }
        if (axis == 3) {
            // indivisible - overwrite current node to be a leaf
            collider_node->type = COLLIDER_LEAF;
            const uint num_indices = collider_node->num_indices;
            const uint num_vertices = num_indices * 3;
            vec3* vertices = (vec3*)malloc(num_vertices * sizeof(vec3));
            if (!vertices) {
                tekColliderCleanup();
                tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for vertices.");
            }
            vec3* w_vertices = (vec3*)malloc(num_vertices * sizeof(vec3));
            if (!w_vertices) {
                tekColliderCleanup();
                tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for world space vertices.");
            }
            for (uint i = 0; i < num_indices; i++) {
                const struct Triangle* triangle;
                const uint index = collider_node->indices[i];
                vectorGetItemPtr(&triangles, index, &triangle);
                glm_vec3_copy(triangle->vertices[0], vertices[i * 3]);
                glm_vec3_copy(triangle->vertices[1], vertices[i * 3 + 1]);
                glm_vec3_copy(triangle->vertices[2], vertices[i * 3 + 2]);
                for (uint j = 0; j < 3; j++) {
                    glm_vec3_copy(vertices[i * 3 + j], w_vertices[i * 3 + j]);
                }
            }

            collider_node->data.leaf.num_vertices = num_vertices;
            collider_node->data.leaf.vertices = vertices;
            collider_node->data.leaf.w_vertices = w_vertices;
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

    return SUCCESS;
}

/**
 * Delete a collider node
 * @param collider_node The pointer to be freed.
 * @param handedness Whether the node was a left or right child (use LEFT or RIGHT, assume LEFT for root node).
 */
static void tekDeleteColliderNode(TekColliderNode* collider_node, const flag handedness) {
    if (collider_node->type == COLLIDER_LEAF)
        free(collider_node->data.leaf.vertices);
    // the right hand node has half the indices buffer, but left and right indices are allocated as one block. Left has index 0 as the start. Right has index 0 + num_left as the start
    if (handedness == LEFT)
        free(collider_node->indices);
    free(collider_node);
}

struct TekNodeHandednessPair {
    TekColliderNode* collider_node;
    flag handedness;
};

/**
 * Delete a collider node by freeing all allocated memory. Will set pointer to NULL to avoid misuse of freed pointer.
 * @param collider The collider structure to free.
 */
void tekDeleteCollider(TekCollider* collider) {
    if (!collider || !(*collider)) return;
    Vector collider_stack = {};
    if (vectorCreate(16, sizeof(struct TekNodeHandednessPair), &collider_stack) != SUCCESS) return;

    // traverse tree and record the handedness of each child node for when freeing.
    struct TekNodeHandednessPair pair = {*collider, LEFT};
    vectorAddItem(&collider_stack, &pair);
    while (vectorPopItem(&collider_stack, &pair)) {
        // post-order traversal so that child nodes can be recorded before freeing the parent.
        if (pair.collider_node->type == COLLIDER_NODE) {
            // temp struct so that the data can be copied into the vector
            struct TekNodeHandednessPair temp_pair = {};
            temp_pair.collider_node = pair.collider_node->data.node.right;
            temp_pair.handedness = RIGHT;
            vectorAddItem(&collider_stack, &temp_pair);
            temp_pair.collider_node = pair.collider_node->data.node.left;
            temp_pair.handedness = LEFT;
            vectorAddItem(&collider_stack, &temp_pair);
        }
        tekDeleteColliderNode(pair.collider_node, pair.handedness);
    }

    // free vector used to iterate.
    vectorDelete(&collider_stack);
    *collider = 0;
}

void tekUpdateOBB(struct OBB* obb, mat4 transform) {
    glm_mat4_mulv3(transform, obb->centre, 1.0f, obb->w_centre);

    for (uint i = 0; i < 3; i++) {
        glm_mat4_mulv3(transform, obb->axes[i], 0.0f, obb->w_axes[i]);
        const float scale_factor = glm_vec3_norm(obb->axes[i]);
        glm_vec3_scale(obb->axes[i], 1.0f / scale_factor, obb->axes[i]);
        obb->w_half_extents[i] = obb->half_extents[i] * scale_factor;
    }
}

void tekUpdateLeaf(TekColliderNode* leaf, mat4 transform) {
    for (uint i = 0; i < leaf->data.leaf.num_vertices; i++) {
        glm_mat4_mulv3(transform, leaf->data.leaf.vertices[i], 1.0f, leaf->data.leaf.w_vertices[i]);
    }
}
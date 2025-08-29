#include "collider.h"

#include <stdio.h>
#include <string.h>

#include <cglm/vec3-ext.h>
#include <cglm/mat3.h>

#include "geometry.h"
#include "body.h"
#include "engine.h"
#include "../tekgl/manager.h"

#define EPSILON 1e-6
#define LEFT  0
#define RIGHT 1

#define NOT_INITIALISED 0
#define INITIALISED     1
#define DE_INITIALISED  2

static Vector collider_buffer = {};
static Vector collision_data_buffer = {};
static flag collider_init = NOT_INITIALISED;

struct CollisionData {
    vec3 r_a;
    vec3 r_b;
    vec3 impulse;
};

void tekColliderDelete() {
    collider_init = DE_INITIALISED;
    vectorDelete(&collider_buffer);
    vectorDelete(&collision_data_buffer);
}

tek_init TekColliderInit() {
    exception tek_exception = vectorCreate(16, 2 * sizeof(TekColliderNode*), &collider_buffer);
    if (tek_exception != SUCCESS) return;

    tek_exception = vectorCreate(8, sizeof(struct CollisionData), &collision_data_buffer);
    if (tek_exception != SUCCESS) return;

    tek_exception = tekAddDeleteFunc(tekColliderDelete);
    if (tek_exception != SUCCESS) return;

    collider_init = INITIALISED;
}

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

    // TODO: remove this print statement
    printf("Created OBB: %f %f %f\n", EXPAND_VEC3(centre));

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

    tekPrintCollider(*collider, 0);

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

/**
 * Check whether there is a collision between two OBBs using the separating axis theorem.
 * @param obb_a The first obb.
 * @param obb_b The obb that will be tested against the first one.
 */
static flag tekCheckOBBCollision(struct OBB* obb_a, struct OBB* obb_b) {
    // OBB-OBB collision using the separating axis theorem (SAT)
    // based on Gottschalk et al., "OBBTree" (1996) https://www.cs.unc.edu/techreports/96-013.pdf
    // also described in Ericson, "Real-Time Collision Detection", Ch. 4

    // algorithm based on https://jkh.me/files/tutorials/Separating%20Axis%20Theorem%20for%20Oriented%20Bounding%20Boxes.pdf
    // see section "Optimized Computation of OBBs Intersections"

    // find the vector between the centres
    vec3 translate;
    glm_vec3_sub(obb_b->w_centre, obb_a->w_centre, translate);

    // some buffers to store some precalculated data that is reused throughout.
    float t_array[3];
    float dot_matrix[3][3];

    for (uint i = 0; i < 3; i++) {
        t_array[i] = glm_vec3_dot(translate, obb_a->w_axes[i]);
        for (uint j = 0; j < 3; j++) {
            dot_matrix[i][j] = glm_vec3_dot(obb_a->w_axes[i], obb_b->w_axes[j]) + EPSILON;
        }
    }

    // simple cases - check axes aligned with faces of the first obb.
    for (uint i = 0; i < 3; i++) {
        float mag_sum = 0.0f;
        for (uint j = 0; j < 3; j++) {
            mag_sum += fabs(obb_b->w_half_extents[i] * dot_matrix[i][j]);
        }
        if (fabs(t_array[i] > obb_a->w_half_extents[i] + mag_sum)) {
            return 0;
        }
    }

    // medium cases - check axes aligned with the faces of the second obb.
    for (uint i = 0; i < 3; i++) {
        float mag_sum = 0.0f;
        for (uint j = 0; j < 3; j++) {
            mag_sum += fabs(obb_a->w_half_extents[i] * dot_matrix[j][i]);
        }
        const float projection = fabs(glm_vec3_dot(translate, obb_b->w_axes[i]));
        if (projection > obb_b->w_half_extents[i] + mag_sum) {
            return 0;
        }
    }

    // store indices of t_array to access depending on iteration count
    const uint multis_indices[3][2] = {
        {2, 1},
        {0, 2},
        {1, 0}
    };

    // store indices for direction (W, H or D) for inner and outer loop cycles
    const uint cyc_indices[3][2] = {
        {1, 2},
        {0, 2},
        {0, 1}
    };

    // tricky tricky cases - check axes aligned with the edges and corners of both - cross products of the axes from before.
    for (uint i = 0; i < 3; i++) {
        for (uint j = 0; j < 3; j++) {
            const uint ti_a = multis_indices[i][0], ti_b = multis_indices[i][1];
            const float cmp_base = t_array[ti_a] * glm_vec3_dot(obb_a->w_axes[ti_b], obb_b->w_axes[j]);
            const float cmp_subt = t_array[ti_b] * glm_vec3_dot(obb_a->w_axes[ti_a], obb_b->w_axes[j]);
            const float cmp = fabs(cmp_base - cmp_subt);

            const uint cyc_ll = cyc_indices[i][0], cyc_lh = cyc_indices[i][1];
            const uint cyc_sl = cyc_indices[j][0], cyc_sh = cyc_indices[j][1];
            float tst = 0.0f;
            tst += fabs(obb_a->w_half_extents[cyc_ll] * dot_matrix[cyc_lh][j]);
            tst += fabs(obb_a->w_half_extents[cyc_lh] * dot_matrix[cyc_ll][j]);
            tst += fabs(obb_a->w_half_extents[cyc_sl] * dot_matrix[i][cyc_sh]);
            tst += fabs(obb_a->w_half_extents[cyc_sh] * dot_matrix[i][cyc_sl]);

            if (cmp > tst) {
                return 0;
            }
        }
    }

    // if no separation is found in any axis, there must be a collision.
    return 1;
}

static void tekCreateOBBTransform(const struct OBB* obb, mat4 transform) {
    mat4 temp_transform = {
        obb->w_axes[0][0], obb->w_axes[1][0], obb->w_axes[2][0], obb->w_centre[0],
        obb->w_axes[0][1], obb->w_axes[1][1], obb->w_axes[2][1], obb->w_centre[1],
        obb->w_axes[0][2], obb->w_axes[1][2], obb->w_axes[2][2], obb->w_centre[2],
        0.0f, 0.0f, 0.0f, 1.0f
    };
    glm_mat4_inv_fast(temp_transform, transform);
}

static int getSign(const float x) {
    if (x > FLT_EPSILON) {
        return 1;
    }
    if (x < -FLT_EPSILON) {
        return -1;
    }
    return 0;
}

static float tekCalculateOrientation(vec3 triangle[3], vec3 point) {
    vec3 sub_buffer[3];
    for (uint i = 0; i < 3; i++) {
        glm_vec3_sub(triangle[i], point, sub_buffer[i]);
    }
    return scalarTripleProduct(sub_buffer[0], sub_buffer[1], sub_buffer[2]);
}

static float tekCalculateOrientationPoints(vec3 point_a, vec3 point_b, vec3 point_c, vec3 point_d) {
    vec3 sub_buffer[3];
    glm_vec3_sub(point_a, point_d, sub_buffer[0]);
    glm_vec3_sub(point_b, point_d, sub_buffer[1]);
    glm_vec3_sub(point_c, point_d, sub_buffer[2]);
    return scalarTripleProduct(sub_buffer[0], sub_buffer[1], sub_buffer[2]);
}

void tekSwapVerticesRight(vec3 vertices[3]) {
    vec3 temp;
    glm_vec3_copy(vertices[0], temp);
    glm_vec3_copy(vertices[2], vertices[0]);
    glm_vec3_copy(vertices[1], vertices[2]);
    glm_vec3_copy(temp, vertices[1]);
}

void tekSwapVerticesLeft(vec3 vertices[3]) {
    vec3 temp;
    glm_vec3_copy(vertices[0], temp);
    glm_vec3_copy(vertices[1], vertices[0]);
    glm_vec3_copy(vertices[2], vertices[1]);
    glm_vec3_copy(temp, vertices[2]);
}

static void tekCalculateSignArray(vec3 triangle_a[3], vec3 triangle_b[3], int sign_array[3]) {
    // find the "determinants" of each point on triangle_a against triangle_b
    // we only care about the sign of the calculation though.
    // the sign represents whether the point is in the positive or negative halfspace represented by the plane formed by the triangle.
    // the two halfspaces are if you imagine the triangle extended to an infinite plane and dividing the universe in **half** on either side of the plane.
    for (uint i = 0; i < 3; i++) {
        sign_array[i] = getSign(tekCalculateOrientation(triangle_a, triangle_b[i]));
    }
}

static void tekFindAndSwapTriangleVertices(vec3 triangle[3], const int sign_array[3]) {
    // get the triangle into a point where there is only one vertex with a negative sign.
    if (sign_array[0] == sign_array[1]) {
        tekSwapVerticesRight(triangle);
    } else if (sign_array[0] == sign_array[2]) {
        tekSwapVerticesLeft(triangle);
    } else if (sign_array[1] != sign_array[2]) {
        if (sign_array[1] > 0) {
            tekSwapVerticesLeft(triangle);
        } else if (sign_array[2] > 0) {
            tekSwapVerticesRight(triangle);
        }
    }
}

static void tekSwapToMakePositive(vec3 triangle_a[3], vec3 triangle_b[3]) {
    const int sign_check = getSign(tekCalculateOrientation(triangle_b, triangle_a[0]));
    if (sign_check == -1) {
        vec3 temp;
        glm_vec3_copy(triangle_b[1], temp);
        glm_vec3_copy(triangle_b[2], triangle_b[1]);
        glm_vec3_copy(temp, triangle_b[2]);
    }
}

/**
 * Calculate the point of intersection between a line (defined by point_a and point_b) with a plane (defined by point_p and the normal)
 * @param point_a[in] The first point defining a line
 * @param point_b[in] The second point defining a line
 * @param point_p[in] Any point that lies on the plane
 * @param plane_normal[in] The normal of the plane, orthogonal to the surface of the plane.
 * @param intersection[out] The intersection point.
 */
static void tekGetPlaneIntersection(vec3 point_a, vec3 point_b, vec3 point_p, vec3 plane_normal, vec3 intersection) {
    vec3 vec_ab, vec_pa;
    // vector that represents direction of the line
    glm_vec3_sub(point_b, point_a, vec_ab);
    // vector that represents direction from the point on the plane to the start of the line
    glm_vec3_sub(point_a, point_p, vec_pa);
    const float
        dot_ab = glm_vec3_dot(plane_normal, vec_ab), // projection of line direction onto normal
        dot_pa = glm_vec3_dot(plane_normal, vec_pa); // projection of vector from plane to point

    if (fabsf(dot_ab) <= EPSILON) {
        if (fabsf(dot_pa) <= EPSILON) {
            glm_vec3_copy(point_a, intersection);
        }
        return;
    }

    glm_vec3_scale(vec_ab, -dot_pa / dot_ab, vec_ab); // scale by negative ratio between projections to get the vector
    glm_vec3_add(point_a, vec_ab, intersection);
}

/**
 * Get the contact normal of two triangles, assuming they have an edge-edge contact.
 * @param triangle_a The first triangle involved in the collision.
 * @param triangle_b The second triangle involved in the collision.
 * @param manifold_a The first manifold to update with the collision normal.
 * @param manifold_b The second manifold to update with the collision normal.
 */
static void tekGetTriangleEdgeContactNormal(vec3 edge_a, vec3 edge_b, TekCollisionManifold manifold) {
    vec3 contact_normal;
    glm_vec3_cross(edge_a, edge_b, contact_normal);
    glm_vec3_normalize(contact_normal);
    glm_vec3_copy(contact_normal, manifold.contact_normal);
}

exception tekCheckTriangleCollision(vec3 triangle_a[3], vec3 triangle_b[3], flag* collision, Vector* contact_points) {
    // get the sign number, which represents which vertices are on opposite sides of the triangle to each other.
    int sign_array_a[3];
    tekCalculateSignArray(triangle_b, triangle_a, sign_array_a);

    // if they're all positive or all negative, this means that all points are on the same side of the plane
    // this means its truly impossible for there to be a collision, there needs to be a point that crosses the plane for an intersection to happen.
    if ((sign_array_a[0] == sign_array_a[1]) && (sign_array_a[0] == sign_array_a[2])) {
        *collision = 0;
        return SUCCESS;
    }

    // do the same for the other triangle, quitting early if there is a scenario where the planes of triangles have no intersection.
    int sign_array_b[3];
    tekCalculateSignArray(triangle_b, triangle_a, sign_array_b);
    if ((sign_array_b[0] == sign_array_b[1]) && (sign_array_b[0] == sign_array_b[2])) {
        printf("Removed the bogus.\n");
        *collision = 0;
        return SUCCESS;
    }

    // now we need to permute the triangle vertices so that the point at index 0 is alone on it's side of the halfspace.
    tekFindAndSwapTriangleVertices(triangle_a, sign_array_a);
    tekFindAndSwapTriangleVertices(triangle_b, sign_array_b);

    tekSwapToMakePositive(triangle_b, triangle_a);
    tekSwapToMakePositive(triangle_a, triangle_b);

    const int orient_a = getSign(tekCalculateOrientationPoints(triangle_a[0], triangle_a[1], triangle_b[0], triangle_b[1]));
    if (orient_a > 0) {
        *collision = 0;
        return SUCCESS;
    }
    const int orient_b = getSign(tekCalculateOrientationPoints(triangle_a[0], triangle_a[2], triangle_b[2], triangle_b[0]));
    if (orient_b > 0) {
        *collision = 0;
        return SUCCESS;
    }
    *collision = 1;
    const int orient_c = getSign(tekCalculateOrientationPoints(triangle_a[0], triangle_a[2], triangle_b[1], triangle_b[0]));
    const int orient_d = getSign(tekCalculateOrientationPoints(triangle_a[0], triangle_a[1], triangle_b[2], triangle_b[0]));

    vec3 normal_a, normal_b;
    triangleNormal(triangle_a, normal_a);
    triangleNormal(triangle_b, normal_b);

    TekCollisionManifold manifold;

    if (orient_c > 0) {
        if (orient_d > 0) {
            // edge of triangle a vs edge of triangle b
            tekGetPlaneIntersection(triangle_a[0], triangle_a[2], triangle_b[0], normal_b, manifold.contact_points[0]);
            tekGetPlaneIntersection(triangle_b[0], triangle_b[2], triangle_a[0], normal_a, manifold.contact_points[1]);
            vec3 edge_a, edge_b;
            glm_vec3_sub(triangle_a[0], triangle_a[2], edge_a);
            glm_vec3_sub(triangle_b[0], triangle_b[2], edge_b);
            tekGetTriangleEdgeContactNormal(edge_b, edge_a, manifold);
        } else {
            // vertex of triangle a vs face of triangle b
            tekGetPlaneIntersection(triangle_a[0], triangle_a[2], triangle_b[0], normal_b, manifold.contact_points[0]);
            tekGetPlaneIntersection(triangle_a[0], triangle_a[1], triangle_b[0], normal_b, manifold.contact_points[1]);
            glm_vec3_negate_to(normal_b, manifold.contact_normal);
        }
    } else {
        if (orient_d > 0) {
            // face of triangle a vs vertex of triangle b
            tekGetPlaneIntersection(triangle_b[0], triangle_b[1], triangle_a[0], normal_a, manifold.contact_points[0]);
            tekGetPlaneIntersection(triangle_b[0], triangle_b[2], triangle_a[0], normal_a, manifold.contact_points[1]);
            glm_vec3_copy(normal_a, manifold.contact_normal);
        } else {
            // edge of triangle a vs edge of triangle b
            tekGetPlaneIntersection(triangle_b[0], triangle_b[1], triangle_a[0], normal_a, manifold.contact_points[0]);
            tekGetPlaneIntersection(triangle_a[0], triangle_a[1], triangle_b[0], normal_b, manifold.contact_points[1]);
            vec3 edge_a, edge_b;
            glm_vec3_sub(triangle_a[0], triangle_a[1], edge_a);
            glm_vec3_sub(triangle_b[0], triangle_b[1], edge_b);
            tekGetTriangleEdgeContactNormal(edge_a, edge_b, manifold);
        }
    }

    if (manifold.contact_normal[0] >= 0.57735f) { // ~= 1 / sqrt(3)
        manifold.tangent_vectors[0][0] = manifold.contact_normal[1];
        manifold.tangent_vectors[0][1] = -manifold.contact_normal[0];
        manifold.tangent_vectors[0][2] = 0.0f;
    } else {
        manifold.tangent_vectors[0][0] = 0.0f;
        manifold.tangent_vectors[0][1] = manifold.contact_normal[2];
        manifold.tangent_vectors[0][2] = -manifold.contact_normal[1];
    }

    glm_vec3_normalize(manifold.tangent_vectors[0]);
    glm_vec3_cross(manifold.contact_normal, manifold.tangent_vectors[0], manifold.tangent_vectors[1]);

    return SUCCESS;
}

static exception tekCheckTrianglesCollision(vec3* triangles_a, const uint num_triangles_a, vec3* triangles_b, const uint num_triangles_b, flag* collision, Vector* contact_points) {
    flag sub_collision = 0;
    *collision = 0;
    for (uint i = 0; i < num_triangles_a; i++) {
        for (uint j = 0; j < num_triangles_b; j++) {
            tekChainThrow(tekCheckTriangleCollision(triangles_a + i * 3, triangles_b + j * 3, &sub_collision, contact_points));
            if (sub_collision) *collision = 1;
        }
    }
    return SUCCESS;
}

static flag tekCheckAABBTriangleCollision(const float half_extents[3], vec3 triangle[3]) {
    vec3 face_vectors[3];
    for (uint i = 0; i < 3; i++) {
        glm_vec3_sub(
            triangle[(i + 1) % 3],
            triangle[i],
            face_vectors[i]
            );
    }

    vec3 xyz_axes[3] = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };

    // first, test the axis which is the normal of the triangle.
    // dots[0] = the distance of the triangle from the centre of the aabb when projected onto the normal.
    // face_projection = half the length of the aabb when projected onto the normal axis
    //
    // +---+      /|
    // |   |     / |
    // +---+    /__|
    //
    // |===|------|-    < this is the axis being projected against
    //   ^    ^   ^
    //   a    b   c
    // a = face_projection - the aabb projected onto the axis
    // b = the separation (Separating Axis Theorem)
    // c = dots[0] - the triangle which is perpendicular to the axis, projection is a single point.
    // hence, when c > a, there is separation and no collision. if a > c, then there is no separation in this axis, continue the search
    vec3 face_normal;
    // normal of a triangle = cross product of any two edges
    glm_vec3_cross(face_vectors[0], face_vectors[1], face_normal);
    glm_vec3_normalize(face_normal);
    // when testing the face normal axis, all three points of the triangle project to the same point. so just check point 0

    const float tst = fabs(glm_vec3_dot(triangle[0], face_normal));
    float cmp = 0.0f;
    for (uint i = 0; i < 3; i++) {
        cmp += half_extents[i] * fabs(face_normal[i]);
    }
    if (tst > cmp) {
        return 0;
    }

    // testing against the 3 axes of the AABB, which are by definition the X, Y and Z axes.
    float dots[3];
    for (uint i = 0; i < 3; i++) {
        for (uint j = 0; j < 3; j++) {
            dots[j] = glm_vec3_dot(triangle[j], xyz_axes[i]);
        }
        // the projection of the AABB along this axis will just be the half extent:
        //
        // +-----------+
        // |     O---->|   this vector represents an axis, as you can see the axis aligns with the half extent.
        // +-----------+

        const float tri_min = fminf(dots[0], fminf(dots[1], dots[2]));
        const float tri_max = fmaxf(dots[0], fmaxf(dots[1], dots[2]));
        if (tri_min >  half_extents[i] || tri_max < -half_extents[i]) {
            return 0;
        }
    }

    // testing against the final 9 axes, which are the cross products against the AABB's axes and the triangle's edge vectors.
    for (uint i = 0; i < 3; i++) {
        for (uint j = 0; j < 3; j++) {
            vec3 curr_axis;
            glm_vec3_cross(xyz_axes[i], face_vectors[j], curr_axis);
            float projection = 0.0f;
            for (uint k = 0; k < 3; k++) {
                projection += half_extents[k] * fabsf(glm_vec3_dot(xyz_axes[k], curr_axis));
                dots[k] = glm_vec3_dot(triangle[k], curr_axis);
            }

            const float tri_min = fminf(dots[0], fminf(dots[1], dots[2]));
            const float tri_max = fmaxf(dots[0], fmaxf(dots[1], dots[2]));
            if (tri_min >  projection || tri_max < -projection) {
                return 0;
            }
        }
    }

    // assuming no separation was found in any axis, then there must be a collision.
    return 1;
}

static flag tekCheckOBBTriangleCollision(const struct OBB* obb, const vec3 triangle[3]) {
    mat4 transform;
    tekCreateOBBTransform(obb, transform);

    vec3 transformed_triangle[3];

    for (uint i = 0; i < 3; i++) {
        glm_mat4_mulv3(transform, triangle[i], 1.0f, transformed_triangle[i]);
    }

    return tekCheckAABBTriangleCollision(obb->w_half_extents, transformed_triangle);
}

static flag tekCheckOBBTrianglesCollision(const struct OBB* obb, const vec3* triangles, const uint num_triangles) {
    for (uint i = 0; i < num_triangles; i ++) {
        if (tekCheckOBBTriangleCollision(obb, triangles + i * 3)) return 1;
    }
    return 0;
}

static void tekUpdateOBB(struct OBB* obb, mat4 transform) {
    glm_mat4_mulv3(transform, obb->centre, 1.0f, obb->w_centre);

    for (uint i = 0; i < 3; i++) {
        glm_mat4_mulv3(transform, obb->axes[i], 0.0f, obb->w_axes[i]);
        const float scale_factor = glm_vec3_norm(obb->axes[i]);
        glm_vec3_scale(obb->axes[i], 1.0f / scale_factor, obb->axes[i]);
        obb->w_half_extents[i] = obb->half_extents[i] * scale_factor;
    }
}

static void tekUpdateLeaf(TekColliderNode* leaf, mat4 transform) {
    for (uint i = 0; i < leaf->data.leaf.num_vertices; i++) {
        glm_mat4_mulv3(transform, leaf->data.leaf.vertices[i], 1.0f, leaf->data.leaf.w_vertices[i]);
    }
}

#define getChild(collider_node, i) (i == LEFT) ? collider_node->data.node.left : collider_node->data.node.right

exception tekTestForCollisions(TekBody* a, TekBody* b, flag* collision, Vector* contact_points) {
    if (collider_init == DE_INITIALISED) return SUCCESS;
    if (collider_init == NOT_INITIALISED) tekThrow(FAILURE, "Collider buffer was never initialised.");

    contact_points->length = 0;

    *collision = 0;
    collider_buffer.length = 0;
    TekColliderNode* pair[2] = {
        a->collider, b->collider
    };
    tekChainThrow(vectorAddItem(&collider_buffer, &pair));
    while (vectorPopItem(&collider_buffer, &pair)) {
        if (pair[LEFT]->type == COLLIDER_NODE) {
            tekUpdateOBB(&pair[LEFT]->data.node.left->obb, a->transform);
            tekUpdateOBB(&pair[LEFT]->data.node.right->obb, a->transform);
        }
        if (pair[RIGHT]->type == COLLIDER_NODE) {
            tekUpdateOBB(&pair[RIGHT]->data.node.left->obb, b->transform);
            tekUpdateOBB(&pair[RIGHT]->data.node.right->obb, b->transform);
        }

        TekColliderNode* temp_pair[2];
        for (uint i = 0; i < 2; i++) {
            for (uint j = 0; j < 2; j++) {
                TekColliderNode* node_a = (pair[LEFT]->type == COLLIDER_NODE) ? getChild(pair[LEFT], i) : pair[LEFT];
                TekColliderNode* node_b = (pair[RIGHT]->type == COLLIDER_NODE) ? getChild(pair[RIGHT], j) : pair[RIGHT];

                flag sub_collision = 0;

                if ((node_a->type == COLLIDER_NODE) && (node_b->type == COLLIDER_NODE)) {
                    sub_collision = tekCheckOBBCollision(&node_a->obb, &node_b->obb);
                } else if ((node_a->type == COLLIDER_NODE) && (node_b->type == COLLIDER_LEAF)) {
                    sub_collision = tekCheckOBBTrianglesCollision(&node_a->obb, node_b->data.leaf.w_vertices, node_b->data.leaf.num_vertices / 3);
                } else if ((node_a->type == COLLIDER_LEAF) && (node_b->type == COLLIDER_NODE)) {
                    sub_collision = tekCheckOBBTrianglesCollision(&node_b->obb, node_a->data.leaf.w_vertices, node_a->data.leaf.num_vertices / 3);
                } else {
                    tekUpdateLeaf(node_a, a->transform);
                    tekUpdateLeaf(node_b, b->transform);
                    tekChainThrow(tekCheckTrianglesCollision(
                        node_a->data.leaf.w_vertices, node_a->data.leaf.num_vertices / 3,
                        node_b->data.leaf.w_vertices, node_b->data.leaf.num_vertices / 3,
                        &sub_collision,
                        contact_points
                        ));
                    if (sub_collision) {
                        sub_collision = 0;
                        *collision = 1;
                    }
                }

                if (sub_collision) {
                    temp_pair[LEFT] = node_a;
                    temp_pair[RIGHT] = node_b;
                    tekChainThrow(vectorAddItem(&collider_buffer, &temp_pair));
                }
            }
        }
    }
    return SUCCESS;
}

exception tekApplyCollision(TekBody* a, TekBody* b, const Vector* contact_points) {
    if (collider_init == DE_INITIALISED) return SUCCESS;
    if (collider_init == NOT_INITIALISED) tekThrow(FAILURE, "Collider buffer was never initialised.");

    collision_data_buffer.length = 0;

    for (uint i = 0; i < contact_points->length; i++) {
        TekCollisionManifold* manifold;
        vectorGetItemPtr(contact_points, i, &manifold);

        struct CollisionData collision_data = {};
        glm_vec3_sub(manifold->contact_point, a->position, collision_data.r_a);
        glm_vec3_sub(manifold->contact_point, b->position, collision_data.r_b);

        vec3 rp_a, rp_b;


    }

    return SUCCESS;
}

flag testCollision(TekBody* a, TekBody* b) {
    tekUpdateOBB(&a->collider->obb, a->transform);
    tekUpdateOBB(&b->collider->obb, b->transform);
    return tekCheckOBBCollision(&a->collider->obb, &b->collider->obb);
}

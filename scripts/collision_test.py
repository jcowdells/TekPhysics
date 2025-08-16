from numpy import ndarray
from ursina import *
from dataclasses import dataclass
import numpy as np
from ursina.color import rgba
from ursina.shaders import lit_with_shadows_shader, basic_lighting_shader

EPSILON = 1e-6

@dataclass
class OBB:
    centre: np.ndarray
    half_extents: list[float]
    axes: list[np.ndarray]

@dataclass
class Triangle:
    vertices: list[np.ndarray]

def create_obb_mesh(obb: OBB) -> Mesh:
    # cheeky function to map 0 => 1, 1 => -1
    sign = lambda x: (-1) ** x

    # basically create all the vertices of the OBB based on the dimensions of OBB
    vertices: list[np.ndarray] = [None for _ in range(8)]
    for i in range(2):
        for j in range(2):
            for k in range(2):
                vertices[i + 2 * j + 4 * k] = np.multiply(obb.axes[0], sign(i) * obb.half_extents[0])
                vertices[i + 2 * j + 4 * k] = np.add(vertices[i + 2 * j + 4 * k], np.multiply(obb.axes[1], sign(j) * obb.half_extents[1]))
                vertices[i + 2 * j + 4 * k] = np.add(vertices[i + 2 * j + 4 * k], np.multiply(obb.axes[2], sign(k) * obb.half_extents[2]))
                vertices[i + 2 * j + 4 * k] = np.add(vertices[i + 2 * j + 4 * k], obb.centre)

    # now turn np arrays into normal python arrays so ursina can use it
    for i in range(len(vertices)):
        vertices[i] = vertices[i].tolist()

    # build el mesh using triangle indices
    obb_mesh = Mesh(
        vertices=vertices,
        triangles=[
            1, 3, 7,
            1, 7, 5,
            0, 4, 6,
            0, 6, 2,
            2, 6, 7,
            2, 7, 3,
            0, 1, 5,
            0, 5, 4,
            4, 5, 7,
            4, 7, 6,
            0, 2, 3,
            0, 3, 1
        ]
    )

    # i assume this creates some kinda gpu buffer as it doesnt render without?
    obb_mesh.generate_normals()
    obb_mesh.generate()
    return obb_mesh

def create_triangle_mesh(triangle: Triangle):
    vertices: list[np.ndarray] = list()
    for i in range(3):
        vertices.append(triangle.vertices[i].tolist())

    for i in range(3):
        vertices.append(triangle.vertices[2 - i].tolist())
        vertices[i] = np.add(vertices[i], np.array([0.0, EPSILON, 0.0]))

    triangle_mesh = Mesh(
        vertices=vertices
    )
    triangle_mesh.generate_normals()
    triangle_mesh.generate()
    return triangle_mesh

def check_obb_obb_collision(obb_a: OBB, obb_b: OBB):
    translate = np.subtract(obb_b.centre, obb_a.centre)

    dot_matrix = list()
    for i in range(3):
        dot_matrix.append(list())
        for j in range(3):
            dot_matrix[i].append(0)

    for i in range(3):
        for j in range(3):
            dot_matrix[i][j] = np.dot(obb_a.axes[i], obb_b.axes[j]) + EPSILON

    t_array = [0] * 3
    for i in range(3):
        t_array[i] = np.dot(translate, obb_a.axes[i])

    # Basic cases

    for i in range(3):
        mag_x = np.fabs(obb_b.half_extents[0] * dot_matrix[i][0])
        mag_y = np.fabs(obb_b.half_extents[1] * dot_matrix[i][1])
        mag_z = np.fabs(obb_b.half_extents[2] * dot_matrix[i][2])
        if np.fabs(t_array[i]) > obb_a.half_extents[i] + mag_x + mag_y + mag_z:
            print("Early return @ BASIC")
            return False

    # Intermediate cases

    for i in range(3):
        mag_x = np.fabs(obb_a.half_extents[0] * dot_matrix[0][i])
        mag_y = np.fabs(obb_a.half_extents[1] * dot_matrix[1][i])
        mag_z = np.fabs(obb_a.half_extents[2] * dot_matrix[2][i])
        dot_product = np.fabs(np.dot(translate, obb_b.axes[i]))
        if dot_product > obb_b.half_extents[i] + mag_x + mag_y + mag_z:
            print("Early return @ INTERMEDIATE")
            return False

    # Difficult cases
    # Store the indices of t_array to access for i values.
    multis_indices = [
        (2, 1),
        (0, 2),
        (1, 0)
    ]

    # Store indices for direction (W H or D) for inner and outer loop cycles
    cyc_indices = [
        (1, 2),
        (0, 2),
        (0, 1)
    ]

    # axn = ["X", "Y", "Z"]
    # dep = ["W", "H", "D"]

    for i in range(3):
        for j in range(3):
            ti_a, ti_b = multis_indices[i]
            cmp_base = t_array[ti_a] * np.dot(obb_a.axes[ti_b], obb_b.axes[j])
            cmp_subt = t_array[ti_b] * np.dot(obb_a.axes[ti_a], obb_b.axes[j])
            cmp = np.fabs(cmp_base - cmp_subt)

            cyc_ll, cyc_lh = cyc_indices[i]
            cyc_sl, cyc_sh = cyc_indices[j]

            tst_a = np.fabs(obb_a.half_extents[cyc_ll] * dot_matrix[cyc_lh][j])
            tst_b = np.fabs(obb_a.half_extents[cyc_lh] * dot_matrix[cyc_ll][j])
            tst_c = np.fabs(obb_b.half_extents[cyc_sl] * dot_matrix[i][cyc_sh])
            tst_d = np.fabs(obb_b.half_extents[cyc_sh] * dot_matrix[i][cyc_sl])

            tst = tst_a + tst_b + tst_c + tst_d

            # print(f"|(T.A{axn[ti_a]})R{axn[ri_a]}{axn[j]}-(T.A{axn[ti_b]})R{axn[ri_b]}{axn[j]}| > |{dep[cyc_ll]}AR{axn[cyc_lh]}{axn[j]}| + |{dep[cyc_lh]}AR{axn[cyc_ll]}{axn[j]}| + |{dep[cyc_sl]}BR{axn[i]}{axn[cyc_sh]}| + |{dep[cyc_sh]}BR{axn[i]}{axn[cyc_sl]}|")

            if cmp > tst:
                print("Early return @ DIFFICULT")
                return False

    return True

def create_obb_transform(obb: OBB) -> np.ndarray:
    transform = np.array([
        obb.axes[0][0], obb.axes[1][0], obb.axes[2][0], obb.centre[0],
        obb.axes[0][1], obb.axes[1][1], obb.axes[2][1], obb.centre[1],
        obb.axes[0][2], obb.axes[1][2], obb.axes[2][2], obb.centre[2],
        0.0, 0.0, 0.0, 1.0
    ])
    transform.shape = 4,4
    return np.linalg.inv(transform)

def check_aabb_triangle_collision(half_extents: list[float], triangle: Triangle) -> bool:
    face_vectors: list[np.ndarray] = list()
    for i in range(3):
        face_vectors.append(np.subtract(triangle.vertices[(i + 1) % 3], triangle.vertices[i]))

    xyz_axes: list[np.ndarray] = [
        np.array([1.0, 0.0, 0.0]),
        np.array([0.0, 1.0, 0.0]),
        np.array([0.0, 0.0, 1.0])
    ]

    face_normal = np.cross(face_vectors[0], face_vectors[1])
    face_normal = np.multiply(face_normal, 1.0 / float(np.linalg.norm(face_normal)))

    tst = np.fabs(np.dot(triangle.vertices[0], face_normal))
    cmp = 0.0
    for i in range(3):
        cmp += half_extents[i] * np.fabs(face_normal[i])
    if tst > cmp:
        return False

    for i in range(3):
        dots: list[float] = list()
        for j in range(3):
            dots.append(np.dot(triangle.vertices[j], xyz_axes[i]))

        tri_min = min(dots[0], dots[1], dots[2])
        tri_max = max(dots[0], dots[1], dots[2])

        if (tri_min > half_extents[i]) or (tri_max < -half_extents[i]):
            return False

    for i in range(3):
        for j in range(3):
            dots: list[float] = list()
            curr_axis: np.ndarray = np.cross(xyz_axes[i], face_vectors[j])
            projection: float = 0.0
            for k in range(3):
                projection += half_extents[k] * np.fabs(np.dot(xyz_axes[k], curr_axis))
                dots.append(np.dot(triangle.vertices[k], curr_axis))

            tri_min = min(dots[0], dots[1], dots[2])
            tri_max = max(dots[0], dots[1], dots[2])
            if (tri_min > projection) or (tri_max < -projection):
                return False

    return True

def check_obb_triangle_collision(obb: OBB, triangle: Triangle) -> bool:
    transform = create_obb_transform(obb)
    transformed_vertices = list()
    for i in range(3):
        vertex = np.array([triangle.vertices[i][0], triangle.vertices[i][1], triangle.vertices[i][2], 1.0])
        product = np.matmul(transform, vertex)
        transformed_vertices.append(np.array([product[0], product[1], product[2]]))
    return check_aabb_triangle_collision(obb.half_extents, Triangle(vertices=transformed_vertices))

def check_triangle_triangle_collision(triangle_a: Triangle, triangle_b: Triangle) -> bool:

    return True

class SphereControllerEntity(Entity):
    NORMAL_COLOUR = rgb(0.9, 0.9, 0.9)
    HOVERED_COLOUR = rgb(1.0, 1.0, 1.0)
    HOVERED_SCALE_MULTIPLIER = 1.2
    ANIMATION_DURATION = 0.2

    def __init__(self, position: np.ndarray, radius: float=0.25, update_callback=None):
        super().__init__(model="sphere", scale=radius, color=SphereControllerEntity.NORMAL_COLOUR, collider="sphere")
        self.always_on_top_setter(True)
        self.world_position = position
        self.__radius = radius
        self.__hovering = False
        self.selected = False
        self.__cam_vector = Vec3(0, 0, 0)
        self.__flicker = 0
        self.__update_callback = update_callback

    def update(self):
        if self.__hovering and mouse.left:
            self.selected = True

        if not mouse.left:
            self.selected = False

        if self.selected:
            mouse_vector = self.world_position - camera.world_position
            plane_distance = camera.forward.dot(mouse_vector)

            scale_factor = math.tan(math.radians(camera.fov / 2))
            side_vector = camera.forward.cross(camera.up) * scale_factor
            up_vector = camera.up * (scale_factor / camera.aspect_ratio_getter())

            mouse_x = ((mouse.x / camera.aspect_ratio_getter()) + 0.5)
            mouse_y = (0.5 - mouse.y)

            base_position = (side_vector + up_vector + camera.forward)
            x_vector = side_vector * (-2 * mouse_x)
            y_vector = up_vector * (-2 * mouse_y)

            self.world_position = camera.world_position + (base_position + x_vector + y_vector) * plane_distance

        if self.hovered and not self.__hovering:
            self.animate_color(SphereControllerEntity.HOVERED_COLOUR, duration=SphereControllerEntity.ANIMATION_DURATION, curve=curve.in_out_expo)
            self.animate_scale(SphereControllerEntity.HOVERED_SCALE_MULTIPLIER * self.__radius, duration=SphereControllerEntity.ANIMATION_DURATION, curve=curve.in_out_expo)
            self.__hovering = True
        elif not self.hovered and self.__hovering:
            self.animate_color(SphereControllerEntity.NORMAL_COLOUR, duration=SphereControllerEntity.ANIMATION_DURATION, curve=curve.in_out_expo)
            self.animate_scale(self.__radius, duration=SphereControllerEntity.ANIMATION_DURATION, curve=curve.in_out_expo)
            self.__hovering = False

        if self.__update_callback is not None:
            self.__update_callback()

class SphereController:
    def __init__(self, position: np.ndarray, radius: float=0.25, update_callback=None):
        self.__entity = SphereControllerEntity(position, radius, update_callback)

    def get_position(self) -> np.ndarray:
        sphere_position = self.__entity.world_position
        return np.array([sphere_position.x, sphere_position.y, sphere_position.z])

    def set_position(self, position: np.ndarray):
        self.__entity.world_position = position

    def is_selected(self) -> bool:
        return self.__entity.selected

class OBBController:
    def __init__(self, centre: np.ndarray, radius: float=0.25):
        self.__centre_sphere = SphereController(centre, radius, update_callback=self.__update)
        self.__axis_spheres: list[SphereController] = list()
        self.__axes: list[np.ndarray] = list()
        self.__prev_axes: list[np.ndarray] = list()
        for axis in (np.array([1, 0, 0]), np.array([0, 1, 0]), np.array([0, 0, 1])):
            final_axis = np.multiply(axis, 2 * radius)
            final_position = np.add(final_axis, centre)
            self.__axis_spheres.append(SphereController(final_position, radius))
            self.__prev_axes.append(final_axis)
            self.__axes.append(final_axis)

    def __update(self):
        if self.__centre_sphere.is_selected():
            for i in range(3):
                self.__axis_spheres[i].set_position(np.add(self.__centre_sphere.get_position(), self.__axes[i]))

        for i in range(3):
            if self.__axis_spheres[i].is_selected():
                cur_axis = self.__axes[i]
                prev_axis = self.__prev_axes[i]

                rotation_axis = np.cross(prev_axis, cur_axis)
                len_r = float(np.linalg.norm(rotation_axis))
                if len_r < 0.00001:
                    break
                rotation_axis = np.multiply(rotation_axis, 1 / len_r)

                position_delta = np.subtract(cur_axis, prev_axis)
                len_a = float(np.linalg.norm(position_delta))

                len_b = float(np.linalg.norm(cur_axis))
                len_c = float(np.linalg.norm(prev_axis))

                rotation_angle = math.acos((len_a ** 2 - len_b ** 2 - len_c ** 2) / (-2.0 * len_b * len_c))

                for j in range(3):
                    if i == j:
                        continue

                    # rodriguez formula
                    exv = np.cross(rotation_axis, self.__axes[j])
                    exexv = np.cross(rotation_axis, exv)

                    vec_a = np.multiply(exv, math.sin(rotation_angle))
                    vec_b = np.multiply(exexv, 1 - math.cos(rotation_angle))

                    vec_final = np.add(self.__axes[j], vec_a)
                    vec_final = np.add(vec_final, vec_b)

                    self.__axes[j] = vec_final
                    self.__axis_spheres[j].set_position(np.add(self.__centre_sphere.get_position(), self.__axes[j]))
                break

        # update previous values
        for i in range(3):
            self.__prev_axes[i] = self.__axes[i]
            self.__axes[i] = np.subtract(self.__axis_spheres[i].get_position(), self.__centre_sphere.get_position())

    def get_obb(self) -> OBB:
        half_extents = list()
        axes = list()
        for i in range(3):
            half_extents.append(np.linalg.norm(self.__axes[i]))
            axes.append(np.multiply(self.__axes[i], 1 / half_extents[i]))
        return OBB(
            centre=self.__centre_sphere.get_position(),
            half_extents=half_extents,
            axes=axes
        )

class InteractiveOBB(Entity):
    def __init__(self, centre: np.ndarray):
        self.__obb_controller: OBBController = OBBController(centre=centre, radius=0.25)
        self.__obb: OBB = self.__obb_controller.get_obb()
        self.__obb_mesh = create_obb_mesh(self.__obb)
        super().__init__(model=self.__obb_mesh, color=rgb(0.9, 0.9, 0.9), scale=1, shader=basic_lighting_shader)

    def update(self):
        self.__obb: OBB = self.__obb_controller.get_obb()
        self.__obb_mesh = create_obb_mesh(self.__obb)
        self.model_setter(self.__obb_mesh)

    def get_obb(self) -> OBB:
        return self.__obb

class TriangleController:
    def __init__(self, vertex_a: np.ndarray, vertex_b: np.ndarray, vertex_c: np.ndarray, radius: float=0.25):
        vertices = (vertex_a, vertex_b, vertex_c)
        self.__vertices: list[SphereController] = [SphereController(vertices[i], radius=radius) for i in range(3)]
    def get_triangle(self) -> Triangle:
        vertices = [self.__vertices[i].get_position() for i in range(3)]
        return Triangle(
            vertices=vertices
        )

class InteractiveTriangle(Entity):
    def __init__(self, vertex_a: np.ndarray, vertex_b: np.ndarray, vertex_c: np.ndarray):
        self.__triangle_controller: TriangleController = TriangleController(vertex_a, vertex_b, vertex_c, radius=0.25)
        self.__triangle: Triangle = self.__triangle_controller.get_triangle()
        self.__triangle_mesh = create_triangle_mesh(self.__triangle)
        super().__init__(model=self.__triangle_mesh, color=rgb(0.9, 0.9, 0.9), scale=1, shader=basic_lighting_shader)

    def update(self):
        self.__triangle = self.__triangle_controller.get_triangle()
        self.__triangle_mesh = create_triangle_mesh(self.__triangle)
        self.model_setter(self.__triangle_mesh)

    def get_triangle(self) -> Triangle:
        return self.__triangle

NORMAL_COLOUR = rgb(0.0, 0.9, 0.0)
TOUCH_COLOUR  = rgb(0.9, 0.0, 0.0)

class OBBCollisionTest(Entity):
    def __init__(self, centre_a: np.ndarray, centre_b: np.ndarray):
        self.obb_a = InteractiveOBB(centre_a)
        self.obb_a.color_setter(NORMAL_COLOUR)
        self.obb_b = InteractiveOBB(centre_b)
        self.obb_b.color_setter(NORMAL_COLOUR)
        super().__init__()

    def update(self):
        collision = check_obb_obb_collision(self.obb_a.get_obb(), self.obb_b.get_obb())
        if collision:
            self.obb_a.color_setter(TOUCH_COLOUR)
            self.obb_b.color_setter(TOUCH_COLOUR)
        else:
            self.obb_a.color_setter(NORMAL_COLOUR)
            self.obb_b.color_setter(NORMAL_COLOUR)

class OBBTriangleCollisionTest(Entity):
    def __init__(self, obb_centre: np.ndarray, triangle_a: np.ndarray, triangle_b: np.ndarray, triangle_c: np.ndarray):
        self.obb = InteractiveOBB(obb_centre)
        self.obb.color_setter(NORMAL_COLOUR)
        self.triangle = InteractiveTriangle(triangle_a, triangle_b, triangle_c)
        self.triangle.color_setter(NORMAL_COLOUR)

        self.display_triangle = Triangle(vertices=[triangle_a, triangle_b, triangle_c])
        self.display_mesh = create_triangle_mesh(self.display_triangle)
        self.display_entity = Entity(model=self.display_mesh, color=rgba(0.0, 0.0, 1.0, 0.5), shader=basic_lighting_shader)

        self.static_obb = OBB(centre=np.array([0, 0, 0]),
                              half_extents=self.obb.get_obb().half_extents,
                              axes=[
                                  np.array([1, 0 ,0]), np.array([0, 1, 0]), np.array([0, 0, 1])
                              ])
        self.static_mesh = create_obb_mesh(self.static_obb)
        self.static_entity = Entity(model=self.static_mesh, color=rgba(0.0, 0.0, 1.0, 0.5), shader=basic_lighting_shader)

        super().__init__()

    def update(self):
        triangle = self.triangle.get_triangle()
        obb = self.obb.get_obb()
        collision = check_obb_triangle_collision(self.obb.get_obb(), triangle)

        transform = create_obb_transform(obb)

        for i in range(3):
            vertex = np.array([triangle.vertices[i][0], triangle.vertices[i][1], triangle.vertices[i][2], 1.0])
            product = np.matmul(transform, vertex)
            self.display_triangle.vertices[i] = np.array([product[0], product[1], product[2]])

        self.display_mesh = create_triangle_mesh(self.display_triangle)
        self.display_entity.model_setter(self.display_mesh)

        self.static_obb.half_extents = self.obb.get_obb().half_extents
        self.static_mesh = create_obb_mesh(self.static_obb)
        self.static_entity.model_setter(self.static_mesh)

        if collision:
            self.obb.color_setter(TOUCH_COLOUR)
            self.triangle.color_setter(TOUCH_COLOUR)
        else:
            self.obb.color_setter(NORMAL_COLOUR)
            self.triangle.color_setter(NORMAL_COLOUR)

class TriangleCollisionTest(Entity):
    def __init__(self, *vertices):
        if len(vertices) != 6:
            raise ValueError("Need to have 6 vertices in order to make 2 triangles.")

        self.triangle_a = InteractiveTriangle(vertices[0], vertices[1], vertices[2])
        self.triangle_a.color_setter(NORMAL_COLOUR)
        self.triangle_b = InteractiveTriangle(vertices[3], vertices[4], vertices[5])
        self.triangle_b.color_setter(NORMAL_COLOUR)

        super().__init__()

    def update(self):
        collision = check_triangle_triangle_collision(self.triangle_a.get_triangle(), self.triangle_b.get_triangle())
        if collision:
            self.triangle_a.color_setter(TOUCH_COLOUR)
            self.triangle_b.color_setter(TOUCH_COLOUR)
        else:
            self.triangle_a.color_setter(NORMAL_COLOUR)
            self.triangle_b.color_setter(NORMAL_COLOUR)

def main():
    app = Ursina()
    sun = DirectionalLight()
    sun.look_at(Vec3(0, -1, 0.7))
    Sky()

    # obb_test = InteractiveOBB(centre=np.array([0, 0 ,0]))
    # triangle_test = InteractiveTriangle(np.array([0, 0, 0]), np.array([1, 0, 0]), np.array([0, 0, 1]))

    # collision_test = OBBTriangleCollisionTest(np.array([0, -1, 0]), np.array([0.0, 1.0, 0.0]), np.array([1.0, 0.0, 0.0]), np.array([0.0, 0.0, 1.0]))

    collision_test_2 = TriangleCollisionTest(np.array([0.0, 1.0, 0.0]), np.array([1.0, 0.0, 0.0]), np.array([0.0, 0.0, 1.0]), np.array([0.0, 2.0, 0.0]), np.array([1.0, 1.0, 0.0]), np.array([0.0, 1.0, 1.0]))

    EditorCamera()  # add camera controls for orbiting and moving the camera

    app.run()

if __name__ == "__main__":
    main()
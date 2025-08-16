from numpy import ndarray
from ursina import *
from dataclasses import dataclass
import numpy as np

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
    vertices: list[np.ndarray] = [None for _ in range(3)]
    for i in range(len(vertices)):
        vertices[i] = triangle.vertices[i].tolist()

    triangle_mesh = Mesh(
        vertices=vertices
    )
    triangle_mesh.generate_normals()
    triangle_mesh.generate()

class SphereControllerEntity(Entity):
    NORMAL_COLOUR = rgb(0.6, 0.6, 0.6)
    HOVERED_COLOUR = rgb(0.8, 0.8, 0.8)
    HOVERED_SCALE_MULTIPLIER = 1.2
    ANIMATION_DURATION = 0.2

    def __init__(self, position: np.ndarray, radius: float=0.25, update_callback=None):
        super().__init__(model="sphere", scale=radius, color=SphereControllerEntity.NORMAL_COLOUR, collider="sphere")
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
        return self.__entity.world_position

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
        print(centre)
        for axis in (np.array([1, 0, 0]), np.array([0, 1, 0]), np.array([0, 0, 1])):
            final_axis = np.multiply(axis, 2 * radius)
            print(axis)
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
                print(cur_axis, prev_axis)
                len_r = float(np.linalg.norm(rotation_axis))
                print(len_r)
                if len_r < 0.00001:
                    break
                rotation_axis = np.multiply(rotation_axis, 1 / len_r)

                position_delta = np.subtract(cur_axis, prev_axis)
                len_a = float(np.linalg.norm(position_delta))

                len_b = float(np.linalg.norm(cur_axis))
                len_c = float(np.linalg.norm(prev_axis))

                rotation_angle = math.acos((len_a ** 2 - len_b ** 2 - len_c ** 2) / (-2.0 * len_b * len_c))

                print(rotation_angle)

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

class TriangleController:
    def __init__(self, vertex_a: np.ndarray, vertex_b: np.ndarray, vertex_c: np.ndarray, radius: float=0.25):
        vertices = (vertex_a, vertex_b, vertex_c)
        self.__vertices: list[SphereController] = [SphereController(vertices[i], radius=radius) for i in range(3)]
    def get_triangle(self) -> Triangle:
        vertices = [self.__vertices[i].get_position() for i in range(3)]
        return Triangle(
            vertices=vertices
        )

def main():
    the_obb = OBB(
        centre=np.array([0, 0.11, 0]),
        half_extents=[0.5, 1.5, 0.5],
        axes=[
            np.array([1, 0, 0]),
            np.array([0, 1, 0]),
            np.array([0, 0, 1])
        ]
    )

    app = Ursina()

    obb_mesh = create_obb_mesh(the_obb)

    the_triangle = Triangle(
        vertices=[
            np.array([3.1, 2.7, 0.0]),
            np.array([0.5, 2.1, 0.8]),
            np.array([-0.1, -2.2, 3.1415926])
        ]
    )

    triangle_mesh = create_triangle_mesh(the_triangle)


    #cube = Entity(model=obb_mesh, color=hsv(300,1,1), scale=2, collider='box')
    #triangle = Entity(model=triangle_mesh, color=rgb(100, 40, 40), scale=1)

    # the_thing = OBBController(np.array([0, 0, 0]))

    the_other_thing = TriangleController(np.array([0, 0, 0]), np.array([1, 0, 0]), np.array([0, 0, 1]))

    # def spin():
    #     cube.animate('rotation_x', cube.rotation_x+360, duration=2, curve=curve.linear)
    #
    # cube.on_click = spin
    EditorCamera()  # add camera controls for orbiting and moving the camera

    app.run()

if __name__ == "__main__":
    main()
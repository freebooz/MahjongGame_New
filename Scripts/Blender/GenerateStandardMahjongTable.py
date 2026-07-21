"""Blender 5.2：按参考图生成深色木纹单柱全自动麻将桌与程序化 PBR 材质。

默认成品尺寸：940 x 940 x 780 mm（桌面宽 x 深 x 高），底座 550 x 550 mm。

本脚本是幂等资产生成器，默认会清空当前场景。直接在 Blender 的“脚本”
工作区打开并运行即可；也可以通过命令行执行：

    blender --background --python GenerateStandardMahjongTable.py

项目内推荐命令：

    blender --background \
        --python Scripts/Blender/GenerateStandardMahjongTable.py \
        -- H:/MahjongGame

可选参数（必须放在 Blender 的 ``--`` 之后）：

    --output-dir PATH   指定输出目录
    --no-render         不渲染预览图
    --no-save           不保存 .blend 文件
    --export-glb        额外导出 GLB（程序化纹理在 GLB 中可能被简化）
    --keep-scene        保留当前场景，只替换同名生成集合

生成结果默认位于 ``SourceArt/3D/MahjongTable``。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable

import bpy
from mathutils import Vector


SCRIPT_VERSION = "2.0.0"
MODEL_COLLECTION_NAME = "MG_MahjongTable"
PRESENTATION_COLLECTION_NAME = "MG_MahjongTable_Presentation"
MODEL_ROOT_NAME = "SM_StandardMahjongTable"


@dataclass(frozen=True)
class TableDimensions:
    """参考图全自动麻将桌的公制尺寸；所有数值均为米。"""

    outer_width: float = 0.940
    outer_depth: float = 0.940
    tabletop_height: float = 0.780
    bezel_width: float = 0.070
    bumper_width: float = 0.020
    bezel_height: float = 0.032
    felt_thickness: float = 0.012
    felt_top: float = 0.765
    shroud_top: float = 0.748
    shroud_bottom: float = 0.595
    shroud_bottom_width: float = 0.900
    shroud_bottom_depth: float = 0.900
    corner_chamfer: float = 0.035
    side_panel_width: float = 0.720
    side_panel_height: float = 0.082
    side_panel_depth: float = 0.008
    pedestal_width: float = 0.190
    pedestal_depth: float = 0.190
    pedestal_bottom: float = 0.115
    pedestal_top: float = 0.595
    plinth_width: float = 0.550
    plinth_depth: float = 0.550
    plinth_height: float = 0.040
    plinth_slope_height: float = 0.070
    center_lift_radius: float = 0.062

    @property
    def playing_width(self) -> float:
        return self.outer_width - 2.0 * (self.bezel_width + self.bumper_width)

    @property
    def playing_depth(self) -> float:
        return self.outer_depth - 2.0 * (self.bezel_width + self.bumper_width)


def blender_arguments() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "project_root",
        nargs="?",
        help="项目根目录；默认按脚本所在的 Scripts/Blender 向上推导",
    )
    parser.add_argument("--output-dir", help="生成文件输出目录")
    parser.add_argument("--no-render", action="store_true", help="不生成预览图")
    parser.add_argument("--no-save", action="store_true", help="不保存 Blender 文件")
    parser.add_argument("--export-glb", action="store_true", help="额外导出 GLB")
    parser.add_argument("--keep-scene", action="store_true", help="保留场景中的非生成对象")
    return parser.parse_args(argv)


def inferred_project_root() -> Path:
    try:
        return Path(__file__).resolve().parents[2]
    except (NameError, IndexError):
        if bpy.data.filepath:
            return Path(bpy.data.filepath).resolve().parent
        return Path.cwd()


def remove_collection(name: str) -> None:
    collection = bpy.data.collections.get(name)
    if collection is None:
        return
    for obj in list(collection.all_objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.collections.remove(collection)


def reset_scene(keep_scene: bool) -> None:
    if keep_scene:
        remove_collection(MODEL_COLLECTION_NAME)
        remove_collection(PRESENTATION_COLLECTION_NAME)
        for material in list(bpy.data.materials):
            if material.users == 0 and material.get("generator") == "GenerateStandardMahjongTable.py":
                bpy.data.materials.remove(material)
        return

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for collection in list(bpy.data.collections):
        bpy.data.collections.remove(collection)
    for datablocks in (
        bpy.data.meshes,
        bpy.data.curves,
        bpy.data.materials,
        bpy.data.cameras,
        bpy.data.lights,
    ):
        for datablock in list(datablocks):
            if datablock.users == 0:
                datablocks.remove(datablock)


def configure_scene() -> None:
    scene = bpy.context.scene
    scene.name = "MahjongTable_AssetScene"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.length_unit = "CENTIMETERS"
    scene.unit_settings.scale_length = 1.0
    scene.render.resolution_x = 960
    scene.render.resolution_y = 960
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False

    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue

    try:
        scene.view_settings.look = "AgX - Medium High Contrast"
    except TypeError:
        pass

    world = scene.world or bpy.data.worlds.new("MahjongTable_World")
    scene.world = world
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    if background:
        background.inputs["Color"].default_value = (0.012, 0.018, 0.016, 1.0)
        background.inputs["Strength"].default_value = 0.14


def create_collection(name: str) -> bpy.types.Collection:
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    return collection


def move_to_collection(obj: bpy.types.Object, collection: bpy.types.Collection) -> None:
    for owner in list(obj.users_collection):
        owner.objects.unlink(obj)
    collection.objects.link(obj)


def set_node_input(node: bpy.types.Node, name: str, value) -> None:
    socket = node.inputs.get(name)
    if socket is not None:
        socket.default_value = value


def base_material(
    name: str,
    color: tuple[float, float, float, float],
    metallic: float,
    roughness: float,
) -> tuple[bpy.types.Material, bpy.types.Node, bpy.types.NodeTree]:
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    material.diffuse_color = color
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    output.name = "Material Output"
    output.label = "PBR Output"
    output.location = (680.0, 0.0)

    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.name = "Principled BSDF"
    shader.label = "PBR Principled BSDF"
    shader.location = (380.0, 0.0)
    set_node_input(shader, "Base Color", color)
    set_node_input(shader, "Metallic", metallic)
    set_node_input(shader, "Roughness", roughness)
    set_node_input(shader, "IOR", 1.46)
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])

    material["pbr_workflow"] = "metallic_roughness"
    material["generator"] = "GenerateStandardMahjongTable.py"
    return material, shader, material.node_tree


def create_felt_material() -> bpy.types.Material:
    material, shader, tree = base_material(
        "M_Table_Felt_Green_PBR",
        (0.018, 0.205, 0.087, 1.0),
        metallic=0.0,
        roughness=0.82,
    )
    nodes, links = tree.nodes, tree.links
    set_node_input(shader, "Sheen Weight", 0.025)
    set_node_input(shader, "Sheen Roughness", 0.82)
    # Blender 5.2 的公开输入名为 Specular IOR Level；同时保留旧名称兼容预览版。
    set_node_input(shader, "Specular IOR Level", 0.02)
    set_node_input(shader, "IOR Level", 0.02)

    texcoord = nodes.new("ShaderNodeTexCoord")
    texcoord.location = (-760.0, 40.0)
    noise = nodes.new("ShaderNodeTexNoise")
    noise.name = "Felt Microfiber"
    noise.label = "Felt Microfiber"
    noise.location = (-550.0, 40.0)
    set_node_input(noise, "Scale", 185.0)
    set_node_input(noise, "Detail", 3.0)
    set_node_input(noise, "Roughness", 0.68)

    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.name = "Felt Green Variation"
    ramp.location = (-280.0, 100.0)
    ramp.color_ramp.elements[0].position = 0.22
    ramp.color_ramp.elements[0].color = (0.0002, 0.008, 0.0005, 1.0)
    ramp.color_ramp.elements[1].position = 0.78
    ramp.color_ramp.elements[1].color = (0.0012, 0.080, 0.008, 1.0)
    middle = ramp.color_ramp.elements.new(0.50)
    middle.color = (0.0005, 0.035, 0.0025, 1.0)

    bump = nodes.new("ShaderNodeBump")
    bump.name = "Felt Fiber Normal"
    bump.location = (80.0, -150.0)
    bump.inputs["Strength"].default_value = 0.24
    bump.inputs["Distance"].default_value = 0.0011

    links.new(texcoord.outputs["Generated"], noise.inputs["Vector"])
    links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], shader.inputs["Base Color"])
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], shader.inputs["Normal"])
    material["surface"] = "woven_wool_felt"
    return material




def create_powder_metal_material() -> bpy.types.Material:
    material, shader, tree = base_material(
        "M_Table_BlackPowderMetal_PBR",
        (0.018, 0.022, 0.021, 1.0),
        metallic=0.72,
        roughness=0.31,
    )
    nodes, links = tree.nodes, tree.links
    texcoord = nodes.new("ShaderNodeTexCoord")
    texcoord.location = (-550.0, 0.0)
    noise = nodes.new("ShaderNodeTexNoise")
    noise.name = "Powder Coat Grain"
    noise.location = (-340.0, 0.0)
    set_node_input(noise, "Scale", 145.0)
    set_node_input(noise, "Detail", 2.2)
    set_node_input(noise, "Roughness", 0.64)
    bump = nodes.new("ShaderNodeBump")
    bump.name = "Powder Coat Normal"
    bump.location = (80.0, -140.0)
    bump.inputs["Strength"].default_value = 0.10
    bump.inputs["Distance"].default_value = 0.00045
    links.new(texcoord.outputs["Generated"], noise.inputs["Vector"])
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], shader.inputs["Normal"])
    material["surface"] = "powder_coated_steel"
    return material




def create_glossy_black_material() -> bpy.types.Material:
    material, shader, _tree = base_material(
        "M_Table_PianoBlackMetal_PBR",
        (0.006, 0.008, 0.008, 1.0),
        metallic=0.52,
        roughness=0.19,
    )
    set_node_input(shader, "Coat Weight", 0.38)
    set_node_input(shader, "Coat Roughness", 0.14)
    material["surface"] = "gloss_black_painted_metal"
    return material


def create_wood_material() -> bpy.types.Material:
    material, shader, tree = base_material(
        "M_Table_SmokedOak_PBR",
        (0.105, 0.055, 0.026, 1.0),
        metallic=0.0,
        roughness=0.38,
    )
    nodes, links = tree.nodes, tree.links
    set_node_input(shader, "Coat Weight", 0.24)
    set_node_input(shader, "Coat Roughness", 0.28)
    texcoord = nodes.new("ShaderNodeTexCoord")
    texcoord.location = (-860.0, 40.0)
    mapping = nodes.new("ShaderNodeMapping")
    mapping.name = "Horizontal Wood Grain"
    mapping.location = (-670.0, 40.0)
    mapping.inputs["Scale"].default_value = (1.0, 5.5, 2.5)
    noise = nodes.new("ShaderNodeTexNoise")
    noise.name = "Oak Pores"
    noise.location = (-440.0, 20.0)
    set_node_input(noise, "Scale", 4.2)
    set_node_input(noise, "Detail", 6.0)
    set_node_input(noise, "Roughness", 0.72)
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.name = "Smoked Oak Color"
    ramp.location = (-160.0, 90.0)
    ramp.color_ramp.elements[0].color = (0.018, 0.014, 0.010, 1.0)
    ramp.color_ramp.elements[1].color = (0.110, 0.078, 0.052, 1.0)
    middle = ramp.color_ramp.elements.new(0.52)
    middle.color = (0.052, 0.038, 0.026, 1.0)
    bump = nodes.new("ShaderNodeBump")
    bump.name = "Wood Grain Normal"
    bump.location = (100.0, -150.0)
    bump.inputs["Strength"].default_value = 0.14
    bump.inputs["Distance"].default_value = 0.0007
    links.new(texcoord.outputs["Generated"], mapping.inputs["Vector"])
    links.new(mapping.outputs["Vector"], noise.inputs["Vector"])
    links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], shader.inputs["Base Color"])
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], shader.inputs["Normal"])
    material["surface"] = "smoked_oak_veneer"
    return material


def create_grille_material() -> bpy.types.Material:
    material, shader, tree = base_material(
        "M_Table_PerforatedGrille_PBR",
        (0.055, 0.060, 0.058, 1.0),
        metallic=0.30,
        roughness=0.40,
    )
    nodes, links = tree.nodes, tree.links
    texcoord = nodes.new("ShaderNodeTexCoord")
    texcoord.location = (-620.0, 20.0)
    brick = nodes.new("ShaderNodeTexBrick")
    brick.name = "Perforation Grid"
    brick.location = (-400.0, 20.0)
    brick.offset = 0.5
    brick.offset_frequency = 2
    set_node_input(brick, "Color1", (0.28, 0.30, 0.29, 1.0))
    set_node_input(brick, "Color2", (0.14, 0.16, 0.15, 1.0))
    set_node_input(brick, "Mortar", (0.001, 0.001, 0.001, 1.0))
    set_node_input(brick, "Scale", 62.0)
    set_node_input(brick, "Mortar Size", 0.025)
    set_node_input(brick, "Mortar Smooth", 0.02)
    bump = nodes.new("ShaderNodeBump")
    bump.name = "Recessed Perforations"
    bump.location = (80.0, -120.0)
    bump.invert = True
    bump.inputs["Strength"].default_value = 0.34
    bump.inputs["Distance"].default_value = 0.0012
    links.new(texcoord.outputs["Generated"], brick.inputs["Vector"])
    links.new(brick.outputs["Color"], shader.inputs["Base Color"])
    links.new(brick.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], shader.inputs["Normal"])
    material["surface"] = "perforated_metal_grille"
    return material




def create_simple_materials() -> dict[str, bpy.types.Material]:
    felt = create_felt_material()
    metal = create_powder_metal_material()
    glossy_black = create_glossy_black_material()
    wood = create_wood_material()
    grille = create_grille_material()

    rubber, _shader, _tree = base_material(
        "M_Table_Rubber_PBR",
        (0.010, 0.012, 0.011, 1.0),
        metallic=0.0,
        roughness=0.78,
    )
    rubber["surface"] = "anti_slip_rubber"

    chrome, chrome_shader, _tree = base_material(
        "M_Table_Chrome_PBR",
        (0.62, 0.67, 0.69, 1.0),
        metallic=1.0,
        roughness=0.13,
    )
    set_node_input(chrome_shader, "Coat Weight", 0.18)
    chrome["surface"] = "polished_chrome"

    deck, _shader, _tree = base_material(
        "M_Table_DeckComposite_PBR",
        (0.020, 0.030, 0.025, 1.0),
        metallic=0.0,
        roughness=0.62,
    )
    deck["surface"] = "sealed_composite"

    indicator_red, red_shader, _tree = base_material(
        "M_Table_IndicatorRed_PBR",
        (0.32, 0.003, 0.004, 1.0),
        metallic=0.0,
        roughness=0.18,
    )
    set_node_input(red_shader, "Coat Weight", 0.38)
    indicator_red["surface"] = "red_indicator_lens"

    indicator_green, green_shader, _tree = base_material(
        "M_Table_IndicatorGreen_PBR",
        (0.002, 0.30, 0.018, 1.0),
        metallic=0.0,
        roughness=0.18,
    )
    set_node_input(green_shader, "Coat Weight", 0.38)
    indicator_green["surface"] = "green_indicator_lens"

    controller_face, face_shader, _tree = base_material(
        "M_Table_ControllerFace_PBR",
        (0.34, 0.37, 0.36, 1.0),
        metallic=0.30,
        roughness=0.29,
    )
    set_node_input(face_shader, "Coat Weight", 0.18)
    controller_face["surface"] = "satin_controller_face"

    floor, _shader, _tree = base_material(
        "M_PreviewFloor_Neutral",
        (0.055, 0.060, 0.056, 1.0),
        metallic=0.0,
        roughness=0.70,
    )
    return {
        "felt": felt,
        "metal": metal,
        "glossy_black": glossy_black,
        "wood": wood,
        "grille": grille,
        "rubber": rubber,
        "chrome": chrome,
        "deck": deck,
        "indicator_red": indicator_red,
        "indicator_green": indicator_green,
        "controller_face": controller_face,
        "floor": floor,
    }


def apply_bevel(obj: bpy.types.Object, width: float, segments: int = 5) -> None:
    if width <= 0.0:
        return
    modifier = obj.modifiers.new("ProductionBevel", "BEVEL")
    modifier.width = width
    modifier.segments = segments
    modifier.limit_method = "ANGLE"
    modifier.angle_limit = math.radians(24.0)
    modifier.harden_normals = True
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.select_set(False)


def create_rounded_box(
    name: str,
    dimensions: tuple[float, float, float],
    location: tuple[float, float, float],
    material: bpy.types.Material,
    bevel: float,
    collection: bpy.types.Collection,
    parent: bpy.types.Object,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.object
    obj.name = name
    obj.data.name = f"{name}_Mesh"
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    move_to_collection(obj, collection)
    obj.parent = parent
    obj.data.materials.append(material)
    apply_bevel(obj, min(bevel, min(dimensions) * 0.24))
    return obj




def chamfered_outline(width: float, depth: float, chamfer: float, z: float) -> list[tuple[float, float, float]]:
    half_width = width * 0.5
    half_depth = depth * 0.5
    cut = min(chamfer, half_width * 0.45, half_depth * 0.45)
    return [
        (-half_width + cut, -half_depth, z),
        (half_width - cut, -half_depth, z),
        (half_width, -half_depth + cut, z),
        (half_width, half_depth - cut, z),
        (half_width - cut, half_depth, z),
        (-half_width + cut, half_depth, z),
        (-half_width, half_depth - cut, z),
        (-half_width, -half_depth + cut, z),
    ]


def create_chamfered_frustum(
    name: str,
    bottom_dimensions: tuple[float, float],
    top_dimensions: tuple[float, float],
    height: float,
    location: tuple[float, float, float],
    bottom_chamfer: float,
    top_chamfer: float,
    material: bpy.types.Material,
    bevel: float,
    collection: bpy.types.Collection,
    parent: bpy.types.Object,
) -> bpy.types.Object:
    bottom = chamfered_outline(bottom_dimensions[0], bottom_dimensions[1], bottom_chamfer, -height * 0.5)
    top = chamfered_outline(top_dimensions[0], top_dimensions[1], top_chamfer, height * 0.5)
    vertices = bottom + top
    faces: list[tuple[int, ...]] = [tuple(reversed(range(8))), tuple(range(8, 16))]
    for index in range(8):
        following = (index + 1) % 8
        faces.append((index, following, following + 8, index + 8))
    mesh = bpy.data.meshes.new(f"{name}_Mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(material)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    obj.location = location
    collection.objects.link(obj)
    obj.parent = parent
    apply_bevel(obj, bevel, segments=5)
    return obj


def create_side_panel(
    name: str,
    width: float,
    height: float,
    depth: float,
    location: tuple[float, float, float],
    rotation_z: float,
    material: bpy.types.Material,
    collection: bpy.types.Collection,
    parent: bpy.types.Object,
) -> bpy.types.Object:
    half_width = width * 0.5
    half_height = height * 0.5
    cut_x = min(0.045, width * 0.12)
    cut_z = min(0.018, height * 0.24)
    outline = [
        (-half_width + cut_x, -half_height),
        (half_width - cut_x, -half_height),
        (half_width, -half_height + cut_z),
        (half_width, half_height - cut_z),
        (half_width - cut_x, half_height),
        (-half_width + cut_x, half_height),
        (-half_width, half_height - cut_z),
        (-half_width, -half_height + cut_z),
    ]
    vertices = [(x, -depth * 0.5, z) for x, z in outline]
    vertices += [(x, depth * 0.5, z) for x, z in outline]
    faces: list[tuple[int, ...]] = [tuple(reversed(range(8))), tuple(range(8, 16))]
    for index in range(8):
        following = (index + 1) % 8
        faces.append((index, following, following + 8, index + 8))
    mesh = bpy.data.meshes.new(f"{name}_Mesh")
    mesh.from_pydata(vertices, [], faces)
    mesh.materials.append(material)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    obj.location = location
    obj.rotation_euler.z = rotation_z
    collection.objects.link(obj)
    obj.parent = parent
    apply_bevel(obj, min(0.004, depth * 0.35), segments=4)
    return obj


def create_cylinder(
    name: str,
    radius: float,
    depth: float,
    location: tuple[float, float, float],
    material: bpy.types.Material,
    collection: bpy.types.Collection,
    parent: bpy.types.Object,
    rotation: tuple[float, float, float] = (0.0, 0.0, 0.0),
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=64,
        radius=radius,
        depth=depth,
        location=location,
        rotation=rotation,
    )
    obj = bpy.context.object
    obj.name = name
    obj.data.name = f"{name}_Mesh"
    move_to_collection(obj, collection)
    obj.parent = parent
    obj.data.materials.append(material)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def create_torus(
    name: str,
    major_radius: float,
    minor_radius: float,
    location: tuple[float, float, float],
    material: bpy.types.Material,
    collection: bpy.types.Collection,
    parent: bpy.types.Object,
) -> bpy.types.Object:
    bpy.ops.mesh.primitive_torus_add(
        major_radius=major_radius,
        minor_radius=minor_radius,
        major_segments=64,
        minor_segments=12,
        location=location,
    )
    obj = bpy.context.object
    obj.name = name
    obj.data.name = f"{name}_Mesh"
    move_to_collection(obj, collection)
    obj.parent = parent
    obj.data.materials.append(material)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def new_group(name: str, collection: bpy.types.Collection, parent: bpy.types.Object) -> bpy.types.Object:
    group = bpy.data.objects.new(name, None)
    group.empty_display_type = "PLAIN_AXES"
    group.empty_display_size = 0.08
    collection.objects.link(group)
    group.parent = parent
    return group






def build_table(
    dimensions: TableDimensions,
    collection: bpy.types.Collection,
    materials: dict[str, bpy.types.Material],
) -> tuple[bpy.types.Object, list[bpy.types.Object]]:
    """按 94 cm 深色木纹参考图构建单柱全自动麻将桌。"""
    root = bpy.data.objects.new(MODEL_ROOT_NAME, None)
    root.empty_display_type = "CUBE"
    root.empty_display_size = 0.12
    collection.objects.link(root)

    tabletop_group = new_group("AutomaticTabletop_Assembly", collection, root)
    shell_group = new_group("WoodCabinet_Assembly", collection, root)
    pedestal_group = new_group("PedestalAndCaster_Assembly", collection, root)
    control_group = new_group("ControllerAndGrille_Assembly", collection, root)
    details_group = new_group("CenterLift_Assembly", collection, root)
    objects: list[bpy.types.Object] = []
    d = dimensions

    # 桌面机箱：上宽下窄的深色机身，四面嵌入烟熏橡木饰板。
    shroud_height = d.shroud_top - d.shroud_bottom
    objects.append(
        create_chamfered_frustum(
            "Table_DarkCabinetShell",
            (d.shroud_bottom_width, d.shroud_bottom_depth),
            (d.outer_width, d.outer_depth),
            shroud_height,
            (0.0, 0.0, d.shroud_bottom + shroud_height * 0.5),
            bottom_chamfer=d.corner_chamfer * 1.12,
            top_chamfer=d.corner_chamfer,
            material=materials["glossy_black"],
            bevel=0.007,
            collection=collection,
            parent=shell_group,
        )
    )

    panel_z = d.shroud_bottom + shroud_height * 0.50
    panel_face = 0.458
    panel_specs = (
        ("Table_WoodPanel_North", (0.0, panel_face, panel_z), 0.0),
        ("Table_WoodPanel_South", (0.0, -panel_face, panel_z), 0.0),
        ("Table_WoodPanel_East", (panel_face, 0.0, panel_z), math.radians(90.0)),
        ("Table_WoodPanel_West", (-panel_face, 0.0, panel_z), math.radians(90.0)),
    )
    for name, location, rotation_z in panel_specs:
        objects.append(
            create_side_panel(
                name,
                d.side_panel_width,
                d.side_panel_height,
                d.side_panel_depth,
                location,
                rotation_z,
                materials["wood"],
                collection,
                shell_group,
            )
        )

    # 上下两道黑色压条夹住木纹面板，对应参考图的薄层机箱结构。
    trim_width = 0.010
    trim_height = 0.018
    trim_z_values = (d.shroud_bottom + 0.018, d.shroud_top - 0.020)
    for level_index, trim_z in enumerate(trim_z_values, start=1):
        for name, size, location in (
            ("North", (d.outer_width - 0.090, trim_width, trim_height), (0.0, 0.462, trim_z)),
            ("South", (d.outer_width - 0.090, trim_width, trim_height), (0.0, -0.462, trim_z)),
            ("East", (trim_width, d.outer_depth - 0.090, trim_height), (0.462, 0.0, trim_z)),
            ("West", (trim_width, d.outer_depth - 0.090, trim_height), (-0.462, 0.0, trim_z)),
        ):
            objects.append(
                create_rounded_box(
                    f"Table_CabinetTrim_{level_index}_{name}",
                    size,
                    location,
                    materials["metal"],
                    0.003,
                    collection,
                    shell_group,
                )
            )

    # 下沉式绿色桌布和内圈防撞条。
    deck_height = d.felt_top - d.felt_thickness - d.shroud_top + 0.006
    objects.append(
        create_chamfered_frustum(
            "Table_RecessedDeck",
            (d.playing_width + 0.054, d.playing_depth + 0.054),
            (d.playing_width + 0.054, d.playing_depth + 0.054),
            deck_height,
            (0.0, 0.0, d.shroud_top + deck_height * 0.5 - 0.003),
            bottom_chamfer=0.025,
            top_chamfer=0.025,
            material=materials["deck"],
            bevel=0.004,
            collection=collection,
            parent=tabletop_group,
        )
    )
    objects.append(
        create_chamfered_frustum(
            "Table_GreenFeltSurface",
            (d.playing_width, d.playing_depth),
            (d.playing_width, d.playing_depth),
            d.felt_thickness,
            (0.0, 0.0, d.felt_top - d.felt_thickness * 0.5),
            bottom_chamfer=0.020,
            top_chamfer=0.020,
            material=materials["felt"],
            bevel=0.0025,
            collection=collection,
            parent=tabletop_group,
        )
    )

    bezel_z = d.shroud_top + d.bezel_height * 0.5
    y_bezel = d.outer_depth * 0.5 - d.bezel_width * 0.5
    x_bezel = d.outer_width * 0.5 - d.bezel_width * 0.5
    for name, size, location in (
        ("Table_TopFrame_North", (d.outer_width, d.bezel_width, d.bezel_height), (0.0, y_bezel, bezel_z)),
        ("Table_TopFrame_South", (d.outer_width, d.bezel_width, d.bezel_height), (0.0, -y_bezel, bezel_z)),
        ("Table_TopFrame_East", (d.bezel_width, d.outer_depth - 2.0 * d.bezel_width, d.bezel_height), (x_bezel, 0.0, bezel_z)),
        ("Table_TopFrame_West", (d.bezel_width, d.outer_depth - 2.0 * d.bezel_width, d.bezel_height), (-x_bezel, 0.0, bezel_z)),
    ):
        objects.append(
            create_rounded_box(
                name,
                size,
                location,
                materials["metal"],
                0.010,
                collection,
                tabletop_group,
            )
        )

    bumper_height = 0.020
    bumper_z = d.tabletop_height - bumper_height * 0.5 - 0.004
    y_bumper = d.playing_depth * 0.5 + d.bumper_width * 0.5
    x_bumper = d.playing_width * 0.5 + d.bumper_width * 0.5
    for name, size, location in (
        ("Table_InnerRail_North", (d.playing_width + 2.0 * d.bumper_width, d.bumper_width, bumper_height), (0.0, y_bumper, bumper_z)),
        ("Table_InnerRail_South", (d.playing_width + 2.0 * d.bumper_width, d.bumper_width, bumper_height), (0.0, -y_bumper, bumper_z)),
        ("Table_InnerRail_East", (d.bumper_width, d.playing_depth, bumper_height), (x_bumper, 0.0, bumper_z)),
        ("Table_InnerRail_West", (d.bumper_width, d.playing_depth, bumper_height), (-x_bumper, 0.0, bumper_z)),
    ):
        objects.append(
            create_rounded_box(
                name,
                size,
                location,
                materials["glossy_black"],
                0.004,
                collection,
                tabletop_group,
            )
        )

    # 550 x 550 mm 底座、斜面台阶和黑色中央立柱。
    caster_radius = 0.025
    base_bottom = 0.035
    objects.append(
        create_chamfered_frustum(
            "Table_550mmPedestalFoot",
            (d.plinth_width, d.plinth_depth),
            (d.plinth_width * 0.965, d.plinth_depth * 0.965),
            d.plinth_height,
            (0.0, 0.0, base_bottom + d.plinth_height * 0.5),
            bottom_chamfer=0.035,
            top_chamfer=0.035,
            material=materials["glossy_black"],
            bevel=0.008,
            collection=collection,
            parent=pedestal_group,
        )
    )
    slope_bottom = base_bottom + d.plinth_height - 0.004
    objects.append(
        create_chamfered_frustum(
            "Table_PedestalSlopedBase",
            (d.plinth_width * 0.91, d.plinth_depth * 0.91),
            (0.315, 0.315),
            d.plinth_slope_height,
            (0.0, 0.0, slope_bottom + d.plinth_slope_height * 0.5),
            bottom_chamfer=0.040,
            top_chamfer=0.028,
            material=materials["metal"],
            bevel=0.007,
            collection=collection,
            parent=pedestal_group,
        )
    )
    objects.append(
        create_rounded_box(
            "Table_ColumnLowerCollar",
            (0.255, 0.255, 0.024),
            (0.0, 0.0, d.pedestal_bottom - 0.008),
            materials["glossy_black"],
            0.007,
            collection,
            pedestal_group,
        )
    )
    column_height = d.pedestal_top - d.pedestal_bottom
    objects.append(
        create_rounded_box(
            "Table_CentralColumn",
            (d.pedestal_width, d.pedestal_depth, column_height),
            (0.0, 0.0, d.pedestal_bottom + column_height * 0.5),
            materials["glossy_black"],
            0.012,
            collection,
            pedestal_group,
        )
    )

    # 四个带程序化网孔材质的立柱面板。
    grille_width = 0.118
    grille_height = 0.292
    grille_z = d.pedestal_bottom + column_height * 0.48
    for name, location, rotation_z in (
        ("Table_ColumnGrille_Front", (0.0, -d.pedestal_depth * 0.5 - 0.003, grille_z), 0.0),
        ("Table_ColumnGrille_Back", (0.0, d.pedestal_depth * 0.5 + 0.003, grille_z), 0.0),
        ("Table_ColumnGrille_Right", (d.pedestal_width * 0.5 + 0.003, 0.0, grille_z), math.radians(90.0)),
        ("Table_ColumnGrille_Left", (-d.pedestal_width * 0.5 - 0.003, 0.0, grille_z), math.radians(90.0)),
    ):
        objects.append(
            create_side_panel(
                name,
                grille_width,
                grille_height,
                0.006,
                location,
                rotation_z,
                materials["grille"],
                collection,
                control_group,
            )
        )

    grille_front = -d.pedestal_depth * 0.5 - 0.0065
    grille_right = d.pedestal_width * 0.5 + 0.0065
    for index, offset in enumerate((-0.044, -0.022, 0.0, 0.022, 0.044), start=1):
        objects.append(
            create_rounded_box(
                f"Table_GrilleFront_V{index}",
                (0.0022, 0.0018, 0.252),
                (offset, grille_front, grille_z),
                materials["metal"],
                0.0006,
                collection,
                control_group,
            )
        )
        objects.append(
            create_rounded_box(
                f"Table_GrilleRight_V{index}",
                (0.0018, 0.0022, 0.252),
                (grille_right, offset, grille_z),
                materials["metal"],
                0.0006,
                collection,
                control_group,
            )
        )
    for index in range(10):
        z = grille_z - 0.117 + index * 0.026
        objects.append(
            create_rounded_box(
                f"Table_GrilleFront_H{index + 1}",
                (0.100, 0.0018, 0.0022),
                (0.0, grille_front, z),
                materials["metal"],
                0.0006,
                collection,
                control_group,
            )
        )
        objects.append(
            create_rounded_box(
                f"Table_GrilleRight_H{index + 1}",
                (0.0018, 0.100, 0.0022),
                (grille_right, 0.0, z),
                materials["metal"],
                0.0006,
                collection,
                control_group,
            )
        )

    capital_height = 0.050
    objects.append(
        create_chamfered_frustum(
            "Table_PedestalCapital",
            (0.220, 0.220),
            (0.330, 0.330),
            capital_height,
            (0.0, 0.0, d.pedestal_top - capital_height * 0.5),
            bottom_chamfer=0.022,
            top_chamfer=0.035,
            material=materials["glossy_black"],
            bevel=0.006,
            collection=collection,
            parent=pedestal_group,
        )
    )

    # 前置控制盒与三枚物理按钮。
    controller_z = d.shroud_bottom - 0.045
    objects.append(
        create_rounded_box(
            "Table_FrontControllerHousing",
            (0.300, 0.160, 0.068),
            (0.0, -0.320, controller_z),
            materials["glossy_black"],
            0.009,
            collection,
            control_group,
        )
    )
    objects.append(
        create_rounded_box(
            "Table_FrontControllerFace",
            (0.205, 0.006, 0.038),
            (0.0, -0.403, controller_z),
            materials["controller_face"],
            0.005,
            collection,
            control_group,
        )
    )
    for index, (x, material_key) in enumerate(
        ((-0.055, "indicator_green"), (0.0, "glossy_black"), (0.055, "indicator_red")),
        start=1,
    ):
        objects.append(
            create_cylinder(
                f"Table_ControllerButton_{index}",
                radius=0.011,
                depth=0.008,
                location=(x, -0.408, controller_z),
                material=materials[material_key],
                collection=collection,
                parent=control_group,
                rotation=(math.radians(90.0), 0.0, 0.0),
            )
        )

    # 四组脚轮与支架，所有轮子保持在 550 mm 底座外廓内。
    caster_positions = (
        ("NE", 0.245, 0.282),
        ("NW", -0.245, 0.282),
        ("SE", 0.245, -0.282),
        ("SW", -0.245, -0.282),
    )
    for suffix, x, y in caster_positions:
        objects.append(
            create_rounded_box(
                f"Table_CasterBracket_{suffix}",
                (0.040, 0.040, 0.034),
                (x, y, 0.052),
                materials["metal"],
                0.005,
                collection,
                pedestal_group,
            )
        )
        objects.append(
            create_cylinder(
                f"Table_CasterWheel_{suffix}",
                radius=caster_radius,
                depth=0.018,
                location=(x, y, caster_radius),
                material=materials["rubber"],
                collection=collection,
                parent=pedestal_group,
                rotation=(0.0, math.radians(90.0), 0.0),
            )
        )

    # 中央升降盘：黑底、红色功能环和绿色中心按钮。
    hub_z = d.felt_top + 0.0015
    objects.append(
        create_cylinder(
            "Table_CenterLiftBase",
            radius=d.center_lift_radius,
            depth=0.003,
            location=(0.0, 0.0, hub_z),
            material=materials["glossy_black"],
            collection=collection,
            parent=details_group,
        )
    )
    objects.append(
        create_torus(
            "Table_CenterLiftRedRing",
            major_radius=0.043,
            minor_radius=0.0045,
            location=(0.0, 0.0, hub_z + 0.0045),
            material=materials["indicator_red"],
            collection=collection,
            parent=details_group,
        )
    )
    objects.append(
        create_torus(
            "Table_CenterLiftChromeRing",
            major_radius=0.030,
            minor_radius=0.0025,
            location=(0.0, 0.0, hub_z + 0.0060),
            material=materials["chrome"],
            collection=collection,
            parent=details_group,
        )
    )
    objects.append(
        create_cylinder(
            "Table_CenterLiftButton",
            radius=0.018,
            depth=0.003,
            location=(0.0, 0.0, hub_z + 0.0065),
            material=materials["indicator_green"],
            collection=collection,
            parent=details_group,
        )
    )

    root["asset_type"] = "automatic_single_pedestal_mahjong_table"
    root["reference_style"] = "dark frame, smoked oak panels, perforated column, controller, casters"
    root["dimensions_mm"] = [
        round(d.outer_width * 1000.0),
        round(d.outer_depth * 1000.0),
        round(d.tabletop_height * 1000.0),
    ]
    root["playing_surface_mm"] = [
        round(d.playing_width * 1000.0),
        round(d.playing_depth * 1000.0),
    ]
    root["base_footprint_mm"] = [round(d.plinth_width * 1000.0), round(d.plinth_depth * 1000.0)]
    root["dimension_basis"] = "reference_image_940x940x780_mm_base_550x550_mm"
    root["units"] = "meters"
    root["pivot"] = "floor_center"
    root["generator_version"] = SCRIPT_VERSION
    root["blender_compatibility"] = "5.2+"
    collection["asset_root"] = MODEL_ROOT_NAME
    collection["nominal_dimensions_mm"] = root["dimensions_mm"]
    return root, objects




def point_at(obj: bpy.types.Object, target: tuple[float, float, float]) -> None:
    direction = Vector(target) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def add_presentation_scene(
    collection: bpy.types.Collection,
    materials: dict[str, bpy.types.Material],
) -> bpy.types.Camera:
    bpy.ops.mesh.primitive_plane_add(size=4.0, location=(0.0, 0.0, -0.001))
    floor = bpy.context.object
    floor.name = "Preview_Floor"
    move_to_collection(floor, collection)
    floor.data.materials.append(materials["floor"])

    light_specs = (
        ("Preview_Key", "AREA", (-1.20, -1.15, 2.10), 220.0, 1.25),
        ("Preview_Fill", "AREA", (1.35, -0.30, 1.45), 95.0, 1.00),
        ("Preview_Rim", "AREA", (0.30, 1.35, 1.80), 150.0, 0.95),
        ("Preview_UnderFill", "AREA", (0.20, -1.10, 0.58), 85.0, 0.55),
    )
    for name, light_type, location, energy, size in light_specs:
        bpy.ops.object.light_add(type=light_type, location=location)
        light = bpy.context.object
        light.name = name
        light.data.energy = energy
        light.data.shape = "DISK"
        light.data.size = size
        move_to_collection(light, collection)
        point_at(light, (0.0, 0.0, 0.48))

    bpy.ops.object.camera_add(location=(1.42, -1.52, 1.24))
    camera = bpy.context.object
    camera.name = "Preview_Camera"
    camera.data.lens = 54.0
    camera.data.sensor_width = 36.0
    move_to_collection(camera, collection)
    point_at(camera, (0.0, 0.0, 0.43))
    bpy.context.scene.camera = camera
    return camera.data


def calculate_bounds(objects: Iterable[bpy.types.Object]) -> tuple[Vector, Vector]:
    bpy.context.view_layer.update()
    corners: list[Vector] = []
    for obj in objects:
        if obj.type != "MESH":
            continue
        corners.extend(obj.matrix_world @ Vector(corner) for corner in obj.bound_box)
    if not corners:
        return Vector((0.0, 0.0, 0.0)), Vector((0.0, 0.0, 0.0))
    minimum = Vector(tuple(min(corner[index] for corner in corners) for index in range(3)))
    maximum = Vector(tuple(max(corner[index] for corner in corners) for index in range(3)))
    return minimum, maximum


def select_model(objects: Iterable[bpy.types.Object]) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    first = None
    for obj in objects:
        if obj.type != "MESH":
            continue
        obj.select_set(True)
        first = first or obj
    if first is not None:
        bpy.context.view_layer.objects.active = first


def export_glb(path: Path, objects: list[bpy.types.Object]) -> None:
    select_model(objects)
    bpy.ops.export_scene.gltf(
        filepath=str(path),
        export_format="GLB",
        use_selection=True,
        export_apply=True,
        export_yup=True,
        export_materials="EXPORT",
    )


def file_record(path: Path, root: Path) -> dict[str, object]:
    payload = path.read_bytes()
    try:
        relative_path = path.relative_to(root).as_posix()
    except ValueError:
        relative_path = path.as_posix()
    return {
        "path": relative_path,
        "bytes": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest(),
    }


def write_manifest(
    path: Path,
    project_root: Path,
    dimensions: TableDimensions,
    root: bpy.types.Object,
    objects: list[bpy.types.Object],
    materials: dict[str, bpy.types.Material],
    generated_files: list[Path],
) -> None:
    minimum, maximum = calculate_bounds(objects)
    measured = maximum - minimum
    manifest = {
        "generator": "Blender 5.2 GenerateStandardMahjongTable.py",
        "generator_version": SCRIPT_VERSION,
        "blender_version": bpy.app.version_string,
        "asset_root": root.name,
        "nominal_dimensions_mm": [
            round(dimensions.outer_width * 1000.0, 3),
            round(dimensions.outer_depth * 1000.0, 3),
            round(dimensions.tabletop_height * 1000.0, 3),
        ],
        "measured_bounds_mm": {
            "minimum": [round(value * 1000.0, 3) for value in minimum],
            "maximum": [round(value * 1000.0, 3) for value in maximum],
            "size": [round(value * 1000.0, 3) for value in measured],
        },
        "playing_surface_mm": [
            round(dimensions.playing_width * 1000.0, 3),
            round(dimensions.playing_depth * 1000.0, 3),
        ],
        "base_footprint_mm": [
            round(dimensions.plinth_width * 1000.0, 3),
            round(dimensions.plinth_depth * 1000.0, 3),
        ],
        "dimensions_m": asdict(dimensions),
        "pivot": "floor_center",
        "mesh_object_count": sum(obj.type == "MESH" for obj in objects),
        "materials": sorted(material.name for material in materials.values() if material.users > 0),
        "material_workflow": "Principled BSDF metallic-roughness PBR; procedural Blender nodes",
        "files": [file_record(file, project_root) for file in generated_files if file.is_file()],
    }
    path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")


def main() -> None:
    args = blender_arguments()
    project_root = Path(args.project_root).resolve() if args.project_root else inferred_project_root()
    output_dir = (
        Path(args.output_dir).resolve()
        if args.output_dir
        else project_root / "SourceArt" / "3D" / "MahjongTable"
    )
    output_dir.mkdir(parents=True, exist_ok=True)

    dimensions = TableDimensions()
    reset_scene(keep_scene=args.keep_scene)
    configure_scene()
    model_collection = create_collection(MODEL_COLLECTION_NAME)
    presentation_collection = create_collection(PRESENTATION_COLLECTION_NAME)
    materials = create_simple_materials()
    root, objects = build_table(dimensions, model_collection, materials)
    add_presentation_scene(presentation_collection, materials)

    minimum, maximum = calculate_bounds(objects)
    measured = maximum - minimum
    expected = Vector((dimensions.outer_width, dimensions.outer_depth, dimensions.tabletop_height))
    tolerance = 0.0005
    if any(abs(measured[index] - expected[index]) > tolerance for index in range(3)):
        raise RuntimeError(
            "模型尺寸校验失败："
            f"实测 {tuple(round(value, 6) for value in measured)} m，"
            f"预期 {tuple(expected)} m"
        )

    generated_files: list[Path] = []
    preview_path = output_dir / "StandardMahjongTable_Preview.png"
    if not args.no_render:
        bpy.context.scene.render.filepath = str(preview_path)
        bpy.ops.render.render(write_still=True)
        generated_files.append(preview_path)

    blend_path = output_dir / "SM_StandardMahjongTable.blend"
    if not args.no_save:
        bpy.ops.wm.save_as_mainfile(filepath=str(blend_path), check_existing=False)
        # 资产生成器每次都会完整重建文件，不保留 Blender 自动产生的旧版 .blend1。
        backup_path = Path(f"{blend_path}1")
        if backup_path.is_file():
            backup_path.unlink()
        generated_files.append(blend_path)

    if args.export_glb:
        glb_path = output_dir / "SM_StandardMahjongTable.glb"
        export_glb(glb_path, objects)
        generated_files.append(glb_path)

    manifest_path = output_dir / "MahjongTableAssetManifest.json"
    write_manifest(
        manifest_path,
        project_root,
        dimensions,
        root,
        objects,
        materials,
        generated_files,
    )

    print(f"MAHJONG_TABLE_ASSETS_GENERATED={output_dir}")
    print(
        "NOMINAL_DIMENSIONS_MM="
        f"{dimensions.outer_width * 1000.0:.0f}x"
        f"{dimensions.outer_depth * 1000.0:.0f}x"
        f"{dimensions.tabletop_height * 1000.0:.0f}"
    )
    print(f"BASE_FOOTPRINT_MM={dimensions.plinth_width * 1000.0:.0f}x{dimensions.plinth_depth * 1000.0:.0f}")
    print(f"MEASURED_DIMENSIONS_MM={'x'.join(f'{value * 1000.0:.3f}' for value in measured)}")
    print(f"MESH_OBJECT_COUNT={sum(obj.type == 'MESH' for obj in objects)}")
    print(f"BLENDER_VERSION={bpy.app.version_string}")


if __name__ == "__main__":
    main()

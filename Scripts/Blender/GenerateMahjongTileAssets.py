"""Blender 5.1+：生成《贵阳捉鸡麻将》运行时麻将牌模型、PBR 贴图和预览图。"""

from __future__ import annotations

import hashlib
import json
import math
import os
import shutil
import struct
import sys
import zlib

import bpy


TILE_WIDTH_CM = 3.2
TILE_DEPTH_CM = 2.2
TILE_HEIGHT_CM = 4.4
BEVEL_CM = 0.18
TEXTURE_SIZE = 512


def project_root() -> str:
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if args:
        return os.path.abspath(args[0])
    return os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


ROOT = project_root()
OUTPUT_ROOT = os.path.join(ROOT, "SourceArt", "3D", "MahjongTiles")
TEXTURE_ROOT = os.path.join(OUTPUT_ROOT, "Textures")
FACE_ROOT = os.path.join(TEXTURE_ROOT, "Faces")
SOURCE_FACE_ROOT = os.path.join(ROOT, "SourceArt", "UI", "Tiles")


def ensure_directories() -> None:
    for path in (OUTPUT_ROOT, TEXTURE_ROOT, FACE_ROOT):
        os.makedirs(path, exist_ok=True)


def write_png_rgb(path: str, pixel) -> None:
    """不依赖 Pillow 写入 8-bit RGB PNG，便于 Blender 干净环境复现。"""
    raw = bytearray()
    for y in range(TEXTURE_SIZE):
        raw.append(0)
        for x in range(TEXTURE_SIZE):
            raw.extend(pixel(x, y))
    header = struct.pack(">IIBBBBB", TEXTURE_SIZE, TEXTURE_SIZE, 8, 2, 0, 0, 0)

    def chunk(name: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + name
            + payload
            + struct.pack(">I", zlib.crc32(name + payload) & 0xFFFFFFFF)
        )

    with open(path, "wb") as stream:
        stream.write(b"\x89PNG\r\n\x1a\n")
        stream.write(chunk(b"IHDR", header))
        stream.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        stream.write(chunk(b"IEND", b""))


def generate_pbr_textures() -> dict[str, str]:
    paths = {
        "body_base_color": os.path.join(TEXTURE_ROOT, "T_MahjongBody_BaseColor.png"),
        "body_normal": os.path.join(TEXTURE_ROOT, "T_MahjongBody_Normal.png"),
        "body_roughness": os.path.join(TEXTURE_ROOT, "T_MahjongBody_Roughness.png"),
        "back_base_color": os.path.join(TEXTURE_ROOT, "T_MahjongBack_BaseColor.png"),
    }

    def ivory(x: int, y: int) -> tuple[int, int, int]:
        grain = ((x * 17 + y * 31 + (x ^ y) * 7) % 13) - 6
        return (max(0, min(255, 239 + grain)), max(0, min(255, 232 + grain)), max(0, min(255, 203 + grain)))

    def green(x: int, y: int) -> tuple[int, int, int]:
        wave = int(5.0 * math.sin(x * 0.045) * math.sin(y * 0.052))
        return (24 + wave, 105 + wave, 61 + wave)

    write_png_rgb(paths["body_base_color"], ivory)
    write_png_rgb(paths["body_normal"], lambda _x, _y: (128, 128, 255))
    write_png_rgb(paths["body_roughness"], lambda x, y: (145 + ((x + y) % 11),) * 3)
    write_png_rgb(paths["back_base_color"], green)
    return paths


def flatten_face_texture(source: str, target: str) -> None:
    """把 UMG 用透明牌面合成到象牙底色，生成适合不透明 3D 材质的版本。"""
    image = bpy.data.images.load(source, check_existing=False)
    width, height = image.size
    source_pixels = list(image.pixels)
    output = bpy.data.images.new(f"Flattened_{PathName(target)}", width=width, height=height, alpha=True)
    pixels = [0.0] * len(source_pixels)
    ivory = (0.94, 0.91, 0.80)
    for index in range(0, len(source_pixels), 4):
        alpha = source_pixels[index + 3]
        pixels[index] = source_pixels[index] * alpha + ivory[0] * (1.0 - alpha)
        pixels[index + 1] = source_pixels[index + 1] * alpha + ivory[1] * (1.0 - alpha)
        pixels[index + 2] = source_pixels[index + 2] * alpha + ivory[2] * (1.0 - alpha)
        pixels[index + 3] = 1.0
    output.pixels = pixels
    output.filepath_raw = target
    output.file_format = "PNG"
    output.save()
    bpy.data.images.remove(output)
    bpy.data.images.remove(image)


def PathName(path: str) -> str:
    return os.path.splitext(os.path.basename(path))[0]


def copy_face_textures() -> list[str]:
    copied = []
    for suit in ("Wan", "Tiao", "Tong"):
        for rank in range(1, 10):
            name = f"T_Tile_{suit}_{rank:02d}.png"
            source = os.path.join(SOURCE_FACE_ROOT, name)
            target = os.path.join(FACE_ROOT, name)
            if not os.path.isfile(source):
                raise FileNotFoundError(f"缺少规则牌面纹理：{source}")
            flatten_face_texture(source, target)
            copied.append(target)
    for name in ("T_Tile_FrontBlank.png",):
        source = os.path.join(SOURCE_FACE_ROOT, name)
        target = os.path.join(FACE_ROOT, name)
        flatten_face_texture(source, target)
        copied.append(target)
    name = "T_Tile_Back.png"
    source = os.path.join(SOURCE_FACE_ROOT, name)
    target = os.path.join(FACE_ROOT, name)
    shutil.copy2(source, target)
    copied.append(target)
    return copied


def reset_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for datablocks in (bpy.data.meshes, bpy.data.curves, bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for block in list(datablocks):
            if block.users == 0:
                datablocks.remove(block)

    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.length_unit = "CENTIMETERS"
    scene.unit_settings.scale_length = 0.01
    # Blender 5.1 正式构建恢复为 BLENDER_EEVEE；兼容仍使用 NEXT 名称的预览构建。
    try:
        scene.render.engine = "BLENDER_EEVEE"
    except TypeError:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 960
    scene.render.resolution_y = 960
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = True


def image_node(nodes, image_path: str, colorspace: str = "sRGB"):
    node = nodes.new("ShaderNodeTexImage")
    node.image = bpy.data.images.load(image_path, check_existing=True)
    node.image.colorspace_settings.name = colorspace
    return node


def make_pbr_material(name: str, base_color_path: str, roughness_path: str | None = None,
                      normal_path: str | None = None, roughness: float = 0.48):
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.inputs["Roughness"].default_value = roughness
    shader.inputs["IOR"].default_value = 1.47
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])

    base = image_node(nodes, base_color_path)
    links.new(base.outputs["Color"], shader.inputs["Base Color"])
    if roughness_path:
        rough = image_node(nodes, roughness_path, "Non-Color")
        links.new(rough.outputs["Color"], shader.inputs["Roughness"])
    if normal_path:
        normal = image_node(nodes, normal_path, "Non-Color")
        normal_map = nodes.new("ShaderNodeNormalMap")
        normal_map.inputs["Strength"].default_value = 0.35
        links.new(normal.outputs["Color"], normal_map.inputs["Color"])
        links.new(normal_map.outputs["Normal"], shader.inputs["Normal"])
    return material


def make_face_material(name: str, texture_path: str):
    material = make_pbr_material(name, texture_path, roughness=0.42)
    material["ue_texture_parameter"] = "TileFaceTexture"
    return material


def rounded_box(name: str, dimensions: tuple[float, float, float], location: tuple[float, float, float],
                material, bevel: float):
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(material)
    modifier = obj.modifiers.new("GameReadyBevel", "BEVEL")
    modifier.width = bevel
    modifier.segments = 4
    modifier.limit_method = "ANGLE"
    modifier.angle_limit = math.radians(30.0)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    return obj


def project_face_uv(obj) -> None:
    """将牌面 X/Z 平面完整映射到 0–1，避免薄盒默认 UV 把图案压成条纹。"""
    mesh = obj.data
    uv_layer = mesh.uv_layers.active or mesh.uv_layers.new(name="TileFaceUV")
    xs = [vertex.co.x for vertex in mesh.vertices]
    zs = [vertex.co.z for vertex in mesh.vertices]
    min_x, max_x = min(xs), max(xs)
    min_z, max_z = min(zs), max(zs)
    width = max(max_x - min_x, 1.0e-6)
    height = max(max_z - min_z, 1.0e-6)
    for polygon in mesh.polygons:
        for loop_index in polygon.loop_indices:
            vertex = mesh.vertices[mesh.loops[loop_index].vertex_index]
            uv_layer.data[loop_index].uv = ((vertex.co.x - min_x) / width, (vertex.co.z - min_z) / height)


def face_decal(name: str, width: float, height: float, y: float, z_center: float, material):
    """创建朝 -Y 的单面牌面；避免极薄倒角盒在移动端产生法线暗斑。"""
    half_w = width * 0.5
    half_h = height * 0.5
    vertices = [
        (-half_w, y, z_center - half_h),
        (half_w, y, z_center - half_h),
        (half_w, y, z_center + half_h),
        (-half_w, y, z_center + half_h),
    ]
    mesh = bpy.data.meshes.new(f"{name}_Mesh")
    mesh.from_pydata(vertices, [], [(0, 1, 2, 3)])
    mesh.materials.append(material)
    # 与牌身默认 UVMap 同名，Join 后材质仍会读取正确的活动 UV 层。
    uv_layer = mesh.uv_layers.new(name="UVMap")
    for loop_index, uv in enumerate(((0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))):
        uv_layer.data[loop_index].uv = uv
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def join_model(parts: list, materials: list):
    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    model = bpy.context.object
    model.name = "SM_MahjongTile"
    model.data.name = "SM_MahjongTile_Mesh"
    model["dimensions_mm"] = [32.0, 22.0, 44.0]
    model["pivot"] = "bottom_center"
    model["material_slots"] = [material.name for material in materials]
    model["blender_compatibility"] = "5.1+"
    model.data.update()
    return model


def create_model(texture_paths: dict[str, str]):
    blank_face = os.path.join(FACE_ROOT, "T_Tile_FrontBlank.png")
    body = make_pbr_material(
        "M_MahjongTile_Body",
        texture_paths["body_base_color"],
        texture_paths["body_roughness"],
        texture_paths["body_normal"],
    )
    face = make_face_material("M_MahjongTile_Face", blank_face)
    back = make_pbr_material("M_MahjongTile_Back", texture_paths["back_base_color"], roughness=0.56)

    body_part = rounded_box(
        "TileBody",
        (TILE_WIDTH_CM, TILE_DEPTH_CM, TILE_HEIGHT_CM),
        (0.0, 0.0, TILE_HEIGHT_CM * 0.5),
        body,
        BEVEL_CM,
    )
    face_part = face_decal(
        "TileFaceInset",
        TILE_WIDTH_CM - 0.34,
        TILE_HEIGHT_CM - 0.40,
        -(TILE_DEPTH_CM * 0.5 + 0.035),
        TILE_HEIGHT_CM * 0.5,
        face,
    )
    back_part = rounded_box(
        "TileBackInset",
        (TILE_WIDTH_CM - 0.22, 0.08, TILE_HEIGHT_CM - 0.24),
        (0.0, TILE_DEPTH_CM * 0.5 + 0.025, TILE_HEIGHT_CM * 0.5),
        back,
        0.10,
    )
    project_face_uv(back_part)
    model = join_model([body_part, face_part, back_part], [body, face, back])
    return model, face


def add_preview_scene(model, original_face_material) -> None:
    preview_face = make_face_material("M_MahjongTile_Preview_Wan01", os.path.join(FACE_ROOT, "T_Tile_Wan_01.png"))
    replaced_slots = 0
    for slot in model.material_slots:
        if slot.material == original_face_material:
            slot.material = preview_face
            replaced_slots += 1
    print(f"PREVIEW_FACE_SLOTS_REPLACED={replaced_slots}")

    # 正面朝 -Y；轻微后仰并绕 Z 轴旋转，预览同时展示牌面、牌身与厚度。
    model.rotation_euler = (math.radians(-10.0), 0.0, math.radians(20.0))
    model.location = (0.0, 0.0, 0.4)

    bpy.ops.object.light_add(type="AREA", location=(-5.0, -7.0, 10.0))
    key = bpy.context.object
    key.name = "Preview_KeyLight"
    key.data.energy = 800.0
    key.data.shape = "DISK"
    key.data.size = 6.0

    bpy.ops.object.light_add(type="AREA", location=(6.0, 2.0, 7.0))
    fill = bpy.context.object
    fill.name = "Preview_FillLight"
    fill.data.energy = 420.0
    fill.data.size = 5.0

    bpy.ops.object.camera_add(location=(7.7, -10.5, 8.0))
    camera = bpy.context.object
    camera.name = "Preview_Camera"
    camera.data.lens = 58.0
    bpy.context.scene.camera = camera

    def point_at(obj, target=(0.0, 0.0, 2.2)):
        direction = mathutils.Vector(target) - obj.location
        obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()

    import mathutils

    point_at(camera)
    point_at(key)
    point_at(fill)
    world = bpy.context.scene.world or bpy.data.worlds.new("MahjongPreviewWorld")
    bpy.context.scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.008, 0.018, 0.014, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.32
    bpy.context.scene.render.filepath = os.path.join(OUTPUT_ROOT, "MahjongTile_Preview.png")
    bpy.ops.render.render(write_still=True)

    for slot in model.material_slots:
        if slot.material == preview_face:
            slot.material = original_face_material
    model.rotation_euler = (0.0, 0.0, 0.0)
    model.location = (0.0, 0.0, 0.0)
    bpy.data.objects.remove(camera, do_unlink=True)
    bpy.data.objects.remove(key, do_unlink=True)
    bpy.data.objects.remove(fill, do_unlink=True)


def export_assets(model) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    model.select_set(True)
    bpy.context.view_layer.objects.active = model

    blend_path = os.path.join(OUTPUT_ROOT, "SM_MahjongTile.blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend_path, check_existing=False)

    fbx_path = os.path.join(OUTPUT_ROOT, "SM_MahjongTile.fbx")
    bpy.ops.export_scene.fbx(
        filepath=fbx_path,
        use_selection=True,
        object_types={"MESH"},
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_UNITS",
        axis_forward="-Z",
        axis_up="Y",
        bake_space_transform=False,
        add_leaf_bones=False,
        path_mode="RELATIVE",
        embed_textures=False,
    )

    glb_path = os.path.join(OUTPUT_ROOT, "SM_MahjongTile.glb")
    bpy.ops.export_scene.gltf(
        filepath=glb_path,
        export_format="GLB",
        use_selection=True,
        export_apply=True,
        export_yup=True,
        export_materials="EXPORT",
    )

    usd_path = os.path.join(OUTPUT_ROOT, "SM_MahjongTile.usdc")
    bpy.ops.wm.usd_export(
        filepath=usd_path,
        selected_objects_only=True,
        export_materials=True,
        export_uvmaps=True,
        convert_orientation=True,
        export_global_forward_selection="NEGATIVE_Z",
        export_global_up_selection="Y",
    )


def write_manifest(model, copied_faces: list[str], texture_paths: dict[str, str]) -> None:
    assets = []
    manifest_path = os.path.join(OUTPUT_ROOT, "MahjongTileAssetManifest.json")
    generated_paths = []
    for directory, _subdirs, files in os.walk(OUTPUT_ROOT):
        for filename in files:
            path = os.path.join(directory, filename)
            if os.path.abspath(path) != os.path.abspath(manifest_path):
                generated_paths.append(path)
    for path in sorted(generated_paths):
        with open(path, "rb") as stream:
            payload = stream.read()
        assets.append(
            {
                "path": os.path.relpath(path, ROOT).replace("\\", "/"),
                "bytes": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest(),
            }
        )

    manifest = {
        "generator": "Blender 5.1+ GenerateMahjongTileAssets.py",
        "blender_version": bpy.app.version_string,
        "mesh": model.name,
        "dimensions_mm": [32, 22, 44],
        "bevel_mm": 1.8,
        "pivot": "bottom_center",
        "material_slots": [slot.material.name for slot in model.material_slots],
        "face_texture_count": 27,
        "assets": assets,
    }
    with open(manifest_path, "w", encoding="utf-8") as stream:
        json.dump(manifest, stream, ensure_ascii=False, indent=2)


def main() -> None:
    ensure_directories()
    texture_paths = generate_pbr_textures()
    copied_faces = copy_face_textures()
    reset_scene()
    model, face_material = create_model(texture_paths)
    add_preview_scene(model, face_material)
    export_assets(model)
    write_manifest(model, copied_faces, texture_paths)
    print(f"MAHJONG_TILE_ASSETS_GENERATED={OUTPUT_ROOT}")
    print(f"BLENDER_VERSION={bpy.app.version_string}")


if __name__ == "__main__":
    main()

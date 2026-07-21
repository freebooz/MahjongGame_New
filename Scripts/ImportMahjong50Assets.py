"""Import the F:/TT Mahjong50 PBR set and build all 34 playable tile assets."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


SOURCE_ROOT = Path("F:/TT")
MODEL_FILE = SOURCE_ROOT / "Model" / "SM_Mahjong50.fbx"
TEXTURE_SOURCE = SOURCE_ROOT / "Textures"
INDEX_FILE = TEXTURE_SOURCE / "Mahjong50_FaceAtlas_Index.json"

DEST_ROOT = "/Game/Art/Mahjong/Mahjong50"
MESH_DEST = f"{DEST_ROOT}/Meshes"
TEXTURE_DEST = f"{DEST_ROOT}/Textures"
MATERIAL_DEST = f"{DEST_ROOT}/Materials"
INSTANCE_DEST = f"{DEST_ROOT}/MaterialInstances"
TILE_DEST = f"{DEST_ROOT}/Tiles"

BASE_MESH_PATH = f"{MESH_DEST}/SM_Mahjong50"
BODY_MATERIAL_PATH = f"{MATERIAL_DEST}/M_Mahjong50_BodyBlend"
FACE_MATERIAL_PATH = f"{MATERIAL_DEST}/M_Mahjong50_FaceAtlas"


def log(message: str) -> None:
    unreal.log(f"[Mahjong50Import] {message}")


def warn(message: str) -> None:
    unreal.log_warning(f"[Mahjong50Import] {message}")


def set_prop(obj, name: str, value) -> bool:
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as exc:
        warn(f"skip property {obj.get_class().get_name()}.{name}: {exc}")
        return False


def ensure_sources() -> list[Path]:
    required = [MODEL_FILE, INDEX_FILE]
    texture_files = sorted(TEXTURE_SOURCE.glob("T_Mahjong50_*.png"))
    required.extend(texture_files)
    missing = [str(path) for path in required if not path.is_file()]
    if missing:
        raise RuntimeError("Missing source files: " + ", ".join(missing))
    if len(texture_files) != 12:
        raise RuntimeError(f"Expected 12 PNG textures, found {len(texture_files)}")
    return texture_files


def import_model():
    options = unreal.FbxImportUI()
    set_prop(options, "import_as_skeletal", False)
    set_prop(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    set_prop(options, "import_materials", False)
    set_prop(options, "import_textures", False)
    set_prop(options, "automated_import_should_detect_type", False)

    static_data = options.get_editor_property("static_mesh_import_data")
    set_prop(static_data, "import_uniform_scale", 1.0)
    set_prop(static_data, "combine_meshes", False)
    set_prop(static_data, "auto_generate_collision", False)
    set_prop(static_data, "generate_lightmap_u_vs", True)
    set_prop(static_data, "convert_scene", True)
    set_prop(static_data, "force_front_x_axis", False)
    set_prop(
        static_data,
        "normal_import_method",
        unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS,
    )
    if hasattr(unreal, "VertexColorImportOption"):
        set_prop(static_data, "vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)

    task = unreal.AssetImportTask()
    task.filename = str(MODEL_FILE)
    task.destination_path = MESH_DEST
    task.destination_name = "SM_Mahjong50"
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    mesh = unreal.EditorAssetLibrary.load_asset(BASE_MESH_PATH)
    if not mesh:
        candidates = unreal.EditorAssetLibrary.list_assets(MESH_DEST, recursive=False, include_folder=False)
        raise RuntimeError(f"FBX import did not create {BASE_MESH_PATH}; imported={candidates}")
    return mesh


def import_texture(source: Path):
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = TEXTURE_DEST
    task.destination_name = source.stem
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(f"{TEXTURE_DEST}/{source.stem}")
    if not texture:
        raise RuntimeError(f"Texture import failed: {source}")
    return texture


def configure_texture(texture, name: str) -> None:
    is_normal = name.endswith("_Normal")
    is_mask = name.endswith("_ORM") or name.endswith("_AO") or name.endswith("_Roughness") or name.endswith("_Height")
    set_prop(texture, "srgb", not (is_normal or is_mask))
    if is_normal:
        set_prop(texture, "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif is_mask:
        set_prop(texture, "compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    set_prop(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    post_edit_change = getattr(texture, "post_edit_change", None)
    if post_edit_change:
        post_edit_change()
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)


def load_texture(name: str):
    texture = unreal.EditorAssetLibrary.load_asset(f"{TEXTURE_DEST}/{name}")
    if not texture:
        raise RuntimeError(f"Missing imported texture: {name}")
    return texture


def get_or_create_material(name: str):
    path = f"{MATERIAL_DEST}/{name}"
    material = unreal.EditorAssetLibrary.load_asset(path)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, MATERIAL_DEST, unreal.Material, unreal.MaterialFactoryNew()
        )
    if not material:
        raise RuntimeError(f"Could not create material {path}")
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    set_prop(material, "two_sided", False)
    return material


def expr(material, cls, x: int, y: int):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def texture_sample(material, texture, parameter_name: str, x: int, y: int, sampler_type=None):
    node = expr(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    set_prop(node, "parameter_name", parameter_name)
    set_prop(node, "texture", texture)
    if sampler_type is not None:
        set_prop(node, "sampler_type", sampler_type)
    return node


def connect(a, output_name: str, b, input_name: str = "") -> None:
    if not unreal.MaterialEditingLibrary.connect_material_expressions(a, output_name, b, input_name):
        raise RuntimeError(
            f"Failed material connection {a.get_class().get_name()}.{output_name} -> "
            f"{b.get_class().get_name()}.{input_name}"
        )


def build_body_material():
    material = get_or_create_material("M_Mahjong50_BodyBlend")
    vertex_color = expr(material, unreal.MaterialExpressionVertexColor, -1050, 0)

    ivory_bc = texture_sample(material, load_texture("T_Mahjong50_Ivory_BaseColor"), "IvoryBaseColor", -1050, -430)
    green_bc = texture_sample(material, load_texture("T_Mahjong50_GreenWrap_BaseColor"), "GreenBaseColor", -1050, -310)
    bc_lerp = expr(material, unreal.MaterialExpressionLinearInterpolate, -650, -360)
    connect(ivory_bc, "", bc_lerp, "A")
    connect(green_bc, "", bc_lerp, "B")
    connect(vertex_color, "R", bc_lerp, "Alpha")
    unreal.MaterialEditingLibrary.connect_material_property(bc_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)

    ivory_n = texture_sample(
        material, load_texture("T_Mahjong50_Ivory_Normal"), "IvoryNormal", -1050, -160,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
    )
    green_n = texture_sample(
        material, load_texture("T_Mahjong50_GreenWrap_Normal"), "GreenNormal", -1050, -40,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
    )
    normal_lerp = expr(material, unreal.MaterialExpressionLinearInterpolate, -650, -100)
    connect(ivory_n, "", normal_lerp, "A")
    connect(green_n, "", normal_lerp, "B")
    connect(vertex_color, "R", normal_lerp, "Alpha")
    unreal.MaterialEditingLibrary.connect_material_property(normal_lerp, "", unreal.MaterialProperty.MP_NORMAL)

    ivory_orm = texture_sample(
        material, load_texture("T_Mahjong50_Ivory_ORM"), "IvoryORM", -1050, 220,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
    )
    green_orm = texture_sample(
        material, load_texture("T_Mahjong50_GreenWrap_ORM"), "GreenORM", -1050, 340,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
    )
    orm_lerp = expr(material, unreal.MaterialExpressionLinearInterpolate, -650, 280)
    connect(ivory_orm, "", orm_lerp, "A")
    connect(green_orm, "", orm_lerp, "B")
    connect(vertex_color, "R", orm_lerp, "Alpha")
    unreal.MaterialEditingLibrary.connect_material_property(orm_lerp, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    unreal.MaterialEditingLibrary.connect_material_property(orm_lerp, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(orm_lerp, "B", unreal.MaterialProperty.MP_METALLIC)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def scalar_parameter(material, name: str, default: float, x: int, y: int):
    node = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    set_prop(node, "parameter_name", name)
    set_prop(node, "default_value", default)
    return node


def constant2(material, x_value: float, y_value: float, x: int, y: int):
    node = expr(material, unreal.MaterialExpressionConstant2Vector, x, y)
    set_prop(node, "r", x_value)
    set_prop(node, "g", y_value)
    return node


def build_face_material():
    material = get_or_create_material("M_Mahjong50_FaceAtlas")

    texcoord = expr(material, unreal.MaterialExpressionTextureCoordinate, -1450, -200)
    uv_scale = constant2(material, 704.0 / 8192.0, 1024.0 / 4096.0, -1450, -80)
    uv_scaled = expr(material, unreal.MaterialExpressionMultiply, -1160, -150)
    connect(texcoord, "", uv_scaled, "A")
    connect(uv_scale, "", uv_scaled, "B")

    column = scalar_parameter(material, "Column", 3.0, -1450, 100)
    column_scale = expr(material, unreal.MaterialExpressionMultiply, -1160, 80)
    set_prop(column_scale, "const_b", 896.0 / 8192.0)
    connect(column, "", column_scale, "A")
    u_offset = expr(material, unreal.MaterialExpressionAdd, -900, 80)
    set_prop(u_offset, "const_b", 160.0 / 8192.0)
    connect(column_scale, "", u_offset, "A")

    row = scalar_parameter(material, "RowFromBottom", 0.0, -1450, 240)
    row_scale = expr(material, unreal.MaterialExpressionMultiply, -1160, 230)
    set_prop(row_scale, "const_b", 1024.0 / 4096.0)
    connect(row, "", row_scale, "A")

    uv_offset = expr(material, unreal.MaterialExpressionAppendVector, -650, 140)
    connect(u_offset, "", uv_offset, "A")
    connect(row_scale, "", uv_offset, "B")
    atlas_uv = expr(material, unreal.MaterialExpressionAdd, -400, -80)
    connect(uv_scaled, "", atlas_uv, "A")
    connect(uv_offset, "", atlas_uv, "B")

    base_color = texture_sample(
        material, load_texture("T_Mahjong50_FaceAtlas_BaseColor"), "FaceBaseColor", -100, -330
    )
    normal = texture_sample(
        material, load_texture("T_Mahjong50_FaceAtlas_Normal"), "FaceNormal", -100, -100,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
    )
    orm = texture_sample(
        material, load_texture("T_Mahjong50_FaceAtlas_ORM"), "FaceORM", -100, 130,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
    )
    for sample in (base_color, normal, orm):
        connect(atlas_uv, "", sample, "UVs")

    unreal.MaterialEditingLibrary.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    unreal.MaterialEditingLibrary.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    unreal.MaterialEditingLibrary.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def create_face_instance(tile: dict, face_material):
    name = f"MI_Mahjong50_{tile['name']}"
    path = f"{INSTANCE_DEST}/{name}"
    instance = unreal.EditorAssetLibrary.load_asset(path)
    if not instance:
        instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, INSTANCE_DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew()
        )
    if not instance:
        raise RuntimeError(f"Could not create {path}")
    unreal.MaterialEditingLibrary.set_material_instance_parent(instance, face_material)
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(instance, "Column", float(tile["column"]))
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        instance, "RowFromBottom", float(tile["row_from_bottom"])
    )
    unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False)
    return instance


def material_slot_names(mesh) -> list[str]:
    names = []
    for slot in mesh.get_editor_property("static_materials"):
        imported = slot.get_editor_property("imported_material_slot_name")
        current = slot.get_editor_property("material_slot_name")
        names.append(str(imported or current))
    return names


def assign_tile_materials(mesh, body_material, face_instance) -> None:
    slots = material_slot_names(mesh)
    if len(slots) < 2:
        raise RuntimeError(f"Expected body and face material slots, found {slots}")
    face_slot = next((i for i, name in enumerate(slots) if "face" in name.lower()), 1)
    for index in range(len(slots)):
        mesh.set_material(index, face_instance if index == face_slot else body_material)
    post_edit_change = getattr(mesh, "post_edit_change", None)
    if post_edit_change:
        post_edit_change()
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)


def create_tile_mesh(tile: dict, base_mesh, body_material, face_instance):
    name = f"SM_Mahjong50_{tile['name']}"
    path = f"{TILE_DEST}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        tile_mesh = unreal.EditorAssetLibrary.load_asset(path)
    else:
        tile_mesh = unreal.EditorAssetLibrary.duplicate_asset(BASE_MESH_PATH, path)
    if not tile_mesh:
        raise RuntimeError(f"Could not create tile mesh {path}")
    assign_tile_materials(tile_mesh, body_material, face_instance)
    return tile_mesh


def validate(base_mesh, tile_specs: list[dict], textures: list) -> None:
    active_tiles = [tile for tile in tile_specs if not tile.get("reserved", False)]
    if len(active_tiles) != 34:
        raise RuntimeError(f"Expected 34 active tile definitions, found {len(active_tiles)}")
    if len(textures) != 12:
        raise RuntimeError(f"Expected 12 textures, found {len(textures)}")
    slots = material_slot_names(base_mesh)
    if len(slots) != 2:
        raise RuntimeError(f"Expected exactly 2 material slots on base mesh, found {slots}")
    bounds = base_mesh.get_bounds()
    size = bounds.box_extent * 2.0
    expected = sorted([3.6, 2.6, 5.0])
    actual = sorted([size.x, size.y, size.z])
    if any(abs(a - e) > 0.35 for a, e in zip(actual, expected)):
        raise RuntimeError(f"Unexpected mesh dimensions cm: ({size.x:.3f}, {size.y:.3f}, {size.z:.3f})")
    for tile in active_tiles:
        mesh_path = f"{TILE_DEST}/SM_Mahjong50_{tile['name']}"
        mi_path = f"{INSTANCE_DEST}/MI_Mahjong50_{tile['name']}"
        if not unreal.EditorAssetLibrary.does_asset_exist(mesh_path):
            raise RuntimeError(f"Missing tile mesh {mesh_path}")
        if not unreal.EditorAssetLibrary.does_asset_exist(mi_path):
            raise RuntimeError(f"Missing face material instance {mi_path}")
    log(
        f"validated mesh dimensions=({size.x:.3f}, {size.y:.3f}, {size.z:.3f}) cm, "
        f"slots={slots}, textures=12, tile variants=34"
    )


def main() -> None:
    log(f"source={SOURCE_ROOT} destination={DEST_ROOT}")
    texture_files = ensure_sources()
    index_data = json.loads(INDEX_FILE.read_text(encoding="utf-8-sig"))
    tile_specs = [tile for tile in index_data["tiles"] if not tile.get("reserved", False)]

    base_mesh = import_model()
    imported_textures = []
    for source in texture_files:
        texture = import_texture(source)
        configure_texture(texture, source.stem)
        imported_textures.append(texture)

    body_material = build_body_material()
    face_material = build_face_material()
    assign_tile_materials(base_mesh, body_material, create_face_instance(
        next(tile for tile in tile_specs if tile["name"] == "Red_Dragon"), face_material
    ))

    for tile in tile_specs:
        face_instance = create_face_instance(tile, face_material)
        create_tile_mesh(tile, base_mesh, body_material, face_instance)

    validate(base_mesh, tile_specs, imported_textures)
    unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False, recursive=True)
    log("MAHJONG50_IMPORT_OK")


if __name__ == "__main__":
    main()

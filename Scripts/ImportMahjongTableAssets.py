"""Import the standard 94 cm Mahjong table, build PBR materials, and place it in MahjongRoomMap.

Run from UnrealEditor-Cmd with ``-ExecutePythonScript=...``. The script is idempotent:
existing assets and the tagged level actor are updated in place.
"""

from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "3D" / "MahjongTable"
MODEL_FILE = SOURCE_ROOT / "SM_StandardMahjongTable.fbx"
TEXTURE_SOURCE = SOURCE_ROOT / "Textures"
TEXTURE_MANIFEST = TEXTURE_SOURCE / "MahjongTableTextureManifest.json"

DEST_ROOT = "/Game/Art/Mahjong/Table"
MESH_DEST = f"{DEST_ROOT}/Meshes"
TEXTURE_DEST = f"{DEST_ROOT}/Textures"
MATERIAL_DEST = f"{DEST_ROOT}/Materials"
MESH_PATH = f"{MESH_DEST}/SM_StandardMahjongTable"
MAP_PATH = "/Game/Maps/MahjongRoomMap"
LEVEL_ACTOR_LABEL = "MahjongRoomTable"
LEVEL_ACTOR_TAG = "MahjongRoomTable"
REPORT_PATH = PROJECT_ROOT / "Saved" / "Reports" / "MahjongTableImportReport.json"


def log(message: str) -> None:
    unreal.log(f"[MahjongTableImport] {message}")


def warn(message: str) -> None:
    unreal.log_warning(f"[MahjongTableImport] {message}")


def set_prop(obj, name: str, value) -> bool:
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as exc:
        warn(f"skip property {obj.get_class().get_name()}.{name}: {exc}")
        return False


def ensure_sources() -> dict:
    if not MODEL_FILE.is_file():
        raise RuntimeError(f"Missing FBX source: {MODEL_FILE}")
    if not TEXTURE_MANIFEST.is_file():
        raise RuntimeError(f"Missing PBR texture manifest: {TEXTURE_MANIFEST}")
    manifest = json.loads(TEXTURE_MANIFEST.read_text(encoding="utf-8-sig"))
    texture_files = sorted(TEXTURE_SOURCE.glob("T_Table_*.png"))
    if len(manifest.get("materials", [])) != 11:
        raise RuntimeError("Expected 11 material definitions in texture manifest")
    if len(texture_files) != 33:
        raise RuntimeError(f"Expected 33 PBR textures, found {len(texture_files)}")
    for source in texture_files:
        if not source.is_file():
            raise RuntimeError(f"Missing texture source: {source}")
    return manifest


def ensure_destination_folders() -> None:
    for path in (DEST_ROOT, MESH_DEST, TEXTURE_DEST, MATERIAL_DEST):
        unreal.EditorAssetLibrary.make_directory(path)


def load_complete_asset_set(material_specs: list[dict]):
    """Return the existing imported set when it is complete, avoiding destructive re-imports."""
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    if not mesh:
        return None
    textures = []
    for source in sorted(TEXTURE_SOURCE.glob("T_Table_*.png")):
        texture = unreal.EditorAssetLibrary.load_asset(f"{TEXTURE_DEST}/{source.stem}")
        if not texture:
            return None
        textures.append(texture)
    materials = {}
    for spec in material_specs:
        material = unreal.EditorAssetLibrary.load_asset(f"{MATERIAL_DEST}/{spec['material_slot']}")
        if not material:
            return None
        materials[spec["material_slot"]] = material
    if len(textures) != 33 or len(materials) != 11:
        return None
    return mesh, textures, materials


def import_model():
    options = unreal.FbxImportUI()
    set_prop(options, "import_as_skeletal", False)
    set_prop(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    set_prop(options, "import_materials", False)
    set_prop(options, "import_textures", False)
    set_prop(options, "automated_import_should_detect_type", False)

    static_data = options.get_editor_property("static_mesh_import_data")
    set_prop(static_data, "import_uniform_scale", 1.0)
    set_prop(static_data, "combine_meshes", True)
    set_prop(static_data, "auto_generate_collision", False)
    set_prop(static_data, "generate_lightmap_u_vs", True)
    set_prop(static_data, "convert_scene", True)
    set_prop(static_data, "force_front_x_axis", False)
    set_prop(
        static_data,
        "normal_import_method",
        unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS,
    )

    task = unreal.AssetImportTask()
    task.filename = str(MODEL_FILE)
    task.destination_path = MESH_DEST
    task.destination_name = "SM_StandardMahjongTable"
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    if not mesh:
        imported = unreal.EditorAssetLibrary.list_assets(MESH_DEST, recursive=False, include_folder=False)
        raise RuntimeError(f"FBX import did not create {MESH_PATH}; imported={imported}")
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
    is_mask = name.endswith("_ORM")
    set_prop(texture, "srgb", not (is_normal or is_mask))
    if is_normal:
        set_prop(texture, "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        set_prop(texture, "flip_green_channel", False)
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


def expression(material, expression_class, x: int, y: int):
    return unreal.MaterialEditingLibrary.create_material_expression(material, expression_class, x, y)


def texture_parameter(material, texture, parameter_name: str, x: int, y: int, sampler_type):
    node = expression(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    set_prop(node, "parameter_name", parameter_name)
    set_prop(node, "texture", texture)
    set_prop(node, "sampler_type", sampler_type)
    return node


def build_material(material_spec: dict):
    slot_name = material_spec["material_slot"]
    textures = material_spec["textures"]
    material = get_or_create_material(slot_name)

    base_color = texture_parameter(
        material,
        load_texture(Path(textures["BaseColor"]).stem),
        "BaseColor",
        -500,
        -220,
        unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
    )
    normal = texture_parameter(
        material,
        load_texture(Path(textures["Normal"]).stem),
        "Normal",
        -500,
        20,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
    )
    orm = texture_parameter(
        material,
        load_texture(Path(textures["ORM"]).stem),
        "ORM",
        -500,
        250,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
    )
    library = unreal.MaterialEditingLibrary
    library.connect_material_property(base_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    library.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    library.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    library.connect_material_property(orm, "G", unreal.MaterialProperty.MP_ROUGHNESS)
    library.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    library.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    return material


def material_slot_names(mesh) -> list[str]:
    names = []
    for slot in mesh.get_editor_property("static_materials"):
        imported = slot.get_editor_property("imported_material_slot_name")
        current = slot.get_editor_property("material_slot_name")
        names.append(str(imported or current))
    return names


def resolve_slot_material(slot_name: str, materials: dict[str, object]):
    lowered = slot_name.lower()
    exact = materials.get(slot_name)
    if exact:
        return exact
    for material_name, material in materials.items():
        material_lower = material_name.lower()
        if lowered.startswith(material_lower) or material_lower in lowered:
            return material
    return None


def assign_materials(mesh, materials: dict[str, object]) -> list[str]:
    slots = material_slot_names(mesh)
    missing = []
    for index, slot_name in enumerate(slots):
        material = resolve_slot_material(slot_name, materials)
        if material:
            mesh.set_material(index, material)
        else:
            missing.append(slot_name)
    if missing:
        raise RuntimeError(f"No generated material for imported FBX slots: {missing}")
    post_edit_change = getattr(mesh, "post_edit_change", None)
    if post_edit_change:
        post_edit_change()
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    return slots


def actor_tags(actor) -> list[str]:
    try:
        return [str(tag) for tag in actor.get_editor_property("tags")]
    except Exception:
        return []


def place_in_room_level(mesh):
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        unreal.EditorLevelLibrary.new_level(MAP_PATH)
    else:
        unreal.EditorLevelLibrary.load_level(MAP_PATH)

    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        label = actor.get_actor_label() if hasattr(actor, "get_actor_label") else ""
        if label == LEVEL_ACTOR_LABEL or LEVEL_ACTOR_TAG in actor_tags(actor):
            unreal.EditorLevelLibrary.destroy_actor(actor)

    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not actor:
        raise RuntimeError("Could not spawn Mahjong room table actor")
    actor.set_actor_label(LEVEL_ACTOR_LABEL)
    set_prop(actor, "tags", [unreal.Name(LEVEL_ACTOR_TAG)])
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component:
        raise RuntimeError("Spawned StaticMeshActor has no StaticMeshComponent")
    component.set_static_mesh(mesh)
    set_prop(component, "mobility", unreal.ComponentMobility.STATIC)
    set_prop(component, "cast_shadow", True)
    set_prop(component, "collision_enabled", unreal.CollisionEnabled.NO_COLLISION)
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError(f"Could not save level {MAP_PATH}")
    return actor


def validate(mesh, slots: list[str], actor, imported_textures: list) -> tuple[float, float, float]:
    if len(imported_textures) != 33:
        raise RuntimeError(f"Expected 33 imported textures, found {len(imported_textures)}")
    if len(slots) != 11:
        raise RuntimeError(f"Expected 11 material slots, found {len(slots)}: {slots}")
    bounds = mesh.get_bounds()
    size = bounds.box_extent * 2.0
    actual = (float(size.x), float(size.y), float(size.z))
    expected = sorted((94.0, 94.0, 78.0))
    if any(abs(a - e) > 1.0 for a, e in zip(sorted(actual), expected)):
        raise RuntimeError(f"Unexpected imported mesh dimensions cm: {actual}")
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if not component or component.get_editor_property("static_mesh") != mesh:
        raise RuntimeError("Level actor does not reference the imported Mahjong table mesh")
    return actual


def write_report(dimensions: tuple[float, float, float], slots: list[str], material_specs: list[dict]) -> None:
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    report = {
        "status": "ok",
        "source_fbx": str(MODEL_FILE),
        "unreal_mesh": MESH_PATH,
        "dimensions_cm": list(dimensions),
        "material_slots": slots,
        "materials": [f"{MATERIAL_DEST}/{spec['material_slot']}" for spec in material_specs],
        "texture_count": 33,
        "level": MAP_PATH,
        "level_actor": LEVEL_ACTOR_LABEL,
        "runtime_actor": "/Script/GuiyangMahjong.Mahjong3DTableActor",
    }
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def main() -> None:
    log(f"source={SOURCE_ROOT} destination={DEST_ROOT}")
    manifest = ensure_sources()
    ensure_destination_folders()
    existing = load_complete_asset_set(manifest["materials"])
    if existing:
        mesh, imported_textures, materials = existing
        log("complete imported asset set found; skipping FBX/texture/material rebuild")
    else:
        mesh = import_model()
        imported_textures = []
        for source in sorted(TEXTURE_SOURCE.glob("T_Table_*.png")):
            texture = import_texture(source)
            configure_texture(texture, source.stem)
            imported_textures.append(texture)

        materials = {}
        for material_spec in manifest["materials"]:
            material = build_material(material_spec)
            materials[material_spec["material_slot"]] = material
    slots = assign_materials(mesh, materials)
    actor = place_in_room_level(mesh)
    dimensions = validate(mesh, slots, actor, imported_textures)

    unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False, recursive=True)
    write_report(dimensions, slots, manifest["materials"])
    log(
        "MAHJONG_TABLE_IMPORT_OK "
        f"dimensions_cm=({dimensions[0]:.3f}, {dimensions[1]:.3f}, {dimensions[2]:.3f}) "
        f"slots={len(slots)} textures={len(imported_textures)} map={MAP_PATH}"
    )


if __name__ == "__main__":
    main()

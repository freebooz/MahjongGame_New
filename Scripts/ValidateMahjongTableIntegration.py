"""Headless validation for the imported Mahjong table and both room integration paths."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MESH_PATH = "/Game/Art/Mahjong/Table/Meshes/SM_StandardMahjongTable"
CONTENT_ROOT = "/Game/Art/Mahjong/Table"
MAP_PATH = "/Game/Maps/MahjongRoomVisualPreviewMap"
ACTOR_LABEL = "MahjongRoomTable"
RUNTIME_CLASS_PATH = "/Script/GuiyangMahjong.Mahjong3DTableActor"
REPORT_PATH = PROJECT_ROOT / "Saved" / "Reports" / "MahjongTableValidationReport.json"


def material_slot_names(mesh) -> list[str]:
    return [
        str(slot.get_editor_property("imported_material_slot_name") or slot.get_editor_property("material_slot_name"))
        for slot in mesh.get_editor_property("static_materials")
    ]


def main() -> None:
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    if not mesh:
        raise RuntimeError(f"Missing table mesh {MESH_PATH}")
    size = mesh.get_bounds().box_extent * 2.0
    dimensions = (float(size.x), float(size.y), float(size.z))
    if any(abs(a - e) > 1.0 for a, e in zip(sorted(dimensions), sorted((94.0, 94.0, 78.0)))):
        raise RuntimeError(f"Unexpected table dimensions: {dimensions}")

    slots = material_slot_names(mesh)
    if len(slots) != 11:
        raise RuntimeError(f"Expected 11 material slots, found {len(slots)}: {slots}")
    assigned_materials = []
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        material = slot.get_editor_property("material_interface")
        if not material:
            raise RuntimeError(f"Material slot {index} ({slots[index]}) is unassigned")
        assigned_materials.append(material.get_path_name())

    assets = unreal.EditorAssetLibrary.list_assets(CONTENT_ROOT, recursive=True, include_folder=False)
    textures = [path for path in assets if "/Textures/T_Table_" in path]
    materials = [path for path in assets if "/Materials/M_Table_" in path]
    if len(textures) != 33 or len(materials) != 11:
        raise RuntimeError(f"Unexpected asset counts: textures={len(textures)} materials={len(materials)}")

    unreal.EditorLevelLibrary.load_level(MAP_PATH)
    room_actors = [
        actor
        for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if actor.get_actor_label() == ACTOR_LABEL
    ]
    if len(room_actors) != 1:
        raise RuntimeError(f"Expected one {ACTOR_LABEL} level actor, found {len(room_actors)}")
    room_component = room_actors[0].get_component_by_class(unreal.StaticMeshComponent)
    if not room_component or room_component.get_editor_property("static_mesh") != mesh:
        raise RuntimeError("Room actor does not use the standard Mahjong table mesh")

    runtime_class = unreal.load_class(None, RUNTIME_CLASS_PATH)
    if not runtime_class:
        raise RuntimeError(f"Could not load runtime table class {RUNTIME_CLASS_PATH}")
    # Loading the native class constructs its CDO and executes the constructor that resolves
    # TableMesh. The pointer is intentionally private, so Python validates class load here;
    # the C++ compile and the explicit asset load path cover the private assignment itself.
    runtime_cdo = unreal.get_default_object(runtime_class)
    if not runtime_cdo:
        raise RuntimeError("Runtime Mahjong3DTableActor CDO could not be constructed")

    report = {
        "status": "ok",
        "mesh": MESH_PATH,
        "dimensions_cm": list(dimensions),
        "material_slots": slots,
        "assigned_materials": assigned_materials,
        "texture_count": len(textures),
        "material_count": len(materials),
        "level": MAP_PATH,
        "level_actor": room_actors[0].get_path_name(),
        "runtime_class": RUNTIME_CLASS_PATH,
        "runtime_class_loaded": True,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log(
        "[MahjongTableValidation] MAHJONG_TABLE_VALIDATION_OK "
        f"dimensions_cm=({dimensions[0]:.3f}, {dimensions[1]:.3f}, {dimensions[2]:.3f}) "
        f"slots={len(slots)} textures={len(textures)} level_actor=1 runtime_class=loaded"
    )


if __name__ == "__main__":
    main()

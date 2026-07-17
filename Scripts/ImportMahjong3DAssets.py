"""将 Blender 生成的麻将牌 FBX/PBR 资产幂等导入 UE 5.8。"""

from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir())
SOURCE_ROOT = ROOT / "SourceArt" / "3D" / "MahjongTiles"
DEST_ROOT = "/Game/Art/Mahjong/Tiles"
TEXTURE_DEST = f"{DEST_ROOT}/Textures"


def log(message: str) -> None:
    unreal.log(f"[Mahjong3DImport] {message}")


def set_prop(obj, name: str, value) -> bool:
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as exc:
        unreal.log_warning(f"[Mahjong3DImport] skip {name}: {exc}")
        return False


def import_model() -> object:
    source = SOURCE_ROOT / "SM_MahjongTile.fbx"
    if not source.is_file():
        raise RuntimeError(f"模型文件不存在：{source}")

    options = unreal.FbxImportUI()
    set_prop(options, "import_as_skeletal", False)
    set_prop(options, "mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    set_prop(options, "import_materials", True)
    set_prop(options, "import_textures", True)
    static_data = options.get_editor_property("static_mesh_import_data")
    set_prop(static_data, "combine_meshes", True)
    set_prop(static_data, "auto_generate_collision", False)
    set_prop(static_data, "generate_lightmap_u_vs", True)
    set_prop(static_data, "convert_scene", True)
    set_prop(static_data, "force_front_x_axis", False)

    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = DEST_ROOT
    task.destination_name = "SM_MahjongTile"
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.EditorAssetLibrary.load_asset(f"{DEST_ROOT}/SM_MahjongTile")
    if not mesh:
        raise RuntimeError("FBX 导入完成但未找到 SM_MahjongTile")
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
        raise RuntimeError(f"纹理导入失败：{source}")
    return texture


def configure_texture(texture, name: str) -> None:
    is_normal = name.endswith("_Normal")
    is_roughness = name.endswith("_Roughness")
    set_prop(texture, "srgb", not (is_normal or is_roughness))
    if is_normal:
        set_prop(texture, "compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif is_roughness:
        set_prop(texture, "compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    set_prop(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    callback = getattr(texture, "post_edit_change", None)
    if callback:
        callback()
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)


def verify(mesh, textures: list) -> None:
    bounds = mesh.get_bounds()
    extent = bounds.box_extent
    if min(extent.x, extent.y, extent.z) <= 0.0:
        raise RuntimeError(f"模型包围盒无效：{extent}")
    if len(textures) != 4:
        raise RuntimeError(f"PBR 纹理数量错误：{len(textures)}")
    log(
        "verified mesh=%s extent=(%.3f, %.3f, %.3f) textures=%d"
        % (mesh.get_path_name(), extent.x, extent.y, extent.z, len(textures))
    )


def main() -> None:
    mesh = import_model()
    texture_files = sorted((SOURCE_ROOT / "Textures").glob("T_Mahjong*.png"))
    textures = []
    for source in texture_files:
        texture = import_texture(source)
        configure_texture(texture, source.stem)
        textures.append(texture)
    verify(mesh, textures)
    unreal.EditorAssetLibrary.save_directory(DEST_ROOT, only_if_is_dirty=False, recursive=True)
    log("MAHJONG_3D_IMPORT_OK")


if __name__ == "__main__":
    main()

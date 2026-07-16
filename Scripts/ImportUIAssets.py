"""Idempotent UE 5.8 Editor Python importer for generated UI source art."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir())
SOURCE = ROOT / "SourceArt" / "UI"
REPORT = SOURCE / "Data" / "ui_asset_inventory.json"
DEST_ROOT = "/Game/UI/Textures"


def log(message: str) -> None:
    unreal.log(f"[GuiyangUIImport] {message}")


def set_prop(obj, name: str, value) -> bool:
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception as exc:
        unreal.log_warning(f"[GuiyangUIImport] property {name} skipped on {obj.get_name()}: {exc}")
        return False


def post_edit(obj) -> None:
    callback = getattr(obj, "post_edit_change", None)
    if callback:
        callback()


def import_texture(source: Path, destination: str):
    asset_path = f"{destination}/{source.stem}"
    existing = unreal.EditorAssetLibrary.load_asset(asset_path)
    if existing:
        return existing
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = destination
    task.destination_name = source.stem
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.EditorAssetLibrary.load_asset(asset_path)


def configure_texture(texture, large_background: bool) -> None:
    # UE 5.8 exposes the legacy UserInterface2D preset as TC_EDITOR_ICON (uncompressed RGBA8).
    set_prop(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    set_prop(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    set_prop(texture, "srgb", True)
    set_prop(texture, "never_stream", not large_background)
    if not large_background:
        set_prop(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    post_edit(texture)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)


def load_texture(folder: str, name: str):
    return unreal.EditorAssetLibrary.load_asset(f"{DEST_ROOT}/{folder}/{name}")


def brush(texture, margin: float = 0.0):
    result = unreal.SlateBrush()
    set_prop(result, "resource_object", texture)
    set_prop(result, "draw_as", unreal.SlateBrushDrawType.BOX if margin > 0 else unreal.SlateBrushDrawType.IMAGE)
    if margin > 0:
        set_prop(result, "margin", unreal.Margin(left=margin, top=margin, right=margin, bottom=margin))
    return result


def create_data_asset(name: str, data_class):
    path = f"/Game/UI/Data/{name}"
    existing = unreal.EditorAssetLibrary.load_asset(path)
    if existing:
        return existing
    factory = unreal.DataAssetFactory()
    set_prop(factory, "data_asset_class", data_class)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, "/Game/UI/Data", data_class, factory)


def populate_data_assets(inventory: dict) -> None:
    if not hasattr(unreal, "GuiyangUIThemeDataAsset"):
        unreal.log_warning("[GuiyangUIImport] Style DataAsset classes are not in the loaded Editor binary; textures/materials will still be imported")
        return
    theme = create_data_asset("DA_UITheme", unreal.GuiyangUIThemeDataAsset)
    colors = {unreal.Name(k): unreal.LinearColor(*[int(v[i:i+2], 16) / 255.0 for i in (1, 3, 5)], 1.0) for k, v in inventory["design_tokens"].items()}
    set_prop(theme, "colors", colors)
    set_prop(theme, "corner_radii", {unreal.Name("SmallRadius"): 8.0, unreal.Name("MediumRadius"): 16.0, unreal.Name("LargeRadius"): 24.0, unreal.Name("DialogRadius"): 32.0})
    set_prop(theme, "border_widths", {unreal.Name("ThinBorder"): 2.0, unreal.Name("NormalBorder"): 4.0, unreal.Name("FocusBorder"): 6.0})

    panels = create_data_asset("DA_PanelStyles", unreal.GuiyangUIPanelStylesDataAsset)
    panel_brushes = {}
    for spec in inventory["panels"]:
        tex = load_texture("Panels", spec["name"])
        if tex:
            panel_brushes[unreal.Name(spec["name"].replace("T_Panel_", ""))] = brush(tex, spec["margin_normalized"])
    set_prop(panels, "brushes", panel_brushes)

    buttons = create_data_asset("DA_ButtonStyles", unreal.GuiyangUIButtonStylesDataAsset)
    styles = {}
    kinds = sorted({item["name"].removeprefix("T_Btn_").rsplit("_", 1)[0] for item in inventory["buttons"]})
    for kind in kinds:
        normal = load_texture("Buttons", f"T_Btn_{kind}_Normal")
        hovered = load_texture("Buttons", f"T_Btn_{kind}_Hovered")
        pressed = load_texture("Buttons", f"T_Btn_{kind}_Pressed")
        disabled = load_texture("Buttons", f"T_Btn_{kind}_Disabled")
        if all((normal, hovered, pressed, disabled)):
            style = unreal.ButtonStyle()
            set_prop(style, "normal", brush(normal, 0.1458))
            set_prop(style, "hovered", brush(hovered, 0.1458))
            set_prop(style, "pressed", brush(pressed, 0.1458))
            set_prop(style, "disabled", brush(disabled, 0.1458))
            styles[unreal.Name(kind)] = style
    set_prop(buttons, "styles", styles)

    fonts = create_data_asset("DA_FontStyles", unreal.GuiyangUIFontStylesDataAsset)
    set_prop(fonts, "styles", {})

    icons = create_data_asset("DA_IconRegistry", unreal.GuiyangUIIconRegistryDataAsset)
    icon_map = {}
    for name in inventory["icons"]:
        tex = load_texture("Icons", name)
        if tex:
            icon_map[unreal.Name(name.removeprefix("Icon_"))] = tex
    set_prop(icons, "icons", icon_map)

    tiles = create_data_asset("DA_TileTextureRegistry", unreal.GuiyangUITileTextureRegistryDataAsset)
    tile_map = {}
    for index in range(27):
        suit = ("Wan", "Tong", "Tiao")[index // 9]
        tex = load_texture("Tiles", f"T_Tile_{suit}_{index % 9 + 1:02d}")
        if tex:
            tile_map[index] = tex
    set_prop(tiles, "rule_index_to_texture", tile_map)
    set_prop(tiles, "back_texture", load_texture("Tiles", "T_Tile_Back"))
    set_prop(tiles, "front_blank_texture", load_texture("Tiles", "T_Tile_FrontBlank"))
    set_prop(tiles, "selected_glow_texture", load_texture("Tiles", "T_Tile_SelectedGlow"))
    set_prop(tiles, "disabled_mask_texture", load_texture("Tiles", "T_Tile_DisabledMask"))

    for asset in (theme, panels, buttons, fonts, icons, tiles):
        post_edit(asset)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def create_materials() -> None:
    material_names = [
        "M_UI_GradientPanel", "M_UI_SoftGlow", "M_UI_Outline", "M_UI_Desaturate", "M_UI_Disabled",
        "M_UI_ProgressFill", "M_UI_NetworkPulse", "M_UI_TileSelected", "M_UI_BackgroundBlurMask",
    ]
    material_factory = unreal.MaterialFactoryNew()
    for name in material_names:
        path = f"/Game/UI/Materials/{name}"
        material = unreal.EditorAssetLibrary.load_asset(path)
        if not material:
            material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, "/Game/UI/Materials", unreal.Material, material_factory)
        set_prop(material, "material_domain", unreal.MaterialDomain.MD_UI)
        set_prop(material, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        set_prop(material, "two_sided", True)
        unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
        tint = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -420, -60)
        set_prop(tint, "parameter_name", "TintColor")
        set_prop(tint, "default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        opacity = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -420, 100)
        set_prop(opacity, "parameter_name", "Opacity")
        set_prop(opacity, "default_value", 1.0)
        unreal.MaterialEditingLibrary.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        unreal.MaterialEditingLibrary.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)

    instance_specs = {
        "MI_UI_GoldGlow": ("M_UI_SoftGlow", unreal.LinearColor(0.851, 0.643, 0.255, 1.0)),
        "MI_UI_GreenGlow": ("M_UI_SoftGlow", unreal.LinearColor(0.259, 0.647, 0.549, 1.0)),
        "MI_UI_RedWarning": ("M_UI_NetworkPulse", unreal.LinearColor(0.722, 0.275, 0.227, 1.0)),
        "MI_UI_Disabled": ("M_UI_Disabled", unreal.LinearColor(0.416, 0.439, 0.427, 1.0)),
        "MI_UI_TileSelected": ("M_UI_TileSelected", unreal.LinearColor(0.851, 0.643, 0.255, 1.0)),
    }
    factory = unreal.MaterialInstanceConstantFactoryNew()
    for name, (parent_name, color) in instance_specs.items():
        path = f"/Game/UI/Materials/{name}"
        instance = unreal.EditorAssetLibrary.load_asset(path)
        if not instance:
            parent = unreal.EditorAssetLibrary.load_asset(f"/Game/UI/Materials/{parent_name}")
            instance = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, "/Game/UI/Materials", unreal.MaterialInstanceConstant, factory)
        parent = unreal.EditorAssetLibrary.load_asset(f"/Game/UI/Materials/{parent_name}")
        unreal.MaterialEditingLibrary.set_material_instance_parent(instance, parent)
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(instance, "TintColor", color)
        unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False)


def verify(inventory: dict) -> None:
    errors = []
    for folder in ("Backgrounds", "Panels", "Buttons", "Controls", "Avatars", "Icons", "Tiles"):
        for source in sorted((SOURCE / folder).glob("*.png")):
            asset = unreal.EditorAssetLibrary.load_asset(f"{DEST_ROOT}/{folder}/{source.stem}")
            if not asset:
                errors.append(f"missing {folder}/{source.stem}")
                continue
            if asset.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_EDITOR_ICON:
                errors.append(f"compression {folder}/{source.stem}")
    if errors:
        raise RuntimeError("; ".join(errors[:20]))
    log(f"verified {len(inventory['quality'])} textures, 0 import setting errors")


def main() -> None:
    inventory = json.loads(REPORT.read_text(encoding="utf-8"))
    imported = 0
    for folder in ("Backgrounds", "Panels", "Buttons", "Controls", "Avatars", "Icons", "Tiles"):
        destination = f"{DEST_ROOT}/{folder}"
        for source in sorted((SOURCE / folder).glob("*.png")):
            texture = import_texture(source, destination)
            if not texture:
                raise RuntimeError(f"failed to import {source}")
            configure_texture(texture, folder == "Backgrounds")
            imported += 1
    populate_data_assets(inventory)
    create_materials()
    verify(inventory)
    unreal.EditorAssetLibrary.save_directory("/Game/UI", only_if_is_dirty=False, recursive=True)
    data_asset_count = 6 if hasattr(unreal, "GuiyangUIThemeDataAsset") else 0
    log(f"completed: {imported} textures, {data_asset_count} data assets, 9 materials, 5 material instances")


if __name__ == "__main__":
    main()

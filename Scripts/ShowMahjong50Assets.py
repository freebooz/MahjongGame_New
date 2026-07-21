"""Open the Content Browser at the generated Mahjong50 tile collection."""

import unreal


TILE_FOLDER = "/Game/Art/Mahjong/Mahjong50/Tiles"
RED_DRAGON = f"{TILE_FOLDER}/SM_Mahjong50_Red_Dragon"


def show_assets() -> None:
    unreal.EditorAssetLibrary.make_directory(TILE_FOLDER)
    target = RED_DRAGON if unreal.EditorAssetLibrary.does_asset_exist(RED_DRAGON) else TILE_FOLDER
    unreal.EditorAssetLibrary.sync_browser_to_objects([target])
    unreal.log(f"[Mahjong50Import] Content Browser synced to {target}")


show_assets()

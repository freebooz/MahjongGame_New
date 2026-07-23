import unreal

MAP_PATH = "/Game/Maps/MahjongServerMap"

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
else:
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
if not world:
    raise RuntimeError("Could not create or load MahjongServerMap")
world_settings = world.get_world_settings()
server_mode = unreal.load_class(None, "/Script/GuiyangMahjongServer.GuiyangMahjongGameMode")
if not server_mode:
    raise RuntimeError("GuiyangMahjongServer game mode class is unavailable")
world_settings.set_editor_property("default_game_mode", server_mode)
if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
    raise RuntimeError("Could not save MahjongServerMap")
unreal.log("MAHJONG_SERVER_MAP_OK")

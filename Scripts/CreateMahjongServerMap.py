import unreal

MAP_PATH = "/Game/Maps/MahjongServerMap"

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    unreal.EditorLevelLibrary.load_level(MAP_PATH)
else:
    unreal.EditorLevelLibrary.new_level(MAP_PATH)

world = unreal.EditorLevelLibrary.get_editor_world()
world_settings = world.get_world_settings()
server_mode = unreal.load_class(None, "/Script/GuiyangMahjongServer.GuiyangMahjongGameMode")
if not server_mode:
    raise RuntimeError("GuiyangMahjongServer game mode class is unavailable")
world_settings.set_editor_property("default_game_mode", server_mode)
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save MahjongServerMap")
unreal.log("MAHJONG_SERVER_MAP_OK actors={}".format(len(unreal.EditorLevelLibrary.get_all_level_actors())))


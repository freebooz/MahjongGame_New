import unreal

MAP_PATH = "/Game/Maps/MahjongRoomMap"

unreal.EditorLevelLibrary.load_level(MAP_PATH)
world = unreal.EditorLevelLibrary.get_editor_world()
world_settings = world.get_world_settings()
client_mode = unreal.load_class(None, "/Script/Engine.GameModeBase")
if not client_mode:
    raise RuntimeError("Engine GameModeBase class is unavailable")
world_settings.set_editor_property("default_game_mode", client_mode)
if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Could not save sanitized MahjongRoomMap")
unreal.log("MAHJONG_CLIENT_MAP_SANITIZED")


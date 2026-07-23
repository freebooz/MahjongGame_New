import unreal

MAP_PATH = "/Game/Maps/MahjongRoomMap"

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if not world:
    raise RuntimeError("Could not load MahjongRoomMap")
world_settings = world.get_world_settings()
client_mode = unreal.load_class(None, "/Script/Engine.GameModeBase")
if not client_mode:
    raise RuntimeError("Engine GameModeBase class is unavailable")
world_settings.set_editor_property("default_game_mode", client_mode)
if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
    raise RuntimeError("Could not save sanitized MahjongRoomMap")
unreal.log("MAHJONG_CLIENT_MAP_SANITIZED")

"""Create the target-neutral map used by both network clients and servers."""

import unreal


MAP_PATH = "/Game/Maps/MahjongNetMap"


world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
if not world:
    raise RuntimeError("Could not create MahjongNetMap")

world_settings = world.get_world_settings()
# Do not serialize a ClientOnly or ServerOnly class into the shared map. The
# dedicated server injects its authoritative GameMode in the launch URL.
world_settings.set_editor_property("default_game_mode", None)

if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
    raise RuntimeError("Could not save MahjongNetMap")

unreal.log("MAHJONG_NET_MAP_OK")

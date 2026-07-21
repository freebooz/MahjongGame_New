import time

import unreal


_open_game_hud_started_at = time.time()
_open_game_hud_callback = None


def _open_game_hud_editor(_delta_seconds):
    global _open_game_hud_callback
    if time.time() - _open_game_hud_started_at < 2.0:
        return

    asset = unreal.load_asset('/Game/UI/Screens/WBP_GameHUD')
    if asset:
        subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        subsystem.open_editor_for_assets([asset])
        unreal.log('MAHJONG_GAMEHUD_EDITOR_OPENED')
    else:
        unreal.log_error('Unable to load /Game/UI/Screens/WBP_GameHUD')

    unreal.unregister_slate_post_tick_callback(_open_game_hud_callback)
    _open_game_hud_callback = None


if '-OpenMahjongGameHUD' in unreal.SystemLibrary.get_command_line():
    _open_game_hud_callback = unreal.register_slate_post_tick_callback(_open_game_hud_editor)

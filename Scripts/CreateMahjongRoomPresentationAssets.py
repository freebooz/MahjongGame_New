"""Create the client presentation Blueprint and migrate the visual preview map to it."""

import unreal


ASSET_NAME = "BP_MahjongRoomPresentation"
ASSET_DIR = "/Game/Client/Room/Presentation"
ASSET_PATH = f"{ASSET_DIR}/{ASSET_NAME}"
GENERATED_CLASS_PATH = f"{ASSET_PATH}.{ASSET_NAME}_C"
LEGACY_LABEL_PATH = f"{ASSET_DIR}/PAL_MahjongRoomPresentation_Client"
MAP_PATH = "/Game/Maps/MahjongRoomVisualPreviewMap"
NATIVE_CLASS_PATH = "/Script/GuiyangMahjongClient.MahjongRoomPresentationActor"


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
parent_class = unreal.load_class(None, NATIVE_CLASS_PATH)
if not parent_class:
    raise RuntimeError("MahjongRoomPresentationActor must be compiled before creating its Blueprint")

blueprint = (
    unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH)
    else None
)
if not blueprint:
    unreal.EditorAssetLibrary.make_directory(ASSET_DIR)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = asset_tools.create_asset(ASSET_NAME, ASSET_DIR, None, factory)
    if not blueprint:
        raise RuntimeError(f"Could not create {ASSET_PATH}")
    unreal.log(f"MAHJONG_PRESENTATION_BLUEPRINT_CREATED={ASSET_PATH}")
else:
    unreal.log(f"MAHJONG_PRESENTATION_BLUEPRINT_REUSED={ASSET_PATH}")

unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
presentation_class = unreal.load_class(None, GENERATED_CLASS_PATH)
if not presentation_class:
    raise RuntimeError(f"Could not load generated class {GENERATED_CLASS_PATH}")

# Client platform configs explicitly cook this directory. A global AlwaysCook
# PrimaryAssetLabel would override the Dedicated Server NeverCook boundary and
# pull the complete presentation dependency graph into the server package.
legacy_label_registered = unreal.EditorAssetLibrary.does_asset_exist(LEGACY_LABEL_PATH)
print(f"MAHJONG_PRESENTATION_LEGACY_LABEL_REGISTERED={legacy_label_registered}")
if legacy_label_registered:
    legacy_label = unreal.EditorAssetLibrary.load_asset(LEGACY_LABEL_PATH)
    legacy_label.set_editor_property("label_assets_in_my_directory", False)
    legacy_label.set_editor_property("is_runtime_label", False)
    legacy_rules = legacy_label.get_editor_property("rules")
    legacy_rules.set_editor_property("cook_rule", unreal.PrimaryAssetCookRule.NEVER_COOK)
    legacy_label.set_editor_property("rules", legacy_rules)
    unreal.EditorAssetLibrary.save_loaded_asset(legacy_label, only_if_is_dirty=False)
    unreal.log(
        f"MAHJONG_PRESENTATION_LEGACY_LABEL_DISABLED={LEGACY_LABEL_PATH}"
    )

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if not world:
    raise RuntimeError(f"Could not load {MAP_PATH}")

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_subsystem.get_all_level_actors()
removed_labels = []
for actor in actors:
    class_name = actor.get_class().get_name()
    if (
        "MahjongRoomPresentation" in class_name
        or class_name in ("DirectionalLight", "SkyLight")
    ):
        removed_labels.append(actor.get_actor_label())
        actor_subsystem.destroy_actor(actor)

presentation = actor_subsystem.spawn_actor_from_class(
    presentation_class, unreal.Vector(), unreal.Rotator()
)
if not presentation:
    raise RuntimeError("Could not place BP_MahjongRoomPresentation")
presentation.set_actor_label("MahjongRoomPresentation")

if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH):
    raise RuntimeError(f"Could not save {MAP_PATH}")

unreal.log(f"MAHJONG_PRESENTATION_REMOVED_PREVIEW_ACTORS={removed_labels}")
unreal.log(f"MAHJONG_PRESENTATION_RUNTIME_CLASS={presentation.get_class().get_path_name()}")
unreal.log("MAHJONG_PRESENTATION_ASSETS_OK")

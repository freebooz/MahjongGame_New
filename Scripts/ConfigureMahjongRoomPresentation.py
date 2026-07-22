import unreal


MAP_PATH = "/Game/Maps/MahjongRoomMap"
TABLE_LABEL = "MahjongRoomTable"
TABLE_TAG = "MahjongRoomTable"
CAMERA_LABEL = "MahjongRoomCamera"
CAMERA_TAG = "MahjongRoomCamera"
CAMERA_PRESET_TAG = "MahjongRoomCameraPresetV1"
LIGHT_LABEL = "MahjongStableRoomLight"
LIGHT_TAG = "MahjongStableRoomLight"
LIGHT_PRESET_TAG = "MahjongStableRoomLightPresetV4"


def class_name(actor):
    actor_class = actor.get_class() if actor else None
    return actor_class.get_name() if actor_class else ""


def tags(actor):
    try:
        return [str(value) for value in actor.get_editor_property("tags")]
    except Exception:
        return []


if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    raise RuntimeError(f"Room map does not exist: {MAP_PATH}")

unreal.EditorLevelLibrary.load_level(MAP_PATH)
table_class = unreal.load_class(None, "/Script/GuiyangMahjong.Mahjong3DTableActor")
camera_class = unreal.load_class(None, "/Script/GuiyangMahjong.MahjongRoomCameraActor")
if not table_class or not camera_class:
    raise RuntimeError("Mahjong room presentation classes are not loaded")

room_table = None
room_camera = None
room_light = None
for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
    label = actor.get_actor_label() if hasattr(actor, "get_actor_label") else ""
    actor_tags = tags(actor)
    name = class_name(actor)

    if name == "Mahjong3DTableActor":
        if room_table is None:
            room_table = actor
        else:
            unreal.EditorLevelLibrary.destroy_actor(actor)
        continue

    # Replace the legacy standalone imported table actor. The same mesh is now a persistent,
    # editor-visible component on AMahjong3DTableActor, so keeping both would z-fight.
    if label == TABLE_LABEL or TABLE_TAG in actor_tags:
        unreal.EditorLevelLibrary.destroy_actor(actor)
        continue

    if name == "MahjongRoomCameraActor":
        if room_camera is None:
            room_camera = actor
        else:
            unreal.EditorLevelLibrary.destroy_actor(actor)

    if name == "SpotLight" and room_light is None:
        room_light = actor

if room_table is None:
    room_table = unreal.EditorLevelLibrary.spawn_actor_from_class(
        table_class,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
    )
if not room_table:
    raise RuntimeError("Could not place AMahjong3DTableActor in MahjongRoomMap")
room_table.set_actor_label(TABLE_LABEL)
room_table.set_editor_property("tags", [unreal.Name(TABLE_TAG)])
room_table.set_actor_location(unreal.Vector(0.0, 0.0, 0.0), False, False)
room_table.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)

if room_camera is None:
    room_camera = unreal.EditorLevelLibrary.spawn_actor_from_class(
        camera_class,
        unreal.Vector(0.0, -950.0, 1320.0),
        unreal.Rotator(roll=0.0, pitch=-54.25, yaw=90.0),
    )
if not room_camera:
    raise RuntimeError("Could not place AMahjongRoomCameraActor in MahjongRoomMap")
room_camera.set_actor_label(CAMERA_LABEL)
if CAMERA_PRESET_TAG not in tags(room_camera):
    room_camera.set_actor_location(unreal.Vector(0.0, -950.0, 1320.0), False, False)
    room_camera.set_actor_rotation(
        unreal.Rotator(roll=0.0, pitch=-54.25, yaw=90.0), False
    )
    room_camera.set_editor_property(
        "tags", [unreal.Name(CAMERA_TAG), unreal.Name(CAMERA_PRESET_TAG)]
    )

camera_component = room_camera.get_component_by_class(unreal.CineCameraComponent)
if not camera_component:
    raise RuntimeError("Mahjong room camera has no CineCameraComponent")

if room_light is None:
    room_light = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SpotLight,
        unreal.Vector(0.0, 0.0, 1200.0),
        unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0),
    )
if not room_light:
    raise RuntimeError("Could not place the stable Mahjong room light")
room_light.set_actor_label(LIGHT_LABEL)
if LIGHT_PRESET_TAG not in tags(room_light):
    room_light.set_actor_location(unreal.Vector(0.0, 0.0, 1200.0), False, False)
    room_light.set_actor_rotation(
        unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0), False
    )
    room_light.set_editor_property(
        "tags", [unreal.Name(LIGHT_TAG), unreal.Name(LIGHT_PRESET_TAG)]
    )
    spot_component = room_light.get_component_by_class(unreal.SpotLightComponent)
    if not spot_component:
        raise RuntimeError("Stable Mahjong room light has no SpotLightComponent")
    spot_component.set_editor_property("intensity", 800.0)
    spot_component.set_editor_property("attenuation_radius", 3000.0)
    spot_component.set_editor_property("inner_cone_angle", 40.0)
    spot_component.set_editor_property("outer_cone_angle", 65.0)
    spot_component.set_editor_property("cast_shadows", False)

table_origin, table_extent = room_table.get_actor_bounds(False, False)
level_lights = []
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    actor_class_name = class_name(actor)
    if "Light" in actor_class_name:
        light_component = actor.get_component_by_class(unreal.LightComponent)
        light_details = ""
        if light_component:
            light_details = (
                f" intensity={light_component.get_editor_property('intensity')}"
                f" color={light_component.get_editor_property('light_color')}"
                f" visible={light_component.get_editor_property('visible')}"
            )
        level_lights.append(
            f"{actor.get_actor_label()}:{actor_class_name}@{actor.get_actor_location()}"
            f" rotation={actor.get_actor_rotation()}{light_details}"
        )

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError(f"Could not save level {MAP_PATH}")

unreal.log(
    "[MahjongRoomPresentation] configured "
    f"table={room_table.get_path_name()} camera={room_camera.get_path_name()} "
    f"camera_location={room_camera.get_actor_location()} "
    f"camera_rotation={room_camera.get_actor_rotation()} "
    f"focal_length={camera_component.get_editor_property('current_focal_length')}mm "
    f"table_origin={table_origin} table_extent={table_extent} "
    f"lights={level_lights}"
)

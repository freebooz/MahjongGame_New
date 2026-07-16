import unreal

asset_path = "/Game/Maps/UIReviewMap"
if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
    unreal.EditorLevelLibrary.new_level(asset_path)
else:
    unreal.EditorLevelLibrary.load_level(asset_path)
unreal.EditorLevelLibrary.save_current_level()
unreal.log("[GuiyangUIReview] review map ready: /Game/Maps/UIReviewMap")

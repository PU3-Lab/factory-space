# -*- coding: utf-8 -*-
"""#186 빌드캠 정탑다운 — BP_OJJ_BuildCamera CDO의 CameraPitch를 -90으로(spawn 클래스가 BP여도 적용)."""
import unreal

BP = "/Game/OJJ/BP/BP_OJJ_BuildCamera"
TARGET = -90.0

bp = unreal.EditorAssetLibrary.load_asset(BP)
gc = bp.generated_class()
cdo = unreal.get_default_object(gc)

try:
    old = cdo.get_editor_property("camera_pitch")
except Exception as e:
    old = "?({})".format(e)
unreal.log("[pitch] BP CDO camera_pitch before = {}".format(old))

cdo.set_editor_property("camera_pitch", TARGET)
ok = unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
unreal.log("[pitch] set -> {} saved={}".format(cdo.get_editor_property("camera_pitch"), ok))
unreal.log("[pitch] DONE")

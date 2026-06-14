# -*- coding: utf-8 -*-
"""#185 별 돔 — 배치된 StarDome 인스턴스의 DomeScale/스케일을 맵을 감싸는 값으로 갱신 후 저장."""
import unreal

LEVEL = "/Game/OJJ/Levels/L_Planet"
TARGET_SCALE = 1500.0  # 반경 ≈ 750m (맵 half-extent 504m의 ~1.5배)

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(LEVEL)

changed = False
for a in eas.get_all_level_actors():
    if a.get_class().get_name() != "OJJ_StarDome":
        continue
    a.set_editor_property("dome_scale", TARGET_SCALE)
    a.set_actor_scale3d(unreal.Vector(TARGET_SCALE, TARGET_SCALE, TARGET_SCALE))
    s = a.get_actor_scale3d()
    unreal.log("[stars] StarDome scale -> ({:.0f},{:.0f},{:.0f}) DomeScale={}".format(
        s.x, s.y, s.z, a.get_editor_property("dome_scale")))
    changed = True

if changed:
    les.save_current_level()
    unreal.log("[stars] level saved")
else:
    unreal.log_warning("[stars] no StarDome found")
unreal.log("[stars] FIX DONE")

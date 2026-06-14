# -*- coding: utf-8 -*-
"""
#185 밤하늘 별 — 레벨 와이어링 (헤드리스 commandlet용)
  L_Planet 에 대해:
    1) AOJJ_DayNightController 인스턴스의 StarCollection <- MPC_Sky 연결
    2) AOJJ_StarDome 1개 배치(없으면)
    3) 레벨 저장

실행: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<이 파일>"
⚠️ 기존 액터/머티리얼은 수정하지 않고, 컨트롤러 프로퍼티 1개 세팅 + 돔 1개 추가만.
"""
import unreal

LEVEL = "/Game/OJJ/Levels/L_Planet"
MPC = "/Game/OJJ/Sky/MPC_Sky"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
asset_lib = unreal.EditorAssetLibrary

changed = False

if not les.load_level(LEVEL):
    unreal.log_error("[stars] load_level failed: {}".format(LEVEL))
    raise SystemExit(1)

mpc = asset_lib.load_asset(MPC)
actors = eas.get_all_level_actors()

# 1) DayNightController.StarCollection <- MPC_Sky
controllers = [a for a in actors if a.get_class().get_name() == "OJJ_DayNightController"]
if not controllers:
    unreal.log_warning("[stars] OJJ_DayNightController not found in level")
for c in controllers:
    c.set_editor_property("star_collection", mpc)
    unreal.log("[stars] wired StarCollection on {}".format(c.get_name()))
    changed = True

# 2) StarDome 배치(중복 방지)
domes = [a for a in actors if a.get_class().get_name() == "OJJ_StarDome"]
if domes:
    unreal.log_warning("[stars] StarDome already present ({}), skip spawn".format(len(domes)))
else:
    dome_cls = unreal.load_class(None, "/Script/Wanted_Factory.OJJ_StarDome")
    dome = eas.spawn_actor_from_class(dome_cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if dome:
        dome.set_actor_label("OJJ_StarDome")
        unreal.log("[stars] spawned StarDome")
        changed = True
    else:
        unreal.log_error("[stars] spawn StarDome failed")

# 3) 저장
if changed:
    les.save_current_level()
    unreal.log("[stars] level saved: {}".format(LEVEL))
else:
    unreal.log("[stars] nothing changed")

unreal.log("[stars] WIRE DONE")

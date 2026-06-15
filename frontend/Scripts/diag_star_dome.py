# -*- coding: utf-8 -*-
"""#185 별 돔 진단 — 맵 extent / 돔 현황 / 카메라 far clip 보고 (수정 없음)."""
import unreal

LEVEL = "/Game/OJJ/Levels/L_Planet"
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level(LEVEL)

actors = eas.get_all_level_actors()

# 1) 월드 extent — 돔 제외한 모든 액터 origin+extent 합집합 박스
mn = unreal.Vector(1e30, 1e30, 1e30)
mx = unreal.Vector(-1e30, -1e30, -1e30)
counted = 0
for a in actors:
    if a.get_class().get_name() == "OJJ_StarDome":
        continue
    try:
        origin, ext = a.get_actor_bounds(only_colliding_components=False)
    except Exception:
        continue
    if ext.x == 0 and ext.y == 0 and ext.z == 0:
        continue
    counted += 1
    for axis in ("x", "y", "z"):
        lo = getattr(origin, axis) - getattr(ext, axis)
        hi = getattr(origin, axis) + getattr(ext, axis)
        setattr(mn, axis, min(getattr(mn, axis), lo))
        setattr(mx, axis, max(getattr(mx, axis), hi))

size_x = mx.x - mn.x
size_y = mx.y - mn.y
size_z = mx.z - mn.z
half_diag = (max(size_x, size_y, size_z)) * 0.5
unreal.log("[diag] actors counted={} extent(cm) X={:.0f} Y={:.0f} Z={:.0f}".format(counted, size_x, size_y, size_z))
unreal.log("[diag] half-largest(cm)={:.0f}  (= {:.1f} m)".format(half_diag, half_diag / 100.0))

# 엔진 Sphere 반경 = 50 cm. 돔 반경 = 50 * scale. 맵을 1.5배 여유로 감싸려면:
need_radius = half_diag * 1.5
suggest_scale = need_radius / 50.0
unreal.log("[diag] suggested dome radius(cm)={:.0f}  -> DomeScale ~= {:.0f}".format(need_radius, suggest_scale))

# 2) 돔 현황
for a in actors:
    if a.get_class().get_name() == "OJJ_StarDome":
        s = a.get_actor_scale3d()
        try:
            ds = a.get_editor_property("dome_scale")
        except Exception:
            ds = "?"
        unreal.log("[diag] StarDome scale3d=({:.0f},{:.0f},{:.0f}) DomeScale_prop={}".format(s.x, s.y, s.z, ds))

# 3) far clip — UCameraComponent 들 점검 (없으면 UE5 기본 무한 reversed-Z)
cam_found = False
for a in actors:
    comps = a.get_components_by_class(unreal.CameraComponent)
    for c in comps:
        cam_found = True
        unreal.log("[diag] CameraComponent on {} (UE5 perspective는 기본 무한 far/reversed-Z)".format(a.get_name()))
if not cam_found:
    unreal.log("[diag] 레벨에 CameraComponent 없음 — 플레이어 폰 카메라 사용. UE5 기본 far=무한(reversed-Z)이라 거대 돔도 렌더됨.")

unreal.log("[diag] DONE")

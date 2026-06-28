# -*- coding: utf-8 -*-
# 로봇 포트레이트 MVP 셋업 (에디터 내부 Python 콘솔/Execute Python Script로 실행)
#  1) RT_RobotPortrait (512x512, RGBA8) RenderTarget 생성/재사용
#  2) AOJJ_PortraitCapture 액터를 현재 레벨에 스폰(지하 -5000, 메인뷰 격리)/재사용
#  3) 액터의 PortraitRenderTarget 슬롯에 RT 할당 + RT 에셋 저장
#
# 안전: 에디터 내부 실행이므로 헤드리스 저장 충돌 없음. 레벨(.umap)은 저장하지 않음(사용자 판단).
# 사전조건: Live Coding(Ctrl+Alt+F11)으로 새 클래스가 컴파일/인식된 상태여야 함.

import unreal

RT_FOLDER = "/Game/OJJ/Character/Robot"
RT_NAME = "RT_RobotPortrait"
RT_PATH = "{}/{}".format(RT_FOLDER, RT_NAME)
CLASS_PATH = "/Script/Wanted_Factory.OJJ_PortraitCapture"
SPAWN_LOC = unreal.Vector(0.0, 0.0, -5000.0)  # 메인뷰에 안 잡히도록 지하

log = unreal.log
warn = unreal.log_warning
err = unreal.log_error

# ---- 0) 새 클래스 인식 확인 ----
actor_class = unreal.load_class(None, CLASS_PATH)
if actor_class is None:
    err("[Portrait] OJJ_PortraitCapture 클래스를 못 찾음 — Live Coding(Ctrl+Alt+F11)으로 컴파일 후 다시 실행하세요.")
    raise SystemExit

# ---- 1) RenderTarget 생성/재사용 ----
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist(RT_PATH):
    rt = eal.load_asset(RT_PATH)
    log("[Portrait] 기존 RT 재사용: {}".format(RT_PATH))
else:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.TextureRenderTargetFactoryNew()
    rt = asset_tools.create_asset(RT_NAME, RT_FOLDER, unreal.TextureRenderTarget2D, factory)
    log("[Portrait] RT 신규 생성: {}".format(RT_PATH))

# 크기/포맷 설정 (512x512, RGBA8)
rt.set_editor_property("size_x", 512)
rt.set_editor_property("size_y", 512)
rt.set_editor_property("render_target_format", unreal.TextureRenderTargetFormat.RTF_RGBA8)

# ---- 2) 액터 스폰/재사용 ----
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
existing = [a for a in actor_sub.get_all_level_actors() if a.get_class() == actor_class]
if existing:
    actor = existing[0]
    log("[Portrait] 기존 액터 재사용: {}".format(actor.get_actor_label()))
else:
    actor = actor_sub.spawn_actor_from_class(actor_class, SPAWN_LOC, unreal.Rotator(0, 0, 0))
    actor.set_actor_label("OJJ_PortraitCapture")
    log("[Portrait] 액터 스폰 @ {}".format(SPAWN_LOC))

# ---- 3) RT 할당 + 저장 ----
actor.set_editor_property("PortraitRenderTarget", rt)
eal.save_asset(RT_PATH)
log("[Portrait] 완료 — PortraitRenderTarget 할당 + RT 저장. 이제 PIE 실행 후 RT_RobotPortrait를 더블클릭해 확인하세요.")
log("[Portrait] (액터를 레벨에 영구 보존하려면 레벨을 저장하세요.)")

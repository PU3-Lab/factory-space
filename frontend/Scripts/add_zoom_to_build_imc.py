# -*- coding: utf-8 -*-
"""#186 휠 줌 — IMC_Build에 IA_Zoom→MouseWheelAxis 매핑 추가.

UE5.7에서 IMC의 'mappings' 배열은 Python 읽기가 막혀 있어, map_key()/unmap_key() 메서드로 직접 조작.
중복 방지: unmap 후 map(멱등). IA_Zoom은 Axis1D 값 → MouseWheelAxis(휠 위/아래 ±1)와 일치.
"""
import unreal

IMC_BUILD = "/Game/OJJ/Input/IMC_Build"
IA_ZOOM = "/Game/OJJ/Input/IA_Zoom"

asset_lib = unreal.EditorAssetLibrary
build = asset_lib.load_asset(IMC_BUILD)
zoom_action = asset_lib.load_asset(IA_ZOOM)


def make_wheel_key():
    # FKey(MouseWheelAxis) 생성 — 여러 형태 시도(버전 호환).
    try:
        return unreal.Key("MouseWheelAxis")
    except Exception:
        pass
    k = unreal.Key()
    try:
        k.set_editor_property("key_name", "MouseWheelAxis")
    except Exception:
        pass
    return k


key = make_wheel_key()
unreal.log("[zoom] wheel key = {}".format(str(key)))

# 멱등: 기존 IA_Zoom 매핑 제거 후 재추가
try:
    build.unmap_key(zoom_action, key)
    unreal.log("[zoom] unmapped existing (if any)")
except Exception as e:
    unreal.log("[zoom] unmap skip: {}".format(e))

mapping = build.map_key(zoom_action, key)
unreal.log("[zoom] map_key returned: {}".format(str(mapping)))

# 메모리 객체를 직접 강제 저장(경로 저장이 in-memory 변경을 못 잡는 문제 회피).
ok = unreal.EditorAssetLibrary.save_loaded_asset(build, only_if_is_dirty=False)
unreal.log("[zoom] save_loaded_asset ok={}".format(ok))
unreal.log("[zoom] DONE")

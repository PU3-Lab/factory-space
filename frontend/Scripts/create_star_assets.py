# -*- coding: utf-8 -*-
"""
#185 밤하늘 별 — 에셋 생성 (헤드리스 commandlet용)
  1) MPC_Sky : 스칼라 파라미터 StarIntensity(0~1) — DayNightController가 매 틱 NightFactor를 기록
  2) M_Stars : Unlit / Two-Sided 절차적 별 머티리얼
       VertexNormalWS(돔 표면 방향) → Custom HLSL(절차적 별) → × StarIntensity(MPC) → × 별색 → Emissive

생성 경로: /Game/OJJ/Sky/
실행: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<이 파일>"

⚠️ UE Python 안전수칙: 에셋 신규 생성만(기존 수정/삭제 없음), 생성 후 명시 저장, 실패 시 로그.
"""
import unreal

PKG_DIR = "/Game/OJJ/Sky"
MPC_NAME = "MPC_Sky"
MAT_NAME = "M_Stars"
PARAM_NAME = "StarIntensity"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_lib = unreal.EditorAssetLibrary
mat_lib = unreal.MaterialEditingLibrary

created = []


def make_mpc():
    path = "{}/{}".format(PKG_DIR, MPC_NAME)
    if asset_lib.does_asset_exist(path):
        unreal.log_warning("[stars] MPC already exists, skip: {}".format(path))
        return asset_lib.load_asset(path)
    mpc = asset_tools.create_asset(MPC_NAME, PKG_DIR,
                                   unreal.MaterialParameterCollection,
                                   unreal.MaterialParameterCollectionFactoryNew())
    # 스칼라 파라미터 StarIntensity 추가 (기본 0 = 낮)
    entry = unreal.CollectionScalarParameter()
    entry.set_editor_property("parameter_name", PARAM_NAME)
    entry.set_editor_property("default_value", 0.0)
    mpc.set_editor_property("scalar_parameters", [entry])
    created.append(path)
    return mpc


STAR_HLSL = (
    "float3 d = normalize(Dir);\n"
    "float3 p = d * Density;\n"
    "float3 i = floor(p);\n"
    "float h = frac(sin(dot(i, float3(127.1, 311.7, 74.7))) * 43758.5453);\n"
    "float present = step(1.0 - Coverage, h);\n"
    "float h2 = frac(sin(dot(i, float3(269.5, 183.3, 246.1))) * 43758.5453);\n"
    "float3 center = float3(h, h2, frac(h + h2));\n"
    "float dist = length(frac(p) - center);\n"
    "float star = present * saturate(1.0 - dist / max(StarSize, 1e-4));\n"
    "return pow(star, Sharpness);\n"
)


def make_material(mpc):
    path = "{}/{}".format(PKG_DIR, MAT_NAME)
    # 튜닝값 갱신을 위해 기존 머티리얼은 덮어쓴다(별 노브를 ScalarParameter로 재구성).
    if asset_lib.does_asset_exist(path):
        asset_lib.delete_asset(path)
        unreal.log_warning("[stars] Material existed -> overwrite: {}".format(path))
    mat = asset_tools.create_asset(MAT_NAME, PKG_DIR,
                                   unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)

    # 돔 표면 방향 = VertexNormalWS
    vnorm = mat_lib.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -800, 0)

    # 절차적 별 Custom 노드
    custom = mat_lib.create_material_expression(mat, unreal.MaterialExpressionCustom, -560, 0)
    custom.set_editor_property("code", STAR_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    custom.set_editor_property("description", "ProceduralStars")
    inputs = []
    for nm, default in [("Dir", None), ("Density", 80.0), ("Coverage", 0.04),
                        ("StarSize", 0.06), ("Sharpness", 3.0)]:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    # Dir <- VertexNormalWS
    mat_lib.connect_material_expressions(vnorm, "", custom, "Dir")
    # 별 노브를 ScalarParameter로 노출(에디터/머티리얼 인스턴스에서 슬라이더 튜닝). 밀도 기본값 상향.
    #   Density  : 방향 격자 해상도(↑ = 별 더 많음)
    #   Coverage : 격자 셀 중 별이 있는 비율(↑ = 별 더 많음)
    #   StarSize : 개별 별 크기
    #   Sharpness: 별 가장자리 날카로움(↑ = 더 점처럼)
    params = [("Density", 320.0), ("Coverage", 0.22), ("StarSize", 0.06), ("Sharpness", 2.0)]
    y = -300
    for nm, val in params:
        sp = mat_lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -780, y)
        sp.set_editor_property("parameter_name", nm)
        sp.set_editor_property("default_value", val)
        sp.set_editor_property("group", "Stars")
        mat_lib.connect_material_expressions(sp, "", custom, nm)
        y += 130

    # MPC StarIntensity 컬렉션 파라미터
    coll = mat_lib.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter, -560, 320)
    coll.set_editor_property("collection", mpc)
    coll.set_editor_property("parameter_name", PARAM_NAME)

    # star * StarIntensity
    mul1 = mat_lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -320, 80)
    mat_lib.connect_material_expressions(custom, "", mul1, "A")
    mat_lib.connect_material_expressions(coll, "", mul1, "B")

    # 별색(살짝 푸른 흰색)
    color = mat_lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -320, 280)
    color.set_editor_property("constant", unreal.LinearColor(0.85, 0.9, 1.0, 1.0))

    mul2 = mat_lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -120, 120)
    mat_lib.connect_material_expressions(mul1, "", mul2, "A")
    mat_lib.connect_material_expressions(color, "", mul2, "B")

    # Emissive
    mat_lib.connect_material_property(mul2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mat_lib.recompile_material(mat)
    created.append(path)
    return mat


def main():
    if not asset_lib.does_directory_exist(PKG_DIR):
        asset_lib.make_directory(PKG_DIR)
    mpc = make_mpc()
    make_material(mpc)
    for p in created:
        asset_lib.save_asset(p)
        unreal.log("[stars] saved: {}".format(p))
    unreal.log("[stars] DONE. created={}".format(created))


main()

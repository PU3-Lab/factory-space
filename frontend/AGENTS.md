## Unreal 자동화 작업 규칙

- **임시 Unreal Python 스크립트(`import unreal`)로 Blueprint, Asset, C++ 반영 작업을 우회하지 않는다.**
  - 사용자가 요청하더라도 `.py` 파일을 새로 만들어 `unreal.EditorAssetLibrary`, `unreal.AssetToolsHelpers`, `unreal.BlueprintFactory`, `unreal.KismetEditorUtilities`로 에셋을 직접 생성·컴파일하지 않는다.
  - 사용자가 Unreal Python 우회 실행을 요청하면 그대로 수행하지 말고, 크래시 위험과 commandlet 우선 원칙을 짧게 설명한 뒤 기존 commandlet/wrapper 기반 대안을 실행한다.
  - 특히 C++ 클래스 생성 직후 같은 흐름에서 Python으로 부모 Blueprint를 만들지 않는다. 새 C++ 클래스는 파일 생성만으로 Unreal reflection에 안정적으로 로드되지 않으며, UHT/빌드/모듈 로드/Editor 재시작이 필요할 수 있다.
  - 이 우회 경로가 필요해 보이면 즉시 중단하고, 왜 기존 commandlet으로 처리할 수 없는지 사용자에게 설명한 뒤 승인을 받는다.
- C++ 클래스와 Blueprint를 함께 다룰 때는 아래 순서를 분리한다.
  1. `tools/ue/generate_cpp_class.*` 또는 기존 C++ 생성 commandlet으로 클래스 파일을 생성한다.
  2. Unreal build/UHT 또는 `tools/ue/validate_cpp_reflection.*`, `tools/ue/validate_buildcs.*`로 새 클래스가 반영 가능한지 검증한다.
  3. 새 프로세스에서 기존 Blueprint 생성 commandlet(`create_character_bp.*`, `create_ai_controller.*`, `create_ai_flow.*`)을 실행한다.
  4. Result JSON과 Unreal 로그를 확인한 뒤 다음 단계로 진행한다.
- Blueprint/Asset 생성은 저장소의 기존 commandlet과 wrapper를 우선 사용한다.
  - 새 자동화가 필요하면 `sample/Plugins/UECommandForge/Source/UECommandForgeEditor/Private/Commandlets/` 아래 commandlet과 `tools/ue/` wrapper로 구현하고, smoke test를 추가한다.
  - 일회성 스크립트가 꼭 필요하면 프로젝트 루트에 만들지 말고 `sample/Saved/CodexReports/` 같은 임시 산출물 위치를 사용하며, 작업 종료 전 삭제 여부를 사용자에게 확인한다.
- Unreal 크래시가 발생하면 새 우회 스크립트를 만들기 전에 기존 로그를 먼저 수집한다.
  - 프로젝트 로그: `<Project>/Saved/Logs`
  - 프로젝트 크래시 리포트: `<Project>/Saved/Crashes`
  - Windows CrashReportClient 로그: `%LOCALAPPDATA%\CrashReportClient\Saved\Crashes`
  - UECommandForge Result JSON: `<Project>/Saved/CodexReports`

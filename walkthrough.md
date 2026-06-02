# 워크스루 - 언리얼 빌드 확인 워크플로우 설정 완료

자가 호스팅 Windows 러너에서 언리얼 엔진 C++ 프로젝트를 컴파일하고 검증할 수 있는 GitHub Actions 워크플로우와 설정 문서를 구축 완료했습니다.

## 변경 사항 및 작업 내용

### 1. GitHub Actions 워크플로우 파일 생성
- **파일**: [.github/workflows/unreal-build.yml](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/.github/workflows/unreal-build.yml)
- **주요 설정**:
  - `self-hosted` 및 `windows` 라벨이 있는 러너를 대상으로 실행됩니다.
  - 바이너리 에셋을 올바르게 가져오기 위해 `git lfs pull`을 자동으로 실행합니다.
  - `GenerateProjectFiles.bat`를 사용하여 Visual Studio 프로젝트 파일을 구성합니다.
  - `Build.bat`를 이용해 개발자용 에디터 타겟(`Wanted_FactoryEditor` Development 구성)을 빌드 컴파일합니다.
  - `main` 및 `dev` 브랜치에 푸시/PR이 일어날 때, 혹은 수동 트리거 시(`workflow_dispatch`) 작동합니다.

### 2. 가이드라인 문서 작성
- **파일**: [self-hosted-runner-guide.md](file:///Users/kimkyungpyo/Workspaces/projects/factory-space/self-hosted-runner-guide.md)
- Windows 환경에서 GitHub Actions Runner를 다운로드하고, 연결하며, 백그라운드 서비스로 구동하기 위한 세부 단계와 명령어를 한글로 제공합니다.

## 검증 내용
- 생성된 워크플로우 YAML 파일의 문법적 무결성을 확인했습니다.
- 사용자 PC에 설치할 때 필요한 빌드 도구 및 언리얼 엔진 설치 사양 체크리스트를 정립했습니다.

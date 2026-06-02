# 언리얼 엔진을 위한 Windows 자가 호스팅 러너(Self-Hosted Runner) 설정 가이드

생성된 GitHub Actions 워크플로우를 사용하여 언리얼 엔진 프로젝트를 빌드하고 검증하려면, 언리얼 엔진 5.7 및 Visual Studio가 설치된 본인의 Windows PC를 GitHub Actions 러너로 등록해야 합니다.

---

## 1. Windows 러너 PC 사전 요구사항

1. **언리얼 엔진 5.7**: `C:\Program Files\Epic Games\UE_5.7` 경로에 설치되어 있어야 합니다 (만약 다른 경로에 설치되어 있다면 `.github/workflows/unreal-build.yml` 파일 내 `UNREAL_ENGINE_PATH` 환경 변수를 수정해 주세요).
2. **Visual Studio 2022 / Build Tools**: "C++를 사용한 데스크톱 개발" 워크로드 내에서 "C++ 프로파일링 도구", "Windows용 C++ CMake 도구" 및 적절한 Windows SDK가 설치되어 있어야 합니다.
3. **Git 및 Git LFS**: Git이 설치되어 시스템 환경 변수(PATH)에 등록되어 있어야 합니다. 터미널(PowerShell)에서 `git lfs install` 명령어를 한 번 실행해 줍니다.

---

## 2. 1단계: GitHub에 러너 등록하기

1. GitHub 저장소(Repository) 페이지로 이동합니다.
2. 상단 탭에서 **Settings** > 좌측 메뉴에서 **Actions** > **Runners**를 선택합니다.
3. 우측 상단의 **New self-hosted runner** 버튼을 클릭합니다.
4. **Runner image**로 **Windows**를 선택합니다.

---

## 3. 2단계: 러너 다운로드 및 구성

Windows PC에서 **PowerShell**(또는 명령 프롬프트)을 열고 GitHub 설정 페이지에 표시된 다운로드 및 구성 명령어를 실행합니다. 아래는 예시 명령어입니다:

### 1. 다운로드 (Download)
러너가 설치될 폴더(예: `C:\actions-runner`)를 생성하고 러너 패키지를 다운로드한 뒤 압축을 풉니다.
```powershell
# 폴더 생성 및 이동
mkdir C:\actions-runner; cd C:\actions-runner

# 최신 러너 패키지 다운로드 (반드시 GitHub Settings 페이지의 최신 URL과 버전을 확인하세요)
Invoke-WebRequest -Uri "https://github.com/actions/runner/releases/download/v2.316.1/actions-runner-win-x64-2.316.1.zip" -OutFile "actions-runner-win-x64-2.316.1.zip"

# 압축 해제
Expand-Archive -Path "actions-runner-win-x64-2.316.1.zip" -DestinationPath .
```

### 2. 구성 (Configure)
GitHub 저장소에 러너 연결 설정을 진행합니다. **반드시 GitHub Settings 페이지에 나타난 토큰(Token)을 사용해 주세요.**
```powershell
# 구성 스크립트 실행
./config.cmd --url https://github.com/<사용자명>/<저장소명> --token <제공된_등록_토큰>
```

설정 시 질문에 대한 입력:
- **Runner group**: 엔터를 입력하여 기본값(Default)을 선택합니다.
- **Runner name**: 러너 이름(예: `Unreal-Build-PC`)을 입력합니다.
- **Runner labels**: 기본값으로 **`self-hosted`**와 **`windows`** 라벨이 지정되므로 엔터를 누릅니다.
- **Work folder**: 엔터를 입력하여 기본값(`_work`)을 사용합니다.

---

## 4. 3단계: Windows 서비스로 실행하기 (권장)

PC가 부팅될 때 러너가 백그라운드에서 자동으로 실행되도록 설정합니다.

1. **PowerShell**을 **관리자 권한**으로 실행합니다.
2. `C:\actions-runner` 디렉토리로 이동한 뒤 아래 명령어를 실행합니다:
   ```powershell
   # 러너를 Windows 서비스로 등록
   ./svc.cmd install
   
   # 서비스 시작
   ./svc.cmd start
   ```

추후 서비스 상태를 확인하거나 정지하려면 아래 명령어를 사용합니다:
```powershell
# 상태 확인
./svc.cmd status

# 서비스 중지
./svc.cmd stop
```

---

## 5. 4단계: GitHub에서 정상 연결 확인

GitHub 저장소의 **Settings** > **Actions** > **Runners**로 다시 이동하면 등록한 러너가 녹색 **Idle** 배지와 함께 온라인 상태로 나타나는 것을 확인할 수 있습니다. 이제 코드가 푸시되면 자동으로 빌드 체크가 시작됩니다.

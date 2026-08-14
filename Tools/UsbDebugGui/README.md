# STM32 USB Debug Monitor

PySide6로 작성한 STM32 USB 디버그 GUI 프로토타입입니다.

현재 버전은 화면 구성만 포함하며 버튼 이벤트, USB CDC 통신, ELF 분석,
Watch 프로토콜 및 메모리 읽기 기능은 연결되어 있지 않습니다.

## 실행 준비

Python 3.10 이상에서 가상환경을 만들고 PySide6를 설치합니다.

```powershell
cd Tools\UsbDebugGui
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
```

## 실행

```powershell
python main.py
```

## 화면 구성

- USB CDC COM 포트 연결 영역
- ELF 변수 검색 및 선택 영역
- 변수별 Watch 주기 선택 영역
- 활성 Watch 테이블
- 실시간 그래프 미리보기
- Flash/SRAM Hex Viewer
- USB 송수신 로그

표시되는 COM 포트, 변수, 주소 및 값은 레이아웃 확인용 예시 데이터입니다.

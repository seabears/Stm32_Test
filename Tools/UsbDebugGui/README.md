# STM32 USB Debug Monitor

PySide6로 작성한 STM32 USB CDC 디버그 GUI입니다. 빌드한 ELF의 `.symtab`에서
RAM 전역/정적 변수 이름, 주소, 크기를 읽고 STM32에 메모리 읽기 요청을 보냅니다.

## 실행 준비

Python 3.9 이상에서 가상환경을 만들고 의존성을 설치합니다.

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

## 사용 순서

1. GUI를 실행하고 `ELF 열기`에서 보드에 실제로 다운로드한 ELF를 선택합니다.
   선택 경로는 저장되며, 다음 실행 때 그 파일이 존재하면 자동으로 다시 엽니다.
2. STM32 USB CDC 포트를 선택하고 `연결`을 누릅니다.
3. 변수 탐색기에서 변수를 한 번 클릭하면 즉시 Watch가 활성화됩니다. 활성 변수는
   탐색기에서 녹색 음영으로 구분됩니다.
4. 메모리 탭에서는 임의의 허용 주소와 길이를 직접 읽을 수 있습니다.

Watch의 각 변수 행에서 `타입 값`/`RAW 값` 선택 상자로 표시 방식을 개별 변경할 수
있습니다. 행의 `그래프 열기` 버튼을 누르면 해당 변수만 표시하는 별도 실시간 그래프
창이 열립니다. 배열은 첫 번째 원소를 그래프로 사용합니다.
`타입 값`은 ELF의 DWARF 정보를 이용해 signed/unsigned 정수, float, bool, 포인터와
기본형 배열을 C 타입에 맞게 해석합니다. `RAW 값`은 같은 수신 바이트를 little-endian
16진수와 부호 없는 정수로 표시합니다. 전환할 때 STM32에 다시 요청하지 않고 마지막으로
받은 데이터를 다시 표시합니다. 구조체와 union은 현재 RAW로 표시하며, 8바이트보다 큰
심볼은 Watch에서 앞 8바이트만 표시합니다. 메모리 탭의 긴 읽기는 8바이트 요청 여러 개로
분할합니다.

Watch는 `READ_MULTI(5)` 명령을 사용합니다. 한 요청에 최대 8개 주소를 넣으며,
응답 데이터 총합은 USB FS 한 패킷에 맞춰 최대 58바이트입니다. 요청은
`[command][count] + count * [address LE32][length]`, 응답은
`[command][status][sequence LE16][count][data length][data...]` 형식입니다.
기존 10바이트 단일 READ 명령은 메모리 탭과 호환 테스트용으로 유지됩니다.

보드에 다운로드한 펌웨어와 다른 ELF를 선택하면 변수 주소가 달라질 수 있으므로
반드시 같은 빌드의 `Debug/stm32f103c8t6.elf`를 사용해야 합니다.

## 화면 구성

- USB CDC COM 포트 검색 및 연결
- ELF RAM 변수 검색과 주소/크기 표시
- 변수별 주기적 메모리 읽기 Watch
- 8바이트 단위 자동 분할 메모리 읽기
- 변수별 별도 실시간 그래프 창
- Flash/SRAM Hex Viewer
- 요청/응답 RAW HEX 통신 로그

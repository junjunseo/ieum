# 그래프와 실행으로 보는 3분 데모

관련: [#28](https://github.com/junjunseo/ieum/issues/28), F5 [#20](https://github.com/junjunseo/ieum/issues/20).
이 문서는 초기 시연 동선입니다. 발표 자료와 대본의 최종 완성은 F5에서 진행합니다.

## 준비

1. 저장소를 내려받고 README에 따라 빌드합니다.
2. `docs/graphs/index.html`을 브라우저에서 엽니다. 뷰어는 HTML에 SVG·코드·진단을 포함하므로 서버, 인터넷, Graphviz 없이 사용할 수 있습니다.
3. 터미널을 저장소 루트에 열고 아래 명령을 준비합니다.
4. 시연 중 네트워크에 의존하지 않도록 저장된 뷰어와 로컬 실행 파일을 사용합니다.

## 3분 진행 순서

| 시간 | 보여줄 내용 | 설명 |
|---|---|---|
| 0:00~0:25 | 대표 예제의 순환 의존 | “실행 전에 잘못된 모듈 연결을 찾아냅니다. 빨간 경로가 원인입니다.” |
| 0:25~1:00 | 수정 전후 → 1. 정상 구조 실행 | ui → service → data와 실제 함수 3회·호출 2회 실행을 보여줍니다. |
| 1:00~1:45 | 2. 의존 한 줄로 위반 발생 | data 선언에 `depends ui`를 추가한 차이를 설명하고 실행이 차단됨을 보여줍니다. |
| 1:45~2:20 | 3. 수정 후 다시 실행 | 역방향 의존을 제거하면 그래프와 실행이 정상으로 돌아옵니다. |
| 2:20~2:45 | 직접·전이 계층 위반 예제 | 회색 점선은 계층, 빨간 실선은 위반 의존임을 비교합니다. |
| 2:45~3:00 | 현재 범위 | unit 함수 호출 실행과 구조 검사까지 구현했으며 수치 연산·반환·조건문은 후속 범위임을 설명합니다. |

## 직접 재현

```powershell
.\build\ieum.exe .\examples\demo\01-valid.ieum --run ui.main
.\build\ieum.exe .\examples\demo\02-violation.ieum --run ui.main
.\build\ieum.exe .\examples\demo\03-fixed.ieum --run ui.main
```

| 단계 | 종료 코드 | 기대 결과 |
|---|---:|---|
| 정상 | 0 | `enter ui.main`부터 `exit ui.main`까지 함수 3회·호출 2회 |
| 위반 | 1 | 순환 1건, 계층 위반 3건, 실행 Trace 없음 |
| 수정 | 0 | 정상과 같은 실행 결과 |

위반의 근거는 순환 `data → ui → service → data`, 직접 역행 `data → ui`, 간접 역행 `data → ui → service`와 `service → data → ui`입니다. “한 줄 수정”은 주석을 제외한 프로그램 선언 기준입니다.

Python 3.9 이상에서는 세 단계를 5회 연속 검증하고 각 실행의 로그를 저장할 수 있습니다.

```powershell
python scripts/demo.py --repeat 5
```

로그: `build/demo/results.json`. 빌드 위치가 다르면 `--ieum build/Release/ieum.exe`를 지정합니다. Ubuntu에서는 `--ieum build/ieum`을 사용합니다. 자동 재생 통과는 발표자가 3분 안에 설명하는 리허설의 성공을 뜻하지 않습니다.

## 복구와 사전 점검

- 뷰어가 안 보이면 `docs/graphs/cyclic_dependency.svg`를 직접 엽니다.
- Graphviz가 없어도 저장된 SVG와 뷰어를 볼 수 있습니다. 소스를 바꿨다면 [그래프 재생성](GRAPH_EXPORT.md#svg와-오프라인-뷰어)을 먼저 수행합니다.
- 실행 파일이 없다면 `scripts/build.ps1`로 빌드합니다. 컴파일러가 없는 시연 환경을 위해 사전에 같은 OS에서 실행 파일을 준비합니다.
- 뷰어는 저장된 예제를 탐색하는 화면입니다. 소스 편집이나 실시간 재검사는 터미널에서 수행합니다.
- 위반 예제의 종료 코드 1은 예상된 결과입니다. 그 단계에 실행 Trace가 없어야 합니다.
- 최종 리허설에서는 시간 측정, 글자 가독성, 발표 5회 연속 성공, 오프라인 복구까지 별도로 확인합니다.

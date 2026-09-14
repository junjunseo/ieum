# 의존 그래프 DOT 출력

## 목적

검사기가 판단한 모듈 의존 관계와 계층 관계를 같은 근거로 시각화합니다. DOT 파일 생성에는 외부 도구가 필요하지 않으며, 이미지 렌더링에만 선택적으로 Graphviz를 사용합니다.

## 사용법

```powershell
.\build\ieum.exe .\examples\valid.ieum --emit-dot .\build\valid.dot
.\build\ieum.exe .\examples\execution.ieum --run service.main --emit-dot .\build\execution.dot
```

`--run`과 `--emit-dot`의 순서는 바꿀 수 있습니다. 구조 위반이 있는 입력도 그래프를 먼저 저장하고 기존과 동일하게 종료 코드 `1`을 반환합니다. 출력 파일을 열 수 없는 경우 `graph_export=failed` 경고를 남기되 구조 검사 성공은 `0`, 구조 위반은 `1`로 유지합니다.

## SVG와 오프라인 뷰어

`docs/graphs/index.html`을 브라우저에서 열면 대표 예제 6종과 정상→위반→수정 데모 3종을 볼 수 있습니다. SVG·소스·실제 검사 진단을 HTML 안에 포함하므로 보기에는 서버, 인터넷, Python, Graphviz가 필요 없습니다.

예제 전환, 계층 표시, 위반 경로 강조, 확대·축소·맞춤과 원본 SVG 저장을 지원합니다. 화면 표시 옵션은 검사 결과나 저장하는 원본 SVG를 바꾸지 않습니다. 같은 폴더에는 개별 DOT·SVG 파일도 있습니다.

브라우저가 다운로드를 지원하지 않으면 `SVG 열기` 또는 같은 폴더의 개별 SVG를 사용합니다. HTML 하나만 복사해도 탐색·SVG 저장은 동작하며, 개별 SVG 열기와 시연 가이드 링크는 원래 폴더 구조가 필요합니다.

소스 변경 후 재생성에는 빌드된 Ieum, Python 3.9 이상과 [Graphviz의 공식 배포본](https://graphviz.org/download/)이 필요합니다. Graphviz는 DOT를 SVG로 렌더링할 때만 사용합니다.

```powershell
python scripts/render_graphs.py
# Graphviz가 PATH에 없다면 실행 파일 경로 지정
python scripts/render_graphs.py --dot C:/tools/Graphviz/bin/dot.exe
# 다른 빌드 경로도 지정 가능
python scripts/render_graphs.py --ieum build/Release/ieum.exe --dot C:/tools/Graphviz/bin/dot.exe
```

렌더러는 `evaluation/manifest.json`의 그래프 항목을 실제 검사기에 입력합니다. 기대한 위반 종류·건수와 DOT 저장 성공을 확인한 뒤 SVG를 만들고, 모든 예제가 성공했을 때 결과를 복사합니다. Graphviz가 없어도 기존 뷰어를 덮어쓰지 않습니다. 그래프 처리 실패는 스크립트 종료 코드 2로 보고합니다.

`provenance.json`에는 Graphviz 버전, 생성에 사용한 실행 파일 해시와 입력·DOT·SVG 해시가 있습니다. 텍스트 해시는 BOM·줄바꿈을 정규화합니다. SVG 배치는 Graphviz 버전·폰트·OS에 따라 달라질 수 있으므로 기존 DOT 스냅샷을 결정성 기준으로 유지합니다.

검증: `python test/test_workflows.py`. Graphviz 없이도 실제 검사기가 생성한 DOT와 저장된 결과, 입력·SVG 해시를 대조합니다. 소스/manifest/뷰어 템플릿을 바꾸면 재생성합니다.

시연 절차는 [데모 가이드](DEMO.md), 규모·가독성 기준은 [평가 계획](EVALUATION_PLAN.md)에 있습니다.

## 표현 규칙

| 대상 | DOT 표현 |
|---|---|
| 선언된 모듈 | 둥근 사각형 노드 |
| 정상 `depends` | 파란 실선 화살표, 의존하는 모듈에서 의존 대상으로 향함 |
| `layer U above L` | `U`에서 `L`로 향하는 회색 점선 빈 화살표 |
| 순환·계층 위반 경로 | 빨간 굵은 의존 화살표 |
| 미선언 참조 | `(undefined)`가 붙은 빨간 점선 노드 |
| 중복 모듈 | `(duplicate)`가 붙은 빨간 노드 |
| 잘못된 계층 선언 | 빨간 굵은 점선 화살표 |

노드, 의존 간선, 계층 간선은 각각 이름순으로 정렬됩니다. 중복 모듈의 의존 관계는 검사기와 마찬가지로 첫 선언을 기준으로 생성합니다.

## 위반 경로 일치

검사 결과의 `Violation::path`를 텍스트 진단과 그래프 강조가 함께 사용합니다. 순환 경로는 시작 모듈을 마지막에 한 번 더 포함하며, 간접 계층 위반은 실제 의존 경로 전체를 진단에 출력합니다.

예를 들어 `data -> helper -> ui` 경로가 계층을 역행하면 진단과 DOT 모두 `data -> helper`, `helper -> ui` 두 간선을 같은 위반 경로로 표시합니다.

## 스냅샷 검증

`valid`, `implicit_dependency`, `cyclic_dependency`, `layer_violation`, `transitive_layer_violation`, `invalid_declarations` 예제의 기준 DOT은 `test/snapshots/`에 저장합니다. 전체 테스트는 새 출력과 기준 파일을 바이트 단위로 비교하며, 그래프 저장 실패가 검사 종료 코드를 바꾸지 않는지도 확인합니다.

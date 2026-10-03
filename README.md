# 이음(Ieum)

구조적으로 잘못된 모듈 의존 관계와 이름·호출 오류를 실행 전에 거부하고, 검증된 함수 호출을 실행할 수 있는 작은 프로그래밍 언어입니다.

공식 PoC 릴리스는 [`v0.1.0`](https://github.com/junjunseo/ieum/releases/tag/v0.1.0)이며 저장소 루트의 `VERSION`을 빌드 경로 전체가 공통으로 사용합니다.

현재 구현은 선언의 유효성과 다음 구조 규칙을 검사합니다.

1. 같은 이름의 모듈 중복 선언 금지
2. 계층 선언에서 존재하지 않는 모듈 참조 금지
3. 자기 자신을 상하 계층으로 선언하는 관계 금지
4. 선언되지 않은 모듈에 대한 의존 금지
5. 모듈 사이의 순환 의존 금지
6. 하위 계층에서 직접 또는 의존 경로를 통해 상위 계층으로 향하는 의존 금지

모듈 본문에는 이름·Scope 규칙도 적용합니다.

1. 모듈 변수·함수는 각 이름 공간에서, 매개변수·지역 변수는 같은 Scope에서 중복 선언 금지
2. 선언되지 않은 변수를 호출 인자로 사용하는 행위 금지
3. 현재 모듈 또는 직접 `depends` 모듈에 없는 함수 호출 금지
4. 여러 의존 모듈에 같은 이름의 함수가 있는 모호한 호출 금지
5. 인자·반환값 타입/개수 불일치, 반환 누락과 반복문 밖 break/continue 금지

## 문법

```text
module <모듈 이름>
module <모듈 이름> depends <의존 대상>, <의존 대상>
module <모듈 이름> [depends <의존 대상>] {
  let <모듈 변수>
  fn <함수 이름>(<매개변수>) {
    let <지역 변수>
    call <함수 이름>(<인자>)
  }
}
layer <상위 계층> above <하위 계층>
```

`int`(64비트 정수), `bool`, UTF-8 `string`, `unit` 값과 표현식·변수 초기화·재대입을 지원합니다. 초기화 없는 기존 `let name`과 타입 없는 매개변수는 `unit`을 유지합니다. 타입이 다른 대입·연산·호출 인자는 실행 전에 거부하고, 정수 범위 초과와 0 나눗셈은 위치를 포함한 실행 오류로 보고합니다. 전체 EBNF와 의미 규칙은 [문법 문서](docs/GRAMMAR.md)를 참고합니다.

값 계산 예제:

```text
module app {
  fn main() {
    let result: int = 1 + 2 * 3
    let greeting = "안녕" + " 이음"
    let safe = false && (1 / 0 == 0)
    result = result + 1
  }
}
```

`--run app.main`으로 실행하면 호출 Trace 다음에 진입 함수의 최종 지역 값이 표시됩니다. 위 예제의 `result`는 8, `greeting`은 `"안녕 이음"`, `safe`는 false입니다. [값 실행 예제](examples/values.ieum)는 `depends` 모듈의 함수에 정수를 전달하고 모듈 변수에 합계를 저장합니다.

```powershell
.\build\ieum.exe .\examples\values.ieum --run app.main
```

이 예제의 최종 `data.total`은 17입니다. 함수 반환 타입(`-> int` 등), `return`, 호출식과 리터럴 인자, 중첩 블록, `if`/`else`, `while`, `for`, `break`/`continue`, 유한 재귀도 지원합니다. non-unit 함수는 매개변수/반환 타입을 명시해야 합니다.

[제어 흐름 예제](examples/control_flow.ieum)는 1~10 합계 55, 재귀 팩토리얼 120, 홀수 합계 25를 계산하고 175를 반환합니다.

```powershell
.\build\ieum.exe .\examples\control_flow.ieum --run app.main --max-steps 10000 --max-call-depth 20
```

기본 실행 한도는 내부 단계 100,000회와 호출 깊이 1,024이며 무한 실행은 한도 오류로 종료합니다. 함수 재귀는 허용하지만 모듈 의존 순환은 계속 금지합니다. 리스트·레코드와 기본 입출력을 지원하며, 여러 파일 로딩은 아직 지원하지 않습니다.

`for`는 초기화·조건·증감을 괄호 안에 작성합니다. `continue`는 증감 후 조건 검사로 돌아가며, 초기화에서 선언한 변수는 반복문 밖에 남지 않습니다.

```text
let sum = 0
for (let i = 1; i <= 10; i = i + 1) {
  sum = sum + i
}
```

[for 실행 예제](examples/for_loop.ieum)는 `--run app.main`으로 실행하면 55를 반환합니다. 세 부분을 각각 생략할 수 있고 `for (;;) { ... }`도 실행 한도의 적용을 받습니다.

`list<int>`와 명명 `record`는 값 복사 의미로 동작합니다. [자료구조 예제](examples/collections.ieum)는 `[10,20,30]`의 합계 60·개수 3을 계산합니다. [파일 합산 예제](examples/collections_io.ieum)는 `std_io`·`std_text`·`std_list`를 depends로 선언하고 읽기→분리→정수 변환→합산→파일 쓰기를 수행합니다.

```powershell
.\build\ieum.exe .\examples\collections.ieum --run app.main
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$inputPath = (Resolve-Path .\test\fixtures\numbers.txt).Path
$outputPath = Join-Path $env:TEMP "ieum-sum.txt"
@($inputPath, $outputPath) | .\build\ieum.exe .\examples\collections_io.ieum --run app.main
```

파일 예제는 지정한 출력 파일을 생성하거나 덮어쓰며 결과는 `60`입니다. 한글 경로를 표준 입력으로 전달할 때는 PowerShell의 파이프 인코딩을 UTF-8로 설정해야 합니다. 함수·타입 표와 복사/인덱스 규칙은 [문법 가이드](docs/GRAMMAR.md)를 참고합니다.

정상적인 구조의 예:

```text
module data
module service depends data
module ui depends service

layer ui above service
layer service above data
```

`layer ui above service`가 선언되면 하위 계층인 `service`가 상위 계층인 `ui`에 의존할 수 없습니다.
또한 `ui above service`, `service above data`처럼 계층이 이어져 있으면 `data`도 `ui`에 의존할 수 없습니다.

## 빌드

필요한 도구:

- C++17을 지원하는 `g++`
- 선택 사항: GNU Make 또는 CMake 3.16 이상

Windows PowerShell에서는 다음 명령으로 빌드합니다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

GNU Make를 사용할 수 있다면 다음 명령도 지원합니다.

```sh
make
```

CMake를 사용할 수 있다면 다음 명령으로도 같은 프로그램과 테스트 실행 파일을 만들 수 있습니다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## 실행

버전 확인:

```powershell
.\build\ieum.exe --version
```

예상 출력은 `ieum 0.1.0`입니다.

구조 선언 정상 예제:

```powershell
.\build\ieum.exe .\examples\valid.ieum
```

출력:

```text
── 파싱 결과 ──
모듈 3개, 계층 선언 2개 (modules=3, layers=2)

✓ 구조 검사 통과: 위반 없음
✓ 의미 검사 통과: 위반 없음
```

모듈 본문 예제:

```powershell
.\build\ieum.exe .\examples\module_body.ieum
```

이 예제는 모듈 본문, 변수, 함수, 매개변수와 호출을 AST로 만들고 구조·의미 규칙을 함께 검사합니다.

함수 호출 실행 예제:

```powershell
.\build\ieum.exe .\examples\execution.ieum --run service.main
```

`--run` 진입점은 `<모듈>.<함수>` 형식이며 매개변수가 없어야 합니다. 실행기는 `enter`, `call`, `exit` Trace와 실행한 함수·호출 수를 출력합니다.

의존 그래프 DOT 내보내기:

```powershell
.\build\ieum.exe .\examples\cyclic_dependency.ieum --emit-dot .\build\cyclic.dot
```

검사 성공 여부와 관계없이 파싱된 모듈·의존·계층 관계를 DOT 파일로 저장합니다. 정상 의존은 파란 실선, 계층은 회색 점선, 순환·계층 위반 경로와 잘못된 선언은 빨간색으로 표시합니다. 출력 순서는 고정되므로 스냅샷 비교에 사용할 수 있습니다. Graphviz가 설치되어 있다면 `dot -Tsvg .\build\cyclic.dot -o .\build\cyclic.svg`로 렌더링할 수 있습니다. 그래프 저장 실패는 경고를 출력하지만 구조 검사 종료 코드는 바꾸지 않습니다. 자세한 형식은 [그래프 출력 문서](docs/GRAPH_EXPORT.md)를 참고합니다.

위반 예제:

```powershell
.\build\ieum.exe .\examples\implicit_dependency.ieum
.\build\ieum.exe .\examples\cyclic_dependency.ieum
.\build\ieum.exe .\examples\layer_violation.ieum
.\build\ieum.exe .\examples\transitive_layer_violation.ieum
.\build\ieum.exe .\examples\invalid_declarations.ieum
.\build\ieum.exe .\examples\semantic_undefined_function.ieum
.\build\ieum.exe .\examples\semantic_arity_mismatch.ieum
.\build\ieum.exe .\examples\semantic_missing_dependency.ieum
.\build\ieum.exe .\examples\semantic_undefined_variable.ieum
```

구조 또는 의미 위반이 발견되면 오류 내용과 행을 출력하고 종료 코드 `1`을 반환합니다. 잘못된 CLI 사용과 파일 열기 실패는 종료 코드 `2`를 반환하므로 빌드 스크립트나 CI에서도 실패를 구분할 수 있습니다.

## 그래프 탐색과 데모

[`docs/graphs/index.html`](docs/graphs/index.html)을 내려받은 저장소에서 브라우저로 열면 대표 예제 6종과 정상→위반→수정 데모 3종을 탐색할 수 있습니다. 그래프·코드·실제 진단을 함께 보여주며, 계층 표시, 위반 강조, 확대·축소와 SVG 저장을 지원합니다. 저장된 뷰어는 서버나 인터넷, Graphviz 없이 동작합니다.

- [3분 데모 동선과 복구 절차](docs/DEMO.md)
- [SVG·뷰어 재생성](docs/GRAPH_EXPORT.md#svg와-오프라인-뷰어)
- [평가 입력과 정확도·성능·가독성 기준](docs/EVALUATION_PLAN.md)

빌드 후 Python 3.9 이상에서 초기 평가 입력과 데모를 검증합니다. 추가 Python 패키지는 필요 없습니다.

```powershell
python scripts/evaluate.py
python scripts/demo.py --repeat 5
```

평가는 직접 작성한 입력 15개의 기대 결과를 검사합니다. 최종 실제 프로젝트 정확도로 해석하지 않습니다. 로그는 `build/evaluation/results.json`, `build/demo/results.json`에 저장됩니다.

실제 프로젝트에서 추출한 구조의 평가는 별도 manifest로 실행합니다.

```powershell
python scripts/prepare_corpus.py --check
python scripts/evaluate.py --manifest evaluation/real/manifest.json --output build/evaluation/real.json
```

고정 커밋의 itsdangerous·packaging 전체 패키지에서 정적 import를 추출한 원본 2개와 변형·정책 입력 8개, 25·50 모듈 합성 입력 4개를 구분해 집계합니다. 14개 모두 기대 결과와 일치했으나 사람의 독립 정답 검토는 대기 중이며 Python 런타임 오류율이나 일반적인 정확도를 의미하지 않습니다. [출처·추출 규칙·재현 방법](docs/REAL_CORPUS.md), [평가 결과](docs/EVALUATION_RESULTS.md), [평가 그래프 뷰어](docs/graphs/evaluation/index.html)를 참고합니다.

## 테스트

Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test.ps1
```

GNU Make:

```sh
make test
```

CMake/CTest:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

테스트 범위:

- 파서 단위 테스트 46개
- Lexer → Parser → Checker 통합 테스트 35개
- 구조 검사기 시나리오 테스트 35개
- 의존 그래프 생성기 테스트 14개
- 의미 분석기 테스트 26개
- 최소 인터프리터 테스트 15개
- 총 171개 assert 기반 자동 테스트
- CLI 버전이 `VERSION`과 일치하는지 확인하는 테스트
- 정상 예제 3개의 종료 코드 `0`과 위반 예제 9개의 종료 코드 `1`을 확인하는 smoke 테스트
- 잘못된 실행 진입점 형식, 미정의 진입 함수와 매개변수가 있는 진입 함수의 CLI 종료 코드 검증
- 예제별 대표 성공·위반 진단이 출력되는지 확인하는 테스트
- 2개 모듈 합성 corpus로 성능 측정 경로를 확인하는 benchmark smoke 테스트
- 대표 예제 6종의 결정적 DOT 스냅샷과 그래프 저장 실패 격리 테스트
- Python이 있으면 평가 집계·실패 처리·저장된 그래프 일치 검증 8개, 초기 평가 입력 15개와 데모 재생도 확인
- PowerShell·CTest의 Python 경로는 실제 corpus 추출·독립 참조 계산·파일 벤치마크·SVG 일치 검증 14개를 추가 실행합니다(총 Python 테스트 22개).
- 정상 구조, 선언 오류, 주석·공백·BOM 입력, 미선언 의존, 직접·다단계·자기·복수 순환, 계층 위반, Scope, 함수 해석, 인자 개수, 재귀와 실행 Trace 검증

## 성능 기준선

구조 검사 성능은 결정적으로 생성되는 계층형 합성 corpus를 `Lexer -> Parser -> Checker` 전체 파이프라인에 입력해 측정합니다.

Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\benchmark.ps1
```

GNU Make:

```sh
make benchmark BENCHMARK_MODULES=200 BENCHMARK_ITERATIONS=11
```

측정 환경, 입력 구성, v0.1.0 결과와 해석은 [성능 기준선](docs/PERFORMANCE_BASELINE.md)에 기록합니다. 벤치마크는 하드웨어 독립적인 합격 시간을 강제하지 않으며, 같은 환경에서 기능 확장 전후의 중앙값을 비교하는 용도입니다.

## CI

GitHub Actions는 `ubuntu-latest`와 `windows-latest`에서 CMake configure, build, CTest를 실행합니다. CTest는 위의 171개 assert 기반 자동 테스트, CLI 버전 테스트, 12개 예제 smoke 테스트, 실행 진입점 경계 조건, 6개 DOT 스냅샷과 benchmark smoke 테스트를 함께 검증합니다.

새 환경에서 재현할 때는 다음 순서를 기준으로 확인합니다.

1. `g++`, GNU Make 또는 CMake 설치 여부를 확인합니다.
2. `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test.ps1` 또는 `ctest --test-dir build -C Release --output-on-failure`를 실행합니다.
3. README의 시연 흐름과 예제가 문서와 같은 종료 코드로 동작하는지 확인합니다.

## v0.1.0 릴리스

- `v0.1.0`은 2026년 8월 10일에 확정한 이음 구조 검사 코어의 첫 공식 PoC 릴리스입니다.
- [변경 기록](CHANGELOG.md)에서 포함 기능과 알려진 제한을 확인할 수 있습니다.
- [릴리스 체크리스트](docs/RELEASE_CHECKLIST.md)에서 새 환경 재현, 예제 결과, CI와 게시 절차를 확인할 수 있습니다.
- GitHub Release는 소스 코드 ZIP/TAR.GZ를 제공하며, 실행 파일은 빌드 절차를 따라 직접 생성합니다.

## 프로젝트 구조

```text
.github/workflows/  GitHub Actions CI
cmake/              CTest 예제 결과 검증 스크립트
docs/               릴리스 체크리스트
benchmark/          구조 검사 성능 기준선 실행 파일 소스
src/
  lexer.h       토큰 생성
  parser.h      구조 선언과 모듈 본문을 AST로 변환
  ast.h         모듈·계층·변수·함수·호출 자료구조
  checker.h     의존성과 계층 규칙 검사
  graph.h       결정적 DOT 그래프와 위반 경로 강조
  semantic.h    이름·Scope·함수 호출 분석
  interpreter.h 검증된 unit 함수 호출 실행과 Trace
  version.h     빌드 시스템이 전달한 버전 노출
  main.cpp      명령행 프로그램
test/           자동 테스트
examples/       구조·모듈 본문 정상 및 위반 시연 파일
scripts/        Windows 빌드·테스트 스크립트
CMakeLists.txt  CMake 빌드·CTest 정의
VERSION         빌드 경로가 공유하는 PoC 버전
CHANGELOG.md    버전별 기능·제한 기록
기획안/         프로젝트 기획 문서
```

## 현재 범위

F3까지 구현되어 모듈 선언, 의존 선언, 계층 선언과 함께 변수, 함수, 매개변수, 지역 변수와 호출 문장을 검사하고 실행하며 의존 그래프를 DOT으로 내보냅니다. 함수 이름은 현재 모듈을 우선한 뒤 직접 `depends` 모듈에서 해석하며, 인자는 매개변수·앞서 선언한 지역 변수·모듈 변수 중 하나여야 합니다. 실행 값은 아직 unit뿐이며 리터럴, 연산, 반환문과 제어 흐름은 지원하지 않습니다.

## 2학기 로드맵

2학기에는 구조 검사 코어의 회귀를 막으면서 다음 순서로 확장합니다.

1. F1: 모듈 본문과 변수·함수·호출 AST
2. F2: 이름 해석, Scope와 최소 인터프리터
3. F3: 의존 그래프와 위반 경로 시각화
4. F4: 실제 corpus 기반 정확도·성능 평가
5. F5: 3분 전시 데모와 발표 자료
6. F6: v1.0.0 최종 QA, 보고서와 사용 가이드

기간, GitHub 이슈와 완료 조건은 [2학기 백로그](docs/SEMESTER2_BACKLOG.md)에서 관리하고, 주간 진행 상황은 [`docs/status/`](docs/status/)에 기록합니다.

## 시연 흐름

1. `examples/valid.ieum`으로 올바른 구조가 통과함을 보입니다.
2. `examples/module_body.ieum`으로 변수·함수·호출 AST와 의미 검사를 보입니다.
3. `examples/execution.ieum --run service.main`으로 모듈 간 함수 호출 Trace를 보입니다.
4. `examples/implicit_dependency.ieum`과 `examples/cyclic_dependency.ieum`으로 잘못된 의존을 거부함을 보입니다.
5. `examples/layer_violation.ieum`과 `examples/transitive_layer_violation.ieum`으로 직접·전이 계층 위반을 거부함을 보입니다.
6. `examples/invalid_declarations.ieum`으로 잘못된 구조 선언을 거부함을 보입니다.
7. `examples/semantic_missing_dependency.ieum` 등 의미 위반 예제로 호출·Scope 오류를 거부함을 보입니다.

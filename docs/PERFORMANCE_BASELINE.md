# v0.1.0 성능 기준선

## 목적

이 문서는 S7 시점의 구조 검사 성능을 같은 입력과 명령으로 다시 측정하기 위한 기준선입니다. 절대적인 성능 보장을 뜻하지 않으며, 이후 문법·의미 분석 확장에 따른 회귀를 비교하는 출발점으로 사용합니다.

## 측정 대상

- 기준 버전: `v0.1.0` (`1d29dc7`)
- 측정 경로: `Lexer -> Parser -> Checker` 전체 파이프라인
- 시나리오: `layered-valid-chain`
- 입력 구성:
  - `module_0`부터 `module_N-1`까지 N개 모듈 생성
  - 각 모듈은 바로 아래 모듈 하나에 의존
  - 인접 모듈마다 `layer module_i above module_i+1` 선언
  - 구조 위반이 없는 결정적 합성 corpus
- 측정 방식: 준비된 소스를 대상으로 1회 워밍업 후 11회 반복, 벽시계 시간 기록
- 포함 범위: 토큰화, 파싱, AST 생성, 구조 검사
- 제외 범위: corpus 문자열 생성과 프로세스 시작 시간

## 측정 환경

- 측정일: 2026-08-24
- OS: Microsoft Windows 10.0.26200, x64
- CPU 식별자: Intel64 Family 6 Model 140, 논리 프로세서 8개
- 컴파일러: MSYS2 `g++ 15.2.0`
- 컴파일 옵션: `-std=c++17 -O2 -DNDEBUG -Wall -Wextra -pedantic`

## 기준 결과

| 모듈 | 계층 | 소스 크기(byte) | 최소(ms) | 중앙값(ms) | p95(ms) | 중앙값 처리량(module/s) |
|---:|---:|---:|---:|---:|---:|---:|
| 50 | 49 | 3,262 | 10.834 | 11.174 | 12.999 | 4,474.753 |
| 100 | 99 | 6,612 | 80.237 | 93.253 | 113.473 | 1,072.354 |
| 200 | 199 | 13,711 | 643.846 | 692.571 | 1,618.546 | 288.779 |

측정값은 실행 중인 프로세스와 전원 정책의 영향을 받을 수 있습니다. 따라서 서로 다른 컴퓨터의 절대 시간을 직접 비교하지 않고, 동일 환경에서 같은 명령을 반복했을 때의 중앙값 비율을 회귀 판단에 사용합니다.

## 해석과 후속 기준

2026-09-29에 같은 50·100·200 모듈과 11회 조건으로 재측정했습니다. 파일 입력 벤치마크와 함께 기록한 [원시 결과](../evaluation/results/2026-09-29-performance.json), [비교표와 한계](EVALUATION_RESULTS.md)를 참고합니다. 검사기 최적화를 수행하지 않았으므로 관측된 시간 차이를 최적화 효과로 해석하지 않습니다.

- 모듈 수가 50개에서 100개로 두 배가 될 때 중앙값은 약 8.3배 증가했습니다.
- 100개에서 200개로 두 배가 될 때 중앙값은 약 7.4배 증가했습니다.
- 현재 Checker는 계층 후보마다 상위 관계와 의존 경로를 반복 탐색하므로 긴 계층 체인에서 비용이 빠르게 증가합니다.
- S7에서는 현상을 기록하고 재현 명령을 고정합니다. 최적화와 다양한 실제 corpus 평가는 F4 이슈 [#21](https://github.com/junjunseo/ieum/issues/21)에서 수행합니다.
- 하드웨어 독립적인 통과 시간은 아직 두지 않습니다. 기능 회귀는 테스트로 차단하고, 성능은 동일 환경의 이전 중앙값과 비교합니다.

## 재측정 방법

Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\benchmark.ps1 `
  -ModuleCounts 50,100,200 `
  -Iterations 11
```

GNU Make:

```sh
make benchmark BENCHMARK_MODULES=200 BENCHMARK_ITERATIONS=11
```

CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target benchmarkChecker
./build/benchmarkChecker 200 11
```

Visual Studio 계열 다중 구성 생성기에서는 실행 파일이 `build/Release/benchmarkChecker.exe`에 생성될 수 있습니다.

출력에는 시나리오, 모듈·계층 수, 입력 크기, 반복 횟수, 최소·중앙값·p95·최대 시간과 중앙값 처리량이 포함됩니다.

## 언어 확장판 측정

L5에서는 확장 전 커밋 29b6348과 L4 병합 커밋 bbcc146 기반의 L5 작업 트리를 같은
Windows 11 환경(Intel64 Family 6 Model 140, 논리 CPU 8개), g++ 15.2.0,
-std=c++17 -O2 -DNDEBUG -Wall -Wextra -pedantic으로 각각 빌드해 재측정했습니다.
입력당 워밍업 1회와 표본 11개입니다. JSON의 파일 해시로 측정한 구현을 구분합니다.

- [확장 전 구조 원시 결과](../evaluation/results/language-expansion-structure-before.json)
- [확장 후 구조 원시 결과](../evaluation/results/language-expansion-structure-after.json)
- [실행 전용 원시 결과](../evaluation/results/language-expansion-runtime.json)

### 구조 검사 비교

구조 측정에는 Lexer → Parser → Checker와 결과 해제·개수 확인이 포함됩니다.
파일 읽기, 의미 검사, 실행기, DOT 출력과 CLI 시작 시간은 제외합니다.
확장 전 프로그램도 처리할 수 있는 동일한 corpus만 비교합니다.

| 모듈 수 | 확장 전 중앙값(ms) | 확장 후 중앙값(ms) | 후/전 |
| ---: | ---: | ---: | ---: |
| 50 | 7.9353 | 9.2518 | 1.166 |
| 100 | 62.8947 | 62.1862 | 0.989 |
| 200 | 496.7721 | 509.4749 | 1.026 |

긴 계층 체인의 비용 증가 경향은 남아 있습니다. 이번 QA에서 구조 검사기를 최적화하지
않았습니다. 작은 corpus까지 포함한 개별 측정은 다음과 같습니다.

| 입력 | 확장 전 중앙값(ms) | 확장 후 중앙값(ms) | 후/전 |
| --- | ---: | ---: | ---: |
| itsdangerous_original | 0.0378 | 0.0333 | 0.881 |
| itsdangerous_undefined | 0.0349 | 0.0484 | 1.387 |
| itsdangerous_self_cycle | 0.0317 | 0.0374 | 1.180 |
| itsdangerous_policy_forward | 0.0461 | 0.0468 | 1.015 |
| itsdangerous_policy_reverse | 0.0413 | 0.0396 | 0.959 |
| packaging_original | 0.1516 | 0.1506 | 0.993 |
| packaging_undefined | 0.1274 | 0.1493 | 1.172 |
| packaging_self_cycle | 0.1239 | 0.1332 | 1.075 |
| packaging_policy_forward | 0.1476 | 0.2846 | 1.928 |
| packaging_policy_reverse | 0.1503 | 0.1834 | 1.220 |
| medium_25_valid | 0.3099 | 0.3479 | 1.123 |
| medium_25_cycle | 0.2968 | 0.3438 | 1.158 |
| medium_50_valid | 0.9576 | 0.9210 | 0.962 |
| medium_50_cycle | 0.9309 | 0.9644 | 1.036 |

일부 작은 입력의 중앙값 증가가 관측되었습니다. 순차 실행 한 번의 11개 표본이며
순서를 무작위화한 통계 실험은 아닙니다. 시스템 부하·전원 상태·측정 순서 영향을
분리하지 않았으므로 이 수치만으로 최적화 효과나 확정적인 회귀율을 주장하지 않습니다.
하드웨어 독립적인 성능 합격 기준도 적용하지 않았습니다.

### 실행 전용 기준선

준비된 AST와 의미 분석 결과를 재사용하며 Interpreter::run만 측정합니다.
호출 프레임·모듈 초기화·Trace/결과 할당은 포함하고 파싱·정적 검사·입출력·CLI 시작/
출력·결과 정답 확인·결과 해제는 제외합니다. 각 실행은 새 Machine과 전역 저장 공간을
만들며 모든 표본의 반환값과 실행 단계 수를 확인합니다.
확장 전에는 이 기능이 없으므로 과거 구조 검사 시간과 비율을 계산하지 않습니다.

| 시나리오 | 크기 | 기대 반환값 | 중앙값(ms) | p95(ms) |
| --- | ---: | ---: | ---: | ---: |
| loop | 100 | 5050 | 0.1715 | 0.1789 |
| loop | 1000 | 500500 | 1.5408 | 2.0550 |
| loop | 10000 | 50005000 | 18.0286 | 18.9792 |
| recursion | 20 | 20 | 0.1173 | 0.1344 |
| recursion | 200 | 200 | 0.7504 | 1.0959 |
| collections | 100 | 6000 | 1.3467 | 1.7770 |
| collections | 1000 | 60000 | 14.0476 | 14.8689 |

loop는 1부터 N의 합, recursion은 N단계 카운트, collections는 매 반복 레코드/목록
복사본을 갱신하고 원본 [10,20,30]을 합산합니다. 크기에 따른 관측치를 비교하는 최초
실행 기준선이며 Python/C++ 등 다른 언어와의 성능 비교 자료는 아닙니다.
실행 벤치마크의 내부 한도는 5,000,000단계/호출 깊이 1,024로 고정됩니다.

CMake Release 빌드 후 다음과 같이 재현합니다. Windows 다중 구성 빌드는
build/Release/benchmarkChecker.exe 및 benchmarkRuntime.exe로 경로를 바꿉니다.

    python scripts/benchmark_corpus.py --benchmark build/benchmarkChecker --iterations 11 --include-baseline --build-label "CMake Release"
    python scripts/benchmark_runtime.py --benchmark build/benchmarkRuntime --iterations 11 --build-label "CMake Release"

PowerShell scripts/test.ps1도 두 벤치마크를 -O2 -DNDEBUG로 빌드합니다.
원시 결과의 환경·옵션·표본·해시를 함께 보존하고 같은 범위끼리 비교해야 합니다.

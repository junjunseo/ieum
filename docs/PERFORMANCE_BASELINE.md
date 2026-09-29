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

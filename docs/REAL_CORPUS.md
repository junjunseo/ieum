# 실제 프로젝트 평가 입력과 검토 규칙

관련: F4 [#21](https://github.com/junjunseo/ieum/issues/21), 주간 작업 [#30](https://github.com/junjunseo/ieum/issues/30).

## 출처와 범위

2026-09-29에 아래 두 공개 프로젝트의 커밋을 고정했습니다. 임의의 파일 부분집합이 아니라 지정한 패키지 디렉터리의 모든 `.py` 파일을 열거합니다. 원본 코드를 실행하거나 설치하지 않습니다.

| 프로젝트 | 고정 커밋 | 추출 범위 | 모듈 / 의존 | 라이선스 고지 |
|---|---|---|---:|---|
| itsdangerous | [`672971d`](https://github.com/pallets/itsdangerous/tree/672971d66a2ef9f85151e53283113f33d642dabd) | `src/itsdangerous/**/*.py` | 8 / 21 | 원본 `LICENSE.txt` (BSD-3-Clause) |
| packaging | [`7b898d9`](https://github.com/pypa/packaging/tree/7b898d9f0b343ca06993157fc328d7caad51d5c2) | `src/packaging/**/*.py` | 22 / 39 | 원본 `LICENSE`, `LICENSE.BSD`, `LICENSE.APACHE` (BSD-2-Clause 또는 Apache-2.0) |

[sources.json](../evaluation/real/sources.json)에 저장소·전체 SHA·패키지 경로·ZIP SHA-256을 기록했습니다. [itsdangerous 스냅샷](../evaluation/real/snapshots/itsdangerous.json)과 [packaging 스냅샷](../evaluation/real/snapshots/packaging.json)에 파일별 해시, Python ↔ Ieum 모듈 대응, import 문장·행·대상, 라이선스 전문을 보존합니다. 본문 구현 전체는 저장소에 복사하지 않습니다. 원본 ZIP은 버전 관리에서 제외하는 `build/upstream/`에 둡니다.

## 추출 규칙 `all-static-imports-v1`

1. `.py` 파일 하나를 모듈 하나로 봅니다. `__init__.py`는 해당 패키지 모듈입니다. 예를 들어 `packaging/licenses/__init__.py`는 `packaging.licenses`, Ieum에서는 `packaging__licenses`입니다. 점을 `__`로 바꾸며 이름 충돌은 오류로 처리합니다.
2. Python AST의 `import`·`from ... import ...`를 순회합니다. 주석·문자열 안의 예시와 동적 `importlib` 호출은 import 간선으로 세지 않습니다.
3. 상대 import는 패키지 문맥으로 해석합니다. `from . import ranges`는 파일이 존재하면 자식 모듈을 가리킵니다. `from .signer import Signer`처럼 자식 모듈이 아닌 심볼은 `signer` 모듈을 가리킵니다. 별칭은 대상 모듈을 바꾸지 않습니다.
4. 열거된 내부 모듈과 정확히 일치하는 명시적 대상만 간선으로 채택합니다. 동일 간선은 합치고 자기 import는 보존합니다. 외부 라이브러리·표준 라이브러리·해석되지 않은 대상도 import 기록에는 남지만 간선에서 제외합니다. 이 제외를 Ieum의 미선언 의존 오류로 취급하지 않습니다.
5. 조건문, `TYPE_CHECKING`, 함수 내부의 지연 import도 포함합니다. 패키지 초기화의 암묵적 부수 효과는 추가하지 않습니다. 따라서 이는 정적 구조의 보수적 투영이며 실제 Python 실행 순서나 런타임 import 그래프가 아닙니다.
6. 원본에는 계층 정책을 추정해서 넣지 않습니다. 별도 `real_policy` 사례만 평가용 계층 하나를 명시적으로 추가합니다.

해석 정책 자체가 모델링 선택입니다. 동적 import, 재수출 심볼 추적, 외부 의존, 타입 전용 import와 런타임 import 구분, 테스트 디렉터리, 비 Python 파일은 현재 평가 범위 밖입니다. 패키지 2개로 다른 언어·대규모 서비스 전체를 대표할 수 없습니다.

## 기대 결과와 독립 검토 근거

`scripts/prepare_corpus.py`는 Ieum을 실행하지 않고 간선 집합의 전이 폐쇄를 계산합니다. 상호 도달하는 구성요소가 순환 근거이고, 계층의 하위 노드에서 상위 노드로 도달 가능한 쌍이 계층 위반 근거입니다. 단순 고리마다 DFS 진단 1건임을 사용할 수 있는 사례만 자동 생성합니다. 내부 진입·진출 간선이 노드마다 하나가 아닌 복잡한 순환 구성요소는 임의의 진단 건수를 정하지 않고 오류로 중단합니다.

| 사례 | 기대 근거 |
|---|---|
| itsdangerous 원본 | 8개 모듈의 의존 그래프는 DAG, 최장 경로 7노드. 위반 없음 |
| packaging 원본 | `packaging.ranges` ↔ `packaging.specifiers`의 단순 고리 1개. 순환 진단 1건 |
| `_undefined` | 원본의 사전순 첫 leaf에 `missing_external_module` 간선 1개 추가. 미선언 진단 1건 증가 |
| `_self_cycle` | 같은 leaf에 자기 간선 1개 추가. 순환 진단 1건 증가 |
| `_policy_forward` | 기존 의존 방향과 같은 계층 정책 1개 추가. 원본 진단 유지 |
| `_policy_reverse` | 같은 의존을 거스르는 계층 정책 1개 추가. 계층 위반 1건 증가 |
| `medium_25/50_valid` | 5노드짜리 계층 체인 5개/10개를 최상위 노드끼리 연결한 DAG |
| `medium_25/50_cycle` | 마지막 체인의 leaf → top 간선 1개 추가. 순환 1건과 해당 체인의 하위·상위 10쌍 위반 |

packaging 순환의 원본 근거는 [specifiers.py의 41행·1132행](https://github.com/pypa/packaging/blob/7b898d9f0b343ca06993157fc328d7caad51d5c2/src/packaging/specifiers.py#L41), [ranges.py의 54행·1769행](https://github.com/pypa/packaging/blob/7b898d9f0b343ca06993157fc328d7caad51d5c2/src/packaging/ranges.py#L54)입니다. 타입 검사·지연 import를 포함한 결과로, 원본 프로젝트의 실행 결함이라는 뜻이 아닙니다.

itsdangerous의 정책 대상은 [serializer → signer (10~11행)](https://github.com/pallets/itsdangerous/blob/672971d66a2ef9f85151e53283113f33d642dabd/src/itsdangerous/serializer.py#L10), packaging의 정책 대상은 `_manylinux → _elffile`입니다. 정방향은 호출 모듈을 위에, 역방향은 의존 대상을 위에 놓습니다. 이 정책은 평가용으로 설정했으며 원저자의 설계 규칙이라고 주장하지 않습니다.

[manifest.json](../evaluation/real/manifest.json)에 각 변형 간선, 순환 구성요소, 미선언 간선, 계층 위반 쌍을 모두 기록했습니다. 원본 2개·간선 변형 4개·계층 정책 4개와 합성 4개를 구분하며, 같은 원본의 파생 사례를 독립 표본으로 해석하지 않습니다.

현재 `review_status`는 전부 **pending-human-review**입니다. AI가 만든 별도 참조 계산과 자동 검증을 사람의 독립 정답 검토로 대체하지 않습니다. 검토자는 원본 링크와 스냅샷의 모듈·간선 대응, 변형 한 개의 차이, 예상 구성요소·위반 쌍을 대조한 뒤 검토자·날짜·수정 사항을 남겨야 합니다. 아직 해당 검토를 완료했다고 기록하지 않았습니다.

## 재현

Python 3.9 이상과 빌드된 Ieum만 있으면 저장된 스냅샷으로 오프라인 재현이 가능합니다.

```powershell
python scripts/prepare_corpus.py --check
python scripts/evaluate.py --manifest evaluation/real/manifest.json --output build/evaluation/real.json
```

`--check`를 빼면 스냅샷에서 `.ieum`과 manifest를 다시 만듭니다. 변경된 출처나 기대 결과를 검토 없이 덮어쓰지 않도록 일반 검증에서는 `--check`를 사용합니다.

원본 소스부터 추출을 재검증하려면 고정 ZIP을 내려받습니다(이 단계만 네트워크 필요).

```powershell
New-Item -ItemType Directory -Force build/upstream | Out-Null
$corpusSources = Get-Content evaluation/real/sources.json -Raw | ConvertFrom-Json
foreach ($item in $corpusSources.sources) {
    $archiveUrl = $item.repository.Replace('https://github.com/', 'https://codeload.github.com/') + '/zip/' + $item.commit
    Invoke-WebRequest -Uri $archiveUrl -OutFile ('build/upstream/' + $item.id + '.zip')
}
python scripts/extract_imports.py --check
```

ZIP의 SHA-256이 출처 lock과 다르면 파싱 전에 중단합니다. `extract_imports.py`는 ZIP 내용을 메모리에서 읽으며 원본 Python 코드를 import/실행하거나 압축 경로를 디스크에 풀지 않습니다.

성능은 최적화된 벤치마크 실행 파일로 측정합니다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/benchmark.ps1 -ModuleCounts 2 -Iterations 1
python scripts/benchmark_corpus.py --include-baseline --iterations 11 --build-label "g++ -std=c++17 -O2 -DNDEBUG -Wall -Wextra -pedantic -Isrc benchmark/benchmarkChecker.cpp"
```

다른 빌드 경로에는 `--benchmark build/Release/benchmarkChecker.exe` 또는 `--benchmark build/benchmarkChecker`를 지정합니다. 실행 파일 해시와 내장 컴파일러·NDEBUG 정보, 사용자가 제공한 빌드 명령 설명을 기록합니다. 실제 파일 읽기·추가 메타데이터 파싱·프로세스 시작·렌더링은 성능 측정에 포함하지 않습니다. CMake 빌드 옵션은 사용한 설정에 맞춰 `--build-label`을 바꿔 기록합니다.

그래프 재생성은 [그래프 문서](GRAPH_EXPORT.md)를, 수치와 화면 검증의 한계는 [결과 문서](EVALUATION_RESULTS.md)를 참고합니다.

# 언어 확장판 사용 가이드

L1~L4와 for 반복문을 포함한 미출시 소스의 안내입니다. 공식 버전 문자열은 아직 0.1.0입니다.
기존 v0.1.0 태그에 새 기능이 포함된다는 뜻은 아니므로 소스 커밋도 함께 기록합니다.

## 새 checkout에서 빌드와 검증

C++17 컴파일러, CMake 3.16 이상, Python 3.9 이상을 준비합니다.
Python은 언어 실행에 필요하지 않지만 전체 통합 검증에는 필요합니다.

    git clone https://github.com/junjunseo/ieum.git
    cd ieum
    git rev-parse HEAD
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DIEUM_REQUIRE_PYTHON_TESTS=ON
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

Ubuntu 실행 파일은 build/ieum, Visual Studio 생성기는 build/Release/ieum.exe입니다.
Windows에서 g++를 사용하면 PowerShell 5.1 또는 7에서 ./scripts/test.ps1을 실행해
build/ieum.exe를 만들고 검증할 수 있습니다. Python 검사를 건너뛰었다면 전체 QA 통과로
해석하지 않습니다.

## 값부터 여러 파일까지

아래는 실행 파일 뒤에 지정할 인자와 기대 결과입니다.

| 인자 | 확인할 결과 |
| --- | --- |
| examples/values.ieum --run app.main | data.total=17, 지역 result=8, 단락 평가 safe=false |
| examples/control_flow.ieum --run app.main | 합계 55, factorial(5)=120, 반환값 175 |
| examples/for_loop.ieum --run app.main | 합계 55, 홀수 합계 25 |
| examples/collections.ieum --run app.main | 합계 60, 개수 3, 복사본 수정 후 원본 유지 |
| examples/multifile/app.ieum --module-path examples/multifile --run app.main | 출력·반환값 60, Summary의 total=60/count=3 |

파일 입출력 예제 examples/collections_io.ieum은 표준 입력으로 입력 파일 경로와 출력 파일
경로를 각각 한 줄씩 받습니다. test/fixtures/numbers.txt를 사용하면 결과 파일에 60을 씁니다.
출력 파일이 이미 있으면 덮어씁니다. [README의 PowerShell 예제](../README.md)를 참고합니다.

전체 예제와 실패 시 기존 결과 파일 보존을 임시 디렉터리에서 검사하려면:

    python scripts/language_demo.py --ieum build/ieum --repeat 5

Windows에서는 --ieum build/ieum.exe 또는 --ieum build/Release/ieum.exe를 사용합니다.
출력은 build/demo/language.json에 저장됩니다.
기존 구조 위반·복구 데모는 scripts/demo.py로 별도 재생합니다.

## 실행과 진단 규칙

- 검사만 수행하면 함수나 입출력을 실행하지 않습니다. 실행에는 --run module.function을 지정합니다.
- 프로세스 반환 코드 0은 검사/실행 성공, 1은 언어 진단, 2는 사용법·입력 파일 오류입니다.
  함수가 정수 60을 반환해도 프로세스 반환 코드는 0이며 값은 Trace에 표시됩니다.
- 출력 함수의 문자열은 검사 결과·Trace와 같은 표준 출력에 나옵니다.
- 여러 파일은 지정한 --module-path에서 필요한 의존 파일만 로딩합니다.
  옵션 없는 실행은 주변 파일을 자동 탐색하지 않습니다.
- 외부 호출·타입·값 접근에는 직접 depends 선언이 필요합니다. 모듈 순환과 계층 역행은 거부합니다.
- private는 이름 접근 제어이며 파일 시스템 권한이나 Trace 비공개 기능이 아닙니다.
- 기본 한도는 내부 실행 단계 100,000회와 호출 깊이 1,024입니다.
  --max-steps와 --max-call-depth로 조절합니다. 메모리·파일 크기·경과 시간 제한은 아닙니다.
- 오류 위치는 파일·행·UTF-8 바이트 열입니다. 실행 오류에는 호출 스택이 붙습니다.

## 호환성과 제한

기존 module/depends/layer, 초기화 없는 unit let, 타입 없는 unit 매개변수,
call 문법, 단일 파일 실행과 결정적 DOT를 유지합니다.
true/false, return/if/else/while/break/continue/for, record/private는 예약어입니다.
내장 모듈 std_io/std_text/std_list도 사용자 모듈 이름으로 사용할 수 없습니다.

정수는 int64이며 부동소수점이나 암묵적인 문자열 변환은 제공하지 않습니다.
조건은 bool, 재대입은 같은 타입이어야 합니다. 빈 목록에는 타입 문맥이 필요합니다.
목록과 레코드는 값 복사 의미이며 중첩 깊이는 128까지입니다.
문자열 인덱싱, 길이를 변경하는 목록 연산, 공유 참조·순환 객체,
일반 제네릭·클로저·함수 값·상속·동시성·FFI·네이티브 코드 생성은 이번 범위에 없습니다.

전체 문법은 [GRAMMAR](GRAMMAR.md), 성능 측정 범위와 결과는
[성능 기준선](PERFORMANCE_BASELINE.md)을 참고합니다.
자동 데모 통과는 사람의 독립 정답 검토, 사용자 평가, 발표 리허설을 대체하지 않습니다.

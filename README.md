# 이음(Ieum)

구조적으로 잘못된 모듈 의존 관계를 실행 전에 거부하는 작은 프로그래밍 언어와 검사기입니다.

현재 구현은 선언의 유효성과 다음 구조 규칙을 검사합니다.

1. 같은 이름의 모듈 중복 선언 금지
2. 계층 선언에서 존재하지 않는 모듈 참조 금지
3. 자기 자신을 상하 계층으로 선언하는 관계 금지
4. 선언되지 않은 모듈에 대한 의존 금지
5. 모듈 사이의 순환 의존 금지
6. 하위 계층에서 직접 또는 전이적 상위 계층으로 향하는 의존 금지

## 문법

```text
module <모듈 이름>
module <모듈 이름> depends <의존 대상>, <의존 대상>
layer <상위 계층> above <하위 계층>
```

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
- 선택 사항: GNU Make

Windows PowerShell에서는 다음 명령으로 빌드합니다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

GNU Make를 사용할 수 있다면 다음 명령도 지원합니다.

```sh
make
```

## 실행

정상 예제:

```powershell
.\build\ieum.exe .\examples\valid.ieum
```

출력:

```text
── 파싱 결과 ──
모듈 3개, 계층 선언 2개

✓ 구조 검사 통과: 위반 없음
```

위반 예제:

```powershell
.\build\ieum.exe .\examples\implicit_dependency.ieum
.\build\ieum.exe .\examples\cyclic_dependency.ieum
.\build\ieum.exe .\examples\layer_violation.ieum
.\build\ieum.exe .\examples\transitive_layer_violation.ieum
.\build\ieum.exe .\examples\invalid_declarations.ieum
```

구조 위반이 발견되면 오류 내용과 행을 출력하고 종료 코드 `1`을 반환합니다. 따라서 빌드 스크립트나 CI에서도 실패를 감지할 수 있습니다.

## 테스트

Windows PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\test.ps1
```

GNU Make:

```sh
make test
```

테스트 범위:

- 파서 단위 테스트 9개
- Lexer → Parser → Checker 통합 테스트 6개
- 구조 검사기 시나리오 테스트 12개
- 정상 구조, 선언 오류, 미선언 의존, 직접·다단계·자기 순환, 직접·전이적 계층 위반, 복합 위반 검증

## 프로젝트 구조

```text
src/
  lexer.h       토큰 생성
  parser.h      구조 선언을 AST로 변환
  ast.h         모듈 및 계층 선언 자료구조
  checker.h     의존성과 계층 규칙 검사
  main.cpp      명령행 프로그램
test/           자동 테스트
examples/       정상 및 위반 시연 파일
scripts/        Windows 빌드·테스트 스크립트
기획안/         프로젝트 기획 문서
```

## 현재 범위

1학기 목표인 구조 검사 코어에 집중하고 있습니다. 현재는 모듈 선언, 의존 선언, 계층 선언을 파싱하고 구조 규칙 위반을 실행 전에 거부하는 최소 코어를 구현했습니다. 모듈 내부의 변수·함수·제어 흐름과 실행 백엔드는 이후 확장 범위입니다.

## 시연 흐름

1. `examples/valid.ieum`으로 올바른 구조가 통과함을 보입니다.
2. `examples/implicit_dependency.ieum`으로 선언되지 않은 모듈 의존을 거부함을 보입니다.
3. `examples/cyclic_dependency.ieum`으로 순환 의존을 거부함을 보입니다.
4. `examples/layer_violation.ieum`으로 계층 위반을 거부함을 보입니다.
5. `examples/transitive_layer_violation.ieum`으로 여러 단계 계층 위반도 거부함을 보입니다.
6. `examples/invalid_declarations.ieum`으로 중복 모듈, 미선언 계층 모듈, 자기 계층 선언을 거부함을 보입니다.

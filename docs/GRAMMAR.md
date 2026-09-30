# 이음 문법

## 범위

구조 선언, 모듈 본문, 기본 값·타입·표현식과 현재 실행 의미를 정의합니다.

## EBNF

```text
program        := { NEWLINE | moduleDecl lineEnd | layerDecl lineEnd }

moduleDecl     := MODULE IDENTIFIER [ DEPENDS identifierList ] [ moduleBody ]
moduleBody     := LEFT_BRACE RIGHT_BRACE
                | LEFT_BRACE NEWLINE { NEWLINE | moduleMember NEWLINE } RIGHT_BRACE
moduleMember   := variableDecl | functionDecl

variableDecl   := LET IDENTIFIER [ COLON type ] [ ASSIGN expression ]
type           := "int" | "bool" | "string" | "unit"
functionDecl   := FN IDENTIFIER LEFT_PAREN [ parameterList ] RIGHT_PAREN functionBody
parameterList  := parameter { COMMA parameter }
parameter      := IDENTIFIER [ COLON type ]
functionBody   := LEFT_BRACE RIGHT_BRACE
                | LEFT_BRACE NEWLINE { NEWLINE | statement NEWLINE } RIGHT_BRACE
statement      := variableDecl | assignment | callStatement
assignment     := IDENTIFIER ASSIGN expression
callStatement  := CALL IDENTIFIER LEFT_PAREN [ identifierList ] RIGHT_PAREN

expression     := logicalOr
logicalOr      := logicalAnd { "||" logicalAnd }
logicalAnd     := equality { "&&" equality }
equality       := comparison { ("==" | "!=") comparison }
comparison     := sum { ("<" | "<=" | ">" | ">=") sum }
sum            := product { ("+" | "-") product }
product        := unary { ("*" | "/" | "%") unary }
unary          := ("!" | "+" | "-") unary | primary
primary        := INTEGER | STRING | "true" | "false" | IDENTIFIER
                | LEFT_PAREN RIGHT_PAREN
                | LEFT_PAREN expression RIGHT_PAREN

layerDecl      := LAYER IDENTIFIER ABOVE IDENTIFIER
identifierList := IDENTIFIER { COMMA IDENTIFIER }
lineEnd        := NEWLINE | END
```

## 예제

```text
module data {
  fn save(item) {}
}

module service depends data {
  let cache

  fn handle(request) {
    let prepared
    call save(prepared)
  }
}

layer service above data
```

## 줄과 본문 규칙

- 비어 있지 않은 `{` 본문은 여는 중괄호 다음 줄에서 시작합니다.
- 선언과 문장은 각각 한 줄에 하나만 작성하며, 표현식 중간의 줄바꿈은 지원하지 않습니다.
- 비어 있는 모듈과 함수 본문은 `{}`로 작성할 수 있습니다.
- 함수 안에 함수를 선언할 수 없습니다.
- 모듈 바로 아래에는 `let`과 `fn`만 올 수 있습니다.
- 함수 본문에는 `let`, 변수 재대입과 `call`이 올 수 있습니다.
- `#`부터 줄 끝까지는 주석입니다.

## 식별자와 예약어

- 식별자 형식은 `[A-Za-z_][A-Za-z0-9_]*`입니다.
- `module`, `depends`, `layer`, `above`, `fn`, `let`, `call`, `true`, `false`는 예약어입니다. 기존에 `true`/`false`를 이름으로 사용했다면 다른 이름으로 변경해야 합니다.
- 타입 이름은 `:` 뒤에서 해석하므로 기존 변수 이름 `unit`, `int` 등은 계속 사용할 수 있습니다. unit 리터럴은 `()`입니다.
- 매개변수에는 타입을 명시할 수 있고 호출 인자는 현재 변수 이름만 지원합니다. 호출식과 리터럴 인자는 아직 지원하지 않습니다.

## 이름과 Scope 규칙

- 모듈 변수끼리, 함수끼리는 각 모듈의 이름 공간 안에서 중복될 수 없습니다.
- 매개변수와 지역 변수는 함수 Scope를 공유하며 같은 이름을 중복 선언할 수 없습니다.
- 지역 변수는 선언 다음 문장부터 사용할 수 있습니다.
- 초기화 식은 새 이름이 추가되기 전에 검사·평가합니다. 같은 이름의 바깥 모듈 변수가 있으면 `let x = x + 1`의 우변은 그 모듈 변수를 가리킵니다.
- 모듈 초기화 식에서는 같은 모듈의 앞서 선언한 변수만 참조할 수 있습니다. 함수에서는 해당 모듈의 모든 변수를 참조할 수 있습니다.
- 호출 인자는 함수의 매개변수, 앞서 선언한 지역 변수 또는 현재 모듈 변수여야 합니다.
- 지역 변수와 매개변수는 같은 이름의 모듈 변수를 가릴 수 있습니다.
- 함수 호출은 현재 모듈 함수를 먼저 찾고, 없으면 직접 `depends`로 선언한 모듈에서 찾습니다.
- 호출 대상이 다른 모듈에만 있으면 `depends` 누락이며, 여러 직접 의존 모듈에 있으면 모호한 호출입니다.
- 호출 인자 수는 대상 함수의 매개변수 수와 같아야 합니다.
- 초기화 식의 타입으로 변수를 추론하며 명시한 타입과 식의 타입이 다르면 오류입니다. 재대입·호출 인자도 같은 타입이어야 합니다. 숫자·문자열 사이의 암묵적 변환은 없습니다.
- 초기화 없는 `let name`은 unit 값입니다. `let name: unit`도 가능하며, 다른 타입은 초기화 식이 필요합니다. 타입 없는 매개변수는 unit입니다.
- 종료 조건을 표현할 수 없는 현재 문법에서는 직접·간접 재귀 호출을 거부합니다.

## 값과 연산

- `int`는 -9223372036854775808~9223372036854775807 범위의 부호 있는 64비트 정수입니다. 10진수 리터럴을 사용합니다. 최솟값은 `-9223372036854775808`로 작성하며 양수 `9223372036854775808` 자체는 오류입니다.
- 정수의 `+ - * / %`와 단항 `+ -`를 지원합니다. 나눗셈은 0 방향으로 버림하며 나머지는 피제수의 부호를 따릅니다. 예: `-7 / 3 == -2`, `-7 % 3 == -1`.
- 리터럴 범위 초과는 파싱 오류이고 연산 중 오버플로와 0 나눗셈은 실행 오류입니다. 최솟값을 -1로 나누거나 나머지를 구하는 연산도 오버플로 오류입니다.
- `bool`은 `true`, `false`이며 `!`, `&&`, `||`를 지원합니다. `&&`와 `||`는 왼쪽부터 단락 평가하지만 타입·이름 검사는 양쪽 모두에 적용합니다. `false && (1 / 0 == 0)`은 false이고 `false && missing`은 의미 오류입니다.
- `string`은 큰따옴표로 감싼 유효한 UTF-8 문자열입니다. `\"`, `\\`, `\n`, `\r`, `\t` escape를 지원합니다. 원시 제어 문자·실제 줄바꿈·지원하지 않는 escape는 오류입니다. 문자열 안의 `#`는 주석이 아닙니다.
- 문자열 `+`는 결합입니다. 같은 타입끼리 `==`, `!=`를 사용할 수 있으며 문자열 비교는 UTF-8 바이트의 정확한 일치를 사용합니다. 정규화·문자열 순서 비교·인덱싱은 제공하지 않습니다. `< <= > >=`는 정수에만 적용합니다.
- 우선순위는 낮은 순서로 `||`, `&&`, `== !=`, `< <= > >=`, `+ -`, `* / %`, 단항 `! + -`, 괄호입니다. 이항 연산은 왼쪽 결합입니다. `1 < 2 < 3`은 bool과 int를 비교하므로 타입 오류입니다.
- 과도한 구문 중첩이나 너무 깊게 연결된 식은 파싱 오류로 제한합니다.

## 실행 의미

- 구조 검사와 의미 검사를 모두 통과해야 실행합니다. `--run`이 없는 검사 명령은 값을 평가하지 않습니다.
- 실행할 때 모든 모듈 변수를 각 모듈 내 선언 순서대로 한 번 초기화합니다. 초기화에서 실행 오류가 발생하면 진입 함수도 실행하지 않습니다. 실행기를 다시 호출하면 모듈 상태를 새로 초기화합니다.
- `let`은 초기화 식을 평가한 값(생략 시 unit)을 저장합니다. 대입은 지역·매개변수에서 먼저 이름을 찾고, 없으면 현재 모듈의 변수 값을 변경합니다.
- `call`은 의미 분석에서 결정한 함수를 동기적으로 실행하며 호출마다 독립적인 지역·매개변수 저장 공간을 만듭니다. 인자는 값으로 복사하므로 매개변수를 재대입해도 호출자의 변수는 바뀌지 않습니다. 모듈 변수 변경은 같은 실행 내에서 유지됩니다.
- 함수는 현재 unit을 반환합니다. 함수 재귀 금지는 유지합니다.
- CLI의 `--run <모듈>.<함수>`로 진입 함수를 지정하며 진입 함수는 매개변수가 없어야 합니다.
- 실행 결과는 함수 진입, 호출, 종료 순서와 실행 횟수를 Trace로 출력합니다. 이어서 모든 모듈 변수와 진입 함수의 최종 지역 값 중 unit이 아닌 값을 이름순으로 출력합니다. 문자열은 escape를 적용해 한 줄로 표시합니다. 기존 unit 전용 예제의 Trace 출력은 유지됩니다.

## 소스 위치

토큰·선언·표현식·문장은 파일·행·열 위치를 보관합니다. 표현식과 문장 ID는 한 번 파싱한 프로그램 안에서 고유하며 호출 해석에 사용됩니다. 새 렉싱·파싱·타입·값 실행 오류는 `파일:행:열`을 제공합니다. 열은 1부터 시작하는 UTF-8 바이트 위치이며 탭은 1바이트로 셉니다. BOM은 열에 포함하지 않고, CRLF도 한 줄바꿈으로 취급합니다. 기존 구조 오류의 행 진단은 유지됩니다.

## 현재 지원하지 않는 항목

- 반환문, 조건문과 반복문
- 중첩 블록, 사용자 정의 자료구조, 함수 반환값·호출식
- 여러 파일 로딩, 표준 입출력, 부동소수점

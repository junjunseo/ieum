# 이음 문법

## 범위

구조 선언, 모듈 본문, 값·타입·표현식, 함수 반환값과 제어 흐름의 실행 의미를 정의합니다.

## EBNF

```text
program        := { NEWLINE | moduleDecl lineEnd | layerDecl lineEnd }

moduleDecl     := MODULE IDENTIFIER [ DEPENDS identifierList ] [ moduleBody ]
moduleBody     := LEFT_BRACE RIGHT_BRACE
                | LEFT_BRACE NEWLINE { NEWLINE | moduleMember NEWLINE } RIGHT_BRACE
moduleMember   := variableDecl | functionDecl

variableDecl   := LET IDENTIFIER [ COLON type ] [ ASSIGN expression ]
type           := "int" | "bool" | "string" | "unit"
functionDecl   := FN IDENTIFIER LEFT_PAREN [ parameterList ] RIGHT_PAREN [ "->" type ] block
parameterList  := parameter { COMMA parameter }
parameter      := IDENTIFIER [ COLON type ]
block          := LEFT_BRACE RIGHT_BRACE
                | LEFT_BRACE NEWLINE { NEWLINE | statement NEWLINE } RIGHT_BRACE
statement      := variableDecl | assignment | callStatement | returnStatement
                | block | ifStatement | whileStatement | forStatement | "break" | "continue"
assignment     := IDENTIFIER ASSIGN expression
callStatement  := CALL callExpression
callExpression := IDENTIFIER LEFT_PAREN [ argumentList ] RIGHT_PAREN
argumentList   := expression { COMMA expression }
returnStatement := "return" [ expression ]
ifStatement    := "if" expression block [ { NEWLINE } "else" (block | ifStatement) ]
whileStatement := "while" expression block
forStatement   := "for" LEFT_PAREN [ forInit ] ";" [ expression ] ";" [ forUpdate ] RIGHT_PAREN block
forInit        := variableDecl | assignment | callStatement
forUpdate      := assignment | callStatement

expression     := logicalOr
logicalOr      := logicalAnd { "||" logicalAnd }
logicalAnd     := equality { "&&" equality }
equality       := comparison { ("==" | "!=") comparison }
comparison     := sum { ("<" | "<=" | ">" | ">=") sum }
sum            := product { ("+" | "-") product }
product        := unary { ("*" | "/" | "%") unary }
unary          := ("!" | "+" | "-") unary | primary
primary        := INTEGER | STRING | "true" | "false" | IDENTIFIER | callExpression
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
- 비어 있는 모듈·함수·조건·반복·독립 블록은 `{}`로 작성할 수 있습니다. `else`는 앞의 `}`와 같은 줄 또는 다음 줄에 작성할 수 있고, `else if`도 지원합니다.
- 함수 안에 함수를 선언할 수 없습니다.
- 모듈 바로 아래에는 `let`과 `fn`만 올 수 있습니다.
- 함수 본문에는 `let`, 변수 재대입, `call`, `return`, 블록, `if`/`else`, `while`, `for`, `break`/`continue`가 올 수 있습니다. 함수 호출 자체를 문장으로 쓸 때는 `call f(...)`를 사용합니다.
- `#`부터 줄 끝까지는 주석입니다.

## 식별자와 예약어

- 식별자 형식은 `[A-Za-z_][A-Za-z0-9_]*`입니다.
- `module`, `depends`, `layer`, `above`, `fn`, `let`, `call`, `true`, `false`, `return`, `if`, `else`, `while`, `for`, `break`, `continue`는 예약어입니다. 이 확장으로 예약어가 된 이름을 이전 코드에서 사용했다면 다른 이름으로 변경해야 합니다.
- 타입 이름은 `:` 또는 `->` 뒤에서 해석하므로 기존 변수 이름 `unit`, `int` 등은 계속 사용할 수 있습니다. unit 리터럴은 `()`입니다.
- 함수 반환 타입은 `fn f(n: int) -> int { ... }`처럼 명시합니다. 생략 시 unit입니다. non-unit 함수는 모든 매개변수 타입도 명시해야 합니다. 호출 인자는 리터럴과 중첩 호출을 포함한 표현식입니다.

## 이름과 Scope 규칙

- 모듈 변수끼리, 함수끼리는 각 모듈의 이름 공간 안에서 중복될 수 없습니다.
- 매개변수와 함수 최상위 지역 변수는 같은 Scope를 공유합니다. 각 중첩 블록·조건 분기·반복 본문은 새 Scope를 만들며 같은 Scope 안에서는 이름을 중복 선언할 수 없습니다. 반복 본문의 Scope는 매 반복마다 새로 만듭니다.
- 지역 변수는 선언 다음 문장부터 사용할 수 있습니다.
- 초기화 식은 새 이름이 추가되기 전에 검사·평가합니다. 같은 이름의 바깥 지역·매개변수·모듈 변수가 있으면 `let x = x + 1`의 우변은 가장 가까운 바깥 변수를 가리킵니다.
- 모듈 초기화 식에서는 같은 모듈의 앞서 선언한 변수만 참조할 수 있습니다. 함수에서는 해당 모듈의 모든 변수를 참조할 수 있습니다.
- 안쪽 지역 변수는 바깥 지역·매개변수·모듈 변수를 가릴 수 있습니다. 블록을 벗어난 지역 변수는 접근할 수 없습니다. 호출된 함수는 호출자의 지역 변수에 접근하지 않습니다.
- 함수 호출은 현재 모듈 함수를 먼저 찾고, 없으면 직접 `depends`로 선언한 모듈에서 찾습니다.
- 호출 대상이 다른 모듈에만 있으면 `depends` 누락이며, 여러 직접 의존 모듈에 있으면 모호한 호출입니다.
- 호출 인자 수는 대상 함수의 매개변수 수와 같아야 합니다.
- 초기화 식의 타입으로 변수를 추론하며 명시한 타입과 식의 타입이 다르면 오류입니다. 재대입·호출 인자도 같은 타입이어야 합니다. 숫자·문자열 사이의 암묵적 변환은 없습니다.
- 초기화 없는 `let name`은 unit 값입니다. `let name: unit`도 가능하며, 다른 타입은 초기화 식이 필요합니다. 타입 없는 매개변수는 unit입니다.
- 직접·간접 함수 재귀를 허용합니다. 모듈 사이의 순환 의존 금지는 유지합니다. 실행하지 않는 분기·단락 평가의 우변·return 뒤의 코드에도 이름·타입·호출 의존 검사를 적용합니다.

## 값과 연산

- `int`는 -9223372036854775808~9223372036854775807 범위의 부호 있는 64비트 정수입니다. 10진수 리터럴을 사용합니다. 최솟값은 `-9223372036854775808`로 작성하며 양수 `9223372036854775808` 자체는 오류입니다.
- 정수의 `+ - * / %`와 단항 `+ -`를 지원합니다. 나눗셈은 0 방향으로 버림하며 나머지는 피제수의 부호를 따릅니다. 예: `-7 / 3 == -2`, `-7 % 3 == -1`.
- 리터럴 범위 초과는 파싱 오류이고 연산 중 오버플로와 0 나눗셈은 실행 오류입니다. 최솟값을 -1로 나누거나 나머지를 구하는 연산도 오버플로 오류입니다.
- `bool`은 `true`, `false`이며 `!`, `&&`, `||`를 지원합니다. `&&`와 `||`는 왼쪽부터 단락 평가하지만 타입·이름 검사는 양쪽 모두에 적용합니다. `false && (1 / 0 == 0)`은 false이고 `false && missing`은 의미 오류입니다.
- `string`은 큰따옴표로 감싼 유효한 UTF-8 문자열입니다. `\"`, `\\`, `\n`, `\r`, `\t` escape를 지원합니다. 원시 제어 문자·실제 줄바꿈·지원하지 않는 escape는 오류입니다. 문자열 안의 `#`는 주석이 아닙니다.
- 문자열 `+`는 결합입니다. 같은 타입끼리 `==`, `!=`를 사용할 수 있으며 문자열 비교는 UTF-8 바이트의 정확한 일치를 사용합니다. 정규화·문자열 순서 비교·인덱싱은 제공하지 않습니다. `< <= > >=`는 정수에만 적용합니다.
- 우선순위는 낮은 순서로 `||`, `&&`, `== !=`, `< <= > >=`, `+ -`, `* / %`, 단항 `! + -`, 괄호입니다. 이항 연산은 왼쪽 결합입니다. `1 < 2 < 3`은 bool과 int를 비교하므로 타입 오류입니다.
- 표현식 구문/트리 깊이와 문장 중첩 깊이를 각각 128까지 허용하며 초과하면 위치를 포함한 파싱 오류입니다.

## 실행 의미

- 구조 검사와 의미 검사를 모두 통과해야 실행합니다. `--run`이 없는 검사 명령은 값을 평가하지 않습니다.
- 실행할 때 모듈의 소스 순서, 각 모듈 변수의 선언 순서대로 한 번 초기화합니다. 초기화 식에서도 함수를 호출할 수 있지만, 함수가 아직 초기화되지 않은 모듈 변수를 읽거나 재대입하면 `uninitialized_variable` 오류입니다. 초기화에서 실행 오류가 발생하면 진입 함수도 실행하지 않습니다. 실행기를 다시 호출하면 모듈 상태를 새로 초기화합니다.
- `let`은 초기화 식을 평가한 값(생략 시 unit)을 저장합니다. 대입은 가장 안쪽 지역 Scope부터 바깥 지역·매개변수에서 이름을 찾고, 없으면 현재 모듈의 변수 값을 변경합니다.
- 호출식과 `call`은 의미 분석에서 결정한 함수를 동기적으로 실행하며 호출마다 독립적인 지역·매개변수 저장 공간을 만듭니다. 인자는 값으로 복사하므로 매개변수를 재대입해도 호출자의 변수는 바뀌지 않습니다. 모듈 변수 변경은 같은 실행 내에서 유지됩니다.
- 호출 인자와 이항 연산의 피연산자는 왼쪽부터 평가합니다. `&&`/`||`에서 생략한 우변의 함수 호출은 실행하지 않습니다. 호출식은 반환값을 만들고 `call` 문장은 반환값을 버립니다.
- `return expression`은 함수의 모든 중첩 블록·반복을 빠져나와 값을 반환합니다. `return`은 unit을 반환합니다. unit 함수는 본문 끝까지 도달해도 됩니다. non-unit 함수의 반환값 타입이 맞지 않거나 반환하지 않는 경로가 있으면 의미 오류입니다.
- 반환 검사는 보수적으로 수행합니다. `if`는 양쪽 분기를 검사하고 `while`은 0회 실행될 가능성을 가정하므로, 반복 내부의 return만으로 반환을 보장하지 않습니다. `while true { ... }` 뒤에도 non-unit 함수의 반환 경로가 필요합니다.
- `if`/`while` 조건은 bool이어야 합니다. while은 매 반복 전에 조건을 다시 평가합니다. `break`는 가장 가까운 반복을 종료하고 `continue`는 그 반복의 다음 단계(while은 조건 검사, for는 증감)로 이동하며 벗어나는 Scope를 정리합니다. 반복문 밖의 break/continue는 의미 오류입니다.
- CLI의 `--run <모듈>.<함수>`로 진입 함수를 지정하며 진입 함수는 매개변수가 없어야 합니다.
- 실행 결과는 함수 진입, 호출, 종료 순서와 실행 횟수를 Trace로 출력합니다. 이어서 non-unit 진입 함수의 반환값, 모든 모듈 변수와 진입 함수 최상위 Scope의 최종 지역 값 중 unit이 아닌 값을 이름순으로 출력합니다. 문자열은 escape를 적용해 한 줄로 표시합니다. 기존 unit 전용 예제의 Trace 출력은 유지됩니다.

## for 반복문

```text
let sum = 0
for (let i = 1; i <= 10; i = i + 1) {
  sum = sum + i
}
# sum은 55, i는 이 위치에서 접근할 수 없음
```

- 괄호 안에 초기화·조건·증감을 세미콜론 두 개로 구분합니다. 헤더는 한 줄에 작성합니다. 세미콜론은 for 헤더에서만 사용하며 일반 문장의 구분자는 계속 줄바꿈입니다.
- 초기화에는 `let`, 대입 또는 `call`을, 증감에는 대입 또는 `call`을 작성할 수 있습니다. 예: `for (call start(); ready(); call next()) { ... }`. 각 부분에는 한 문장만 허용합니다. `++`, `--`, `+=`는 지원하지 않으므로 `i = i + 1`을 사용합니다.
- 초기화는 한 번 실행합니다. 이후 조건 → 본문 → 증감 순서를 반복합니다. 초기화·조건·증감을 각각 생략할 수 있으며, 생략한 조건은 true입니다. `for (;;) { ... }`에도 실행 한도를 적용합니다.
- 명시한 조건은 bool이어야 합니다. 초기화·조건·증감의 호출에도 인자 타입, depends와 기존 구조 검사가 적용됩니다.
- 초기화에서 선언한 변수는 헤더와 본문에서만 보입니다. 본문은 매 반복마다 별도 Scope를 만들며, 본문에서 선언한 변수는 조건·증감에서 보이지 않습니다. 증감은 본문 Scope를 정리한 후 헤더 Scope에서 실행합니다.
- 가장 가까운 반복이 for일 때 `continue`는 증감을 실행한 뒤 조건 검사로 돌아갑니다. `break`는 증감 없이 가장 가까운 반복을 종료하고, `return`은 증감 없이 함수를 종료합니다. while과 for를 중첩해도 가장 가까운 반복에만 break/continue를 적용합니다.
- 반환 경로는 while과 마찬가지로 보수적으로 검사합니다. 조건을 생략한 for도 non-unit 함수의 반환을 보장하는 것으로 간주하지 않습니다.

[for 예제](../examples/for_loop.ieum)를 `--run app.main`으로 실행하면 합계 55를 반환하고 홀수 합계 `oddSum`은 25입니다.

## 실행 한도

기본 한도는 실행기 내부 단계 100,000회, 호출 깊이 1,024입니다. 모듈 초기화·식 평가·조건 검사·블록 진입/종료 등도 단계를 소비하므로 소스 문장 수와 같지 않습니다. 호출 깊이는 진입 함수도 1로 세며 재귀 여부와 무관하게 적용합니다. 값 평가와 함수 호출은 C++ 재귀 대신 명시적인 작업·값·호출 프레임 스택으로 처리합니다.

```powershell
.\build\ieum.exe .\examples\control_flow.ieum --run app.main --max-steps 10000 --max-call-depth 20
```

한도 초과는 `step_limit` 또는 `call_depth_limit`과 파일·행·열을 포함한 실행 오류이며 종료 코드는 1입니다. 한도 옵션은 `--run`과 함께 각 1회만 사용할 수 있고 양의 정수가 필요합니다. 잘못된 CLI 옵션은 종료 코드 2입니다. 검사만 수행할 때는 실행하지 않으므로 무한 반복 코드도 구조·의미 검사를 통과할 수 있습니다. 한도는 실행 단계/호출 깊이에 대한 제한이며 메모리 크기나 경과 시간의 제한은 아닙니다.

[제어 흐름 예제](../examples/control_flow.ieum)의 `sum`은 55, `fact`는 120, `oddSum`은 25이며 `app.main`은 175를 반환합니다.

## 소스 위치

토큰·선언·표현식·문장은 파일·행·열 위치를 보관합니다. 표현식과 문장 ID는 한 번 파싱한 프로그램 안에서 고유하며 호출 해석에 사용됩니다. 새 렉싱·파싱·타입·값 실행 오류는 `파일:행:열`을 제공합니다. 열은 1부터 시작하는 UTF-8 바이트 위치이며 탭은 1바이트로 셉니다. BOM은 열에 포함하지 않고, CRLF도 한 줄바꿈으로 취급합니다. 기존 구조 오류의 행 진단은 유지됩니다.

## 현재 지원하지 않는 항목

- 사용자 정의 자료구조, 함수 값·클로저
- 여러 파일 로딩, 표준 입출력, 부동소수점

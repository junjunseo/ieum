# 이음 문법

## 범위

구조 선언, 모듈 본문, 값·타입·표현식, 함수 반환값·제어 흐름·자료구조·기본 입출력의 실행 의미를 정의합니다.

## EBNF

```text
program        := { NEWLINE | moduleDecl lineEnd | layerDecl lineEnd }

moduleDecl     := MODULE IDENTIFIER [ DEPENDS identifierList ] [ moduleBody ]
moduleBody     := LEFT_BRACE RIGHT_BRACE
                | LEFT_BRACE NEWLINE { NEWLINE | moduleMember NEWLINE } RIGHT_BRACE
moduleMember   := [ "private" ] (variableDecl | functionDecl | recordDecl)
recordDecl     := "record" IDENTIFIER LEFT_BRACE RIGHT_BRACE
                | "record" IDENTIFIER LEFT_BRACE NEWLINE { NEWLINE | fieldDecl NEWLINE } RIGHT_BRACE
fieldDecl      := IDENTIFIER COLON type

variableDecl   := LET IDENTIFIER [ COLON type ] [ ASSIGN expression ]
type           := "int" | "bool" | "string" | "unit" | "list" "<" type ">" | qualifiedName
functionDecl   := FN IDENTIFIER LEFT_PAREN [ parameterList ] RIGHT_PAREN [ "->" type ] block
parameterList  := parameter { COMMA parameter }
parameter      := IDENTIFIER [ COLON type ]
block          := LEFT_BRACE RIGHT_BRACE
                | LEFT_BRACE NEWLINE { NEWLINE | statement NEWLINE } RIGHT_BRACE
statement      := variableDecl | assignment | callStatement | returnStatement
                | block | ifStatement | whileStatement | forStatement | "break" | "continue"
assignment     := IDENTIFIER { LEFT_BRACKET expression RIGHT_BRACKET | DOT IDENTIFIER } ASSIGN expression
callStatement  := CALL callExpression
callExpression := qualifiedName LEFT_PAREN [ argumentList ] RIGHT_PAREN
qualifiedName  := IDENTIFIER [ DOT IDENTIFIER ]
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
unary          := ("!" | "+" | "-") unary | postfix
postfix        := primary { LEFT_BRACKET expression RIGHT_BRACKET | DOT IDENTIFIER }
primary        := INTEGER | STRING | "true" | "false" | IDENTIFIER | callExpression
                | LEFT_BRACKET [ argumentList ] RIGHT_BRACKET
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
- 모듈 바로 아래에는 `let`, `fn`, `record`만 올 수 있습니다.
- 함수 본문에는 `let`, 변수 재대입, `call`, `return`, 블록, `if`/`else`, `while`, `for`, `break`/`continue`가 올 수 있습니다. 함수 호출 자체를 문장으로 쓸 때는 `call f(...)`를 사용합니다.
- `#`부터 줄 끝까지는 주석입니다.

## 식별자와 예약어

- 식별자 형식은 `[A-Za-z_][A-Za-z0-9_]*`입니다.
- `module`, `depends`, `layer`, `above`, `fn`, `let`, `call`, `true`, `false`, `return`, `if`, `else`, `while`, `for`, `break`, `continue`, `record`, `private`는 예약어입니다. 이 확장으로 예약어가 된 이름을 이전 코드에서 사용했다면 다른 이름으로 변경해야 합니다.
- 타입 이름은 `:` 또는 `->` 뒤에서 해석하므로 기존 변수 이름 `unit`, `int` 등은 계속 사용할 수 있습니다. unit 리터럴은 `()`입니다.
- 함수 반환 타입은 `fn f(n: int) -> int { ... }`처럼 명시합니다. 생략 시 unit입니다. non-unit 함수는 모든 매개변수 타입도 명시해야 합니다. 호출 인자는 리터럴과 중첩 호출을 포함한 표현식입니다.

## 이름과 Scope 규칙

- 모듈 변수끼리, 함수끼리는 각 모듈의 이름 공간 안에서 중복될 수 없습니다.
- 매개변수와 함수 최상위 지역 변수는 같은 Scope를 공유합니다. 각 중첩 블록·조건 분기·반복 본문은 새 Scope를 만들며 같은 Scope 안에서는 이름을 중복 선언할 수 없습니다. 반복 본문의 Scope는 매 반복마다 새로 만듭니다.
- 지역 변수는 선언 다음 문장부터 사용할 수 있습니다.
- 초기화 식은 새 이름이 추가되기 전에 검사·평가합니다. 같은 이름의 바깥 지역·매개변수·모듈 변수가 있으면 `let x = x + 1`의 우변은 가장 가까운 바깥 변수를 가리킵니다.
- 모듈 초기화 식에서는 같은 모듈의 앞서 선언한 변수와 직접 의존 모듈의 공개 값을 참조할 수 있습니다. 함수에서는 해당 모듈의 모든 변수를 참조할 수 있습니다.
- 안쪽 지역 변수는 바깥 지역·매개변수·모듈 변수를 가릴 수 있습니다. 블록을 벗어난 지역 변수는 접근할 수 없습니다. 호출된 함수는 호출자의 지역 변수에 접근하지 않습니다.
- 한정되지 않은 함수 호출은 현재 모듈 함수를 먼저 찾고, 없으면 직접 `depends`로 선언한 모듈의 공개 함수에서 찾습니다.
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
- 실행할 때 의존 모듈부터 초기화하고, 각 모듈 안에서는 변수 선언 순서대로 한 번 초기화합니다. 서로 독립인 모듈은 소스 순서를 유지합니다. 초기화 식에서도 함수를 호출할 수 있지만, 함수가 아직 초기화되지 않은 모듈 변수를 읽거나 재대입하면 `uninitialized_variable` 오류입니다. 초기화에서 실행 오류가 발생하면 진입 함수도 실행하지 않습니다. 실행기를 다시 호출하면 모듈 상태를 새로 초기화합니다.
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

## 리스트와 레코드

`[10, 20, 30]`은 `list<int>`입니다. 원소는 모두 같은 타입이어야 하며 `list<list<int>>`처럼 중첩할 수 있습니다. 목록 길이는 생성 시 정해지며 `values[0]`으로 조회하고 `values[0] = 7`로 갱신합니다. 인덱스는 0부터 시작하는 int이고 음수·범위 초과는 `index_out_of_range` 실행 오류입니다. 문자열 인덱싱은 제공하지 않습니다.

빈 목록 `[]`에는 원소 타입이 필요합니다. `let values: list<int> = []`처럼 타입을 명시하거나, 이미 타입이 정해진 대입 대상·함수 인자·반환 타입·레코드 필드에서 타입을 전달받을 수 있습니다. 일반 목록 리터럴은 앞 원소부터 추론하므로 `[[], [1]]`에는 `list<list<int>>` 같은 명시 타입이 필요합니다. `length([])`만으로는 원소 타입을 결정할 수 없습니다.

```text
record Person {
  name: string
  scores: list<int>
}
let original = Person("Jun", [10, 20])
let copy = original
copy.scores[0] = 99
# original.scores[0]은 계속 10
```

record는 모듈에 선언하며 모든 필드의 타입을 명시합니다. 생성자 인자는 필드 선언 순서입니다. 빈 레코드 `record Empty {}`도 가능합니다. 생성자는 같은 모듈의 함수 이름 공간을 사용하므로 같은 이름의 함수/레코드를 중복 선언할 수 없습니다. 레코드의 정체성은 모듈과 이름으로 구분하고, 필드 모양이 같더라도 다른 이름의 타입끼리는 대입할 수 없습니다. 타입 이름과 생성자 모두 현재 모듈을 먼저 찾고 직접 depends 모듈에서 찾습니다. 모호한 타입, 의존 누락, 알 수 없는 타입·필드와 순환 레코드 타입은 의미 오류입니다. 기본 타입 이름을 record 이름으로 사용할 수 없습니다.

목록과 레코드는 대입·인자 전달·반환 모두 값 복사 의미입니다. 복사한 값의 중첩 필드나 원소를 갱신해도 원본은 바뀌지 않습니다. 문자열을 포함한 기본 값처럼 같은 타입끼리 내용에 따른 `==`/`!=` 비교가 가능합니다. 목록의 길이/순서, 레코드의 타입/필드 값을 비교합니다. 갱신 대상은 변수에서 시작하는 `items[0].scores[1]` 같은 경로입니다. 경로의 인덱스를 왼쪽부터 한 번씩 평가한 뒤 우변을 평가하고, 그 시점의 대상 변수 값에 갱신합니다. 갱신할 때 인덱스 범위를 확인합니다.

자료구조 값의 중첩 깊이, 추론된 목록 타입 깊이, 레코드 타입 연결 깊이는 128까지 제한합니다. 길이 변경 연산·공유 참조·순환 객체·문자열 인덱싱·일반 제네릭은 제공하지 않습니다. [자료구조 예제](../examples/collections.ieum)는 목록 합계 60과 개수 3, 복사본 갱신 후 원본 보존을 보여 줍니다.

## 내장 모듈과 입출력

내장 함수는 해당 모듈을 `depends`에 선언한 뒤 기존 함수처럼 이름으로 호출합니다. 예: `module app depends std_io, std_text, std_list`. 직접 depends 모듈 간 이름이 겹치면 기존의 모호한 호출 검사도 적용합니다. 내장 모듈은 구조·계층 검사와 DOT에 참여하며 사용하지 않는 내장 모듈은 추가하지 않습니다. `std_io`, `std_text`, `std_list`는 내장 모듈 이름으로 예약되어 사용자 모듈로 재선언할 수 없습니다.

| 모듈 | 함수 | 동작 |
| --- | --- | --- |
| std_io | `print(text: string) -> unit` | 문자열과 줄바꿈을 표준 출력에 쓰기 |
| std_io | `read_line() -> string` | 표준 입력에서 한 줄 읽기, 줄 끝 LF/CRLF 제거 |
| std_io | `read_text(path: string) -> string` | UTF-8 텍스트 파일 전체 읽기 |
| std_io | `write_text(path: string, text: string) -> unit` | 파일 생성 또는 기존 내용을 덮어쓰기 |
| std_text | `parse_int(text: string) -> int` | 앞뒤 ASCII 공백을 제외한 부호 있는 10진 정수 변환 |
| std_text | `to_string(value: int) -> string` | 정수를 문자열로 명시적 변환 |
| std_text | `split(text: string, separator: string) -> list<string>` | 구분 문자열로 나누기, 앞뒤/연속 구분자의 빈 원소 유지 |
| std_list | `length(values: list<T>) -> int` | 원소 타입과 무관한 목록 개수 반환 |

`length`의 T는 내장 함수가 허용하는 목록 원소 타입을 설명하는 표기이며 사용자 제네릭 문법은 아닙니다. `print`에 정수를 출력하려면 `print(to_string(number))`처럼 변환합니다. 출력은 CLI의 검사·Trace 출력과 같은 표준 출력에 나타납니다. 생성자와 내장 함수 호출도 Trace와 호출 수에 포함됩니다.

빈 입력 줄은 빈 문자열이고 입력 종료/실패는 `io_error`입니다. 잘못된 UTF-8 파일/입력은 `invalid_utf8`, 파일 열기·읽기·쓰기 실패는 `io_error`, 정수가 아닌 문자열은 `invalid_integer`, 범위 초과는 `integer_overflow`, 빈 분리 구분자는 `empty_separator` 실행 오류입니다. CLI의 소스·DOT 경로와 입출력 함수의 파일 경로는 Windows에서도 UTF-8로 처리합니다. 상대 경로는 프로세스의 현재 작업 디렉터리 기준입니다. 상위 디렉터리를 자동 생성하지 않습니다. 파일 내용은 줄바꿈을 바꾸지 않고 저장합니다. 기본 입출력 호출은 동기식이며 실행 단계 한도는 파일 크기나 입력 대기 시간을 제한하지 않습니다.

[파일 합산 예제](../examples/collections_io.ieum)는 입력 경로와 출력 경로를 두 줄로 받아 파일 읽기 → 줄 분리 → 정수 변환 → 합산 → 결과 쓰기를 수행합니다. [숫자 fixture](../test/fixtures/numbers.txt)의 합계는 60, 개수는 3입니다. 오류 경로 검증은 임시 디렉터리에서 수행합니다.

## 여러 파일 모듈과 공개 범위

```powershell
.\build\ieum.exe .\examples\multifile\app.ieum --module-path .\examples\multifile --run app.main
```

`--module-path`는 모듈 검색 디렉터리이며 여러 번 지정할 수 있습니다. 상대 검색 경로는 현재 작업 디렉터리 기준입니다. 로더는 진입 파일의 `depends data`에 대응하는 `data.ieum`을 지정한 디렉터리에서 찾고, 그 파일의 의존 모듈도 같은 방식으로 읽습니다. 파일 안에 요청한 이름의 모듈 선언이 있어야 합니다. 한 파일에 여러 모듈을 둘 수 있으며 이미 로딩된 모듈은 다시 찾지 않습니다. layer 선언만으로 파일을 로딩하지는 않습니다.

검색 경로를 지정하지 않으면 기존 단일 파일 동작을 유지하며 주변 파일을 읽지 않습니다. 지정한 디렉터리에서도 필요하지 않은 파일은 읽지 않고 하위 디렉터리를 자동 탐색하지 않습니다. 모듈 검색 경로는 정규화한 UTF-8 경로 순으로 처리하며 같은 모듈 파일이 여러 검색 경로에 있으면 첫 파일을 고르지 않고 충돌로 거부합니다. `.`/`..`·심볼릭 링크 등으로 같은 디렉터리를 중복 지정하거나 같은 파일을 여러 경로로 가리켜도 중복 경로 오류입니다. 파일 내 모듈 이름 중복은 기존 구조 오류이며 서로 다른 파일의 충돌이면 양쪽 위치를 표시합니다. 여러 파일을 합친 AST의 노드 ID는 프로그램 전체에서 고유합니다.

함수/생성자 호출, 타입, 모듈 변수에 `data.get()`, `data.Point(1)`, `list<data.Point>`, `data.values[0]` 같은 한정 이름을 사용할 수 있습니다. 공개 모듈 변수는 같은 타입으로 갱신할 수 있으며 목록/레코드를 읽어 지역 변수에 대입하면 값 복사 의미를 유지합니다. 한정 이름도 직접 `depends`가 필요하고 해당 의존 관계에 모든 layer/순환 검사를 적용합니다. 호출의 매개변수·반환 타입이나 읽은 모듈 값의 레코드 타입에도 그 타입 소유 모듈의 직접 의존이 필요합니다. 내장 함수도 `std_io.print(...)`처럼 부를 수 있으며 동일한 검사를 받습니다.

선언은 기본 공개입니다. 모듈 바로 아래의 `private fn`, `private let`, `private record`는 해당 모듈에서만 참조할 수 있습니다. private record의 생성자와 타입도 private이며 private 함수는 CLI 진입점이 될 수 없습니다. 공개 함수가 반환한 private 타입을 통해 접근하는 것도 거부합니다. record 필드별 공개 범위는 제공하지 않습니다. private는 언어의 이름 접근 규칙이며 실행 Trace/진단을 숨기는 기능은 아닙니다.

기존 한정되지 않은 함수/타입 참조는 현재 모듈을 먼저 찾고 직접 의존 모듈의 공개 선언에서 찾습니다. 여러 공개 선언이 일치하면 모호한 이름 오류이며 한정 이름으로 구분합니다. 값의 필드 접근에서는 지역/현재 모듈 변수가 모듈 접두어보다 우선합니다. 함수 호출과 타입 이름은 각각 함수·타입 이름 공간에서 해석합니다.

모듈 값은 의존 모듈부터 초기화하고, 서로 독립인 모듈은 소스 순서를 유지합니다. 각 모듈 안에서는 변수 선언 순서대로 초기화합니다. 모듈 초기화 중 호출한 함수가 아직 초기화되지 않은 값을 읽으면 실행 오류입니다. [세 파일 예제](../examples/multifile/app.ieum)는 data의 목록을 service에서 합산하고 app에서 `60`을 출력·반환합니다.

## 실행 한도

기본 한도는 실행기 내부 단계 100,000회, 호출 깊이 1,024입니다. 모듈 초기화·식 평가·조건 검사·블록 진입/종료 등도 단계를 소비하므로 소스 문장 수와 같지 않습니다. 호출 깊이는 진입 함수도 1로 세며 재귀 여부와 무관하게 적용합니다. 값 평가와 함수 호출은 C++ 재귀 대신 명시적인 작업·값·호출 프레임 스택으로 처리합니다.

```powershell
.\build\ieum.exe .\examples\control_flow.ieum --run app.main --max-steps 10000 --max-call-depth 20
```

한도 초과는 `step_limit` 또는 `call_depth_limit`과 파일·행·열을 포함한 실행 오류이며 종료 코드는 1입니다. 한도 옵션은 `--run`과 함께 각 1회만 사용할 수 있고 양의 정수가 필요합니다. 잘못된 CLI 옵션은 종료 코드 2입니다. 검사만 수행할 때는 실행하지 않으므로 무한 반복 코드도 구조·의미 검사를 통과할 수 있습니다. 한도는 실행 단계/호출 깊이에 대한 제한이며 메모리 크기나 경과 시간의 제한은 아닙니다.

[제어 흐름 예제](../examples/control_flow.ieum)의 `sum`은 55, `fact`는 120, `oddSum`은 25이며 `app.main`은 175를 반환합니다.

## 소스 위치

토큰·선언·표현식·문장은 파일·행·열 위치를 보관합니다. 표현식과 문장 ID는 여러 파일을 합친 프로그램 안에서도 고유하며 호출 해석에 사용됩니다. 새 렉싱·파싱·타입·값 실행 오류는 `파일:행:열`을 제공합니다. 열은 1부터 시작하는 UTF-8 바이트 위치이며 탭은 1바이트로 셉니다. BOM은 열에 포함하지 않고, CRLF도 한 줄바꿈으로 취급합니다. 구조·의미·렉싱·파싱·실행 오류는 가능한 선언/표현식 위치와 해당 소스 줄·캐럿을 함께 표시합니다. 실행 오류에는 안쪽 함수부터 바깥쪽 진입 함수까지 함수 이름과 호출 위치를 표시하는 `call_stack`이 붙습니다. CLI 사용 오류처럼 소스 노드가 없는 오류에는 소스 줄이 없습니다. 기존 구조 오류의 행 진단은 유지됩니다.

## 현재 지원하지 않는 항목

- 함수 값·클로저, 일반 제네릭, 공유 참조·순환 객체
- 부동소수점, 네이티브 코드 생성, 패키지 레지스트리

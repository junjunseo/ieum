# 이음 전시용 소개 웹사이트

졸업전시 포스터의 QR로 연결할 모바일 중심 소개 페이지입니다. 소개 → 개발 동기 → 실제 구조 검사 데모 → 핵심 문법 → 설명 영상 → 구현 범위 순서로 구성합니다.

## 로컬 실행

Node.js 22.13 이상이 필요합니다. 이 디렉터리에서 실행합니다.

```sh
npm ci
npm run dev
```

기본 주소는 `http://127.0.0.1:5173/`입니다.

```sh
npx tsc --noEmit
npm run build
```

Vinext / React 기반이며 Sites의 Worker 배포 형식을 사용합니다. `.openai/hosting.json`에 등록된 Site ID가 저장되어 있습니다. 현재 호스팅 등록과 빌드 검증을 마친 배포 전 상태입니다. 후속 작업은 기존 Site ID를 재사용하며, 저장소의 승인 절차에 따라 배포 설정을 커밋한 뒤 게시합니다.

## 수정 위치

- `app/page.tsx`: 소개 문구, 문법, FAQ와 전체 구성
- `app/globals.css`: 색상, 가는 선 그래픽 스타일, 모바일 레이아웃
- `app/demonstration.tsx`: 정상 → 연결 추가 → 연결 수정 데모
- `app/site-config.ts`: GitHub 주소 및 YouTube 영상 ID

`youtubeId`는 현재 빈 문자열입니다. YouTube 주소의 영상 ID를 입력하면 준비 중 안내가 개인정보 보호 모드 YouTube 임베드로 바뀝니다. 영상 ID는 `https://www.youtube.com/watch?v=VIDEO_ID`의 `VIDEO_ID` 부분입니다.

## 실제 결과 기반 데모

`public/demo/`에 모듈 5개 예제 소스, 이음의 실제 검사 로그, DOT, Graphviz SVG 및 SHA-256 출처 기록을 포함합니다. 브라우저는 저장된 결과를 전환해서 보여줍니다. 이음 인터프리터를 브라우저에서 실행하지 않습니다.

- 정상: 의존 4개, 구조·의미 검사 통과
- 위반: `module data depends auth`로 의존 1개 추가, 계층 위반 1건
- 수정: 추가한 의존 제거, 정상 예제 결과 재사용

재생성은 저장소 루트에서 이음 실행 파일을 빌드한 뒤 이 사이트 디렉터리에서 실행합니다.

```sh
python scripts/generate-demo.py --ieum ../../build/ieum.exe --dot /absolute/path/to/dot
```

생성기는 실제 실행 종료 코드(정상 0, 위반 1), 위반 진단과 모듈 수를 검사합니다. 그래프 배치는 Graphviz 버전과 폰트에 따라 달라질 수 있습니다. `public/demo/provenance.json`에 생성에 사용한 실행 파일과 결과의 해시가 기록됩니다.

## 화면 검증

- TypeScript 검사 및 배포 빌드 통과
- 실제 이음 실행으로 정상·위반 결과와 모듈 5개 확인
- 데스크톱에서 3단계 전환 및 FAQ 열기·닫기 확인
- 390px 모바일에서 그래프·소스·진단 전환 확인
- 320px 및 390px 화면에서 페이지 가로 넘침 없음
- 브라우저에서 깨진 이미지 및 console warning/error 없음

첫 화면의 선 그래픽은 사이트에 직접 작성한 SVG입니다. 외부 포스터 이미지를 복사하지 않습니다. 웹 글꼴은 Google Fonts의 Noto Sans KR이며, 네트워크가 없으면 시스템 글꼴을 사용합니다.

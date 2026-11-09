"use client";

import { useState } from "react";

const stages = [
  { id: "valid", label: "정상 구조", status: "검사 통과", title: "선언한 방향을 지키는 연결", description: "ui는 orders와 auth를, orders는 data를 사용합니다. 모든 의존 관계가 선언한 계층 규칙을 지킵니다.", message: "구조 검사 통과: 위반 없음", extra: "의미 검사 통과: 위반 없음", source: "five_valid", dependencies: 4 },
  { id: "violation", label: "연결 추가", status: "계층 위반 1건", title: "data가 auth에 의존한다면?", description: "data에 auth 의존을 추가했습니다. 하위 계층에서 상위 계층으로 향하는 연결을 검사기가 찾아 붉은 경로로 표시합니다.", message: "[계층 위반] 하위 계층 'data'가 상위 계층 'auth'에 의존합니다 (4행)", extra: "module data depends auth", source: "five_violation", dependencies: 5 },
  { id: "repaired", label: "연결 수정", status: "검사 통과", title: "잘못된 연결을 제거하면", description: "data의 auth 의존을 제거해 처음의 정상 구조로 되돌렸습니다. 아래는 정상 예제의 실제 검사 결과입니다.", message: "구조 검사 통과: 위반 없음", extra: "의미 검사 통과: 위반 없음", source: "five_valid", dependencies: 4 },
] as const;

export default function Demonstration() {
  const [index, setIndex] = useState(0);
  const stage = stages[index];
  const bad = stage.id === "violation";
  const lines = ["module ui depends orders, auth", "module orders depends data", "module auth depends config", bad ? "module data depends auth" : "module data", "module config", "", "layer ui above orders", "layer ui above auth", "layer orders above data", "layer auth above data", "layer auth above config"];

  return <div className={`demonstration ${bad ? "is-violation" : ""}`}>
    <div className="demo-toolbar"><div className="stage-buttons" role="group" aria-label="데모 단계 선택">{stages.map((item, i) => <button key={item.id} type="button" onClick={() => setIndex(i)} aria-pressed={i === index}><span>0{i + 1}</span>{item.label}</button>)}</div><span className="demo-tag">실제 IEUM 출력</span></div>
    <div className="demo-narrative" aria-live="polite" aria-atomic="true"><h3>{stage.title}</h3><p>{stage.description}</p></div>
    <div className="demo-workspace">
      <div className="code-panel"><div className="panel-header"><span>example.ieum</span><span>소스 코드</span></div><pre aria-label={`${stage.label} 예제 소스`}><code>{lines.map((line, i) => <span className={`code-line ${i === 3 ? "changed-line" : ""}`} key={i}><span className="line-number" aria-hidden="true">{String(i + 1).padStart(2, "0")}</span><span>{line.split(/(module|depends|layer|above)/).map((part, j) => /^(module|depends|layer|above)$/.test(part) ? <b key={j}>{part}</b> : part)}</span>{i === 3 && <span className="line-indicator" aria-hidden="true">←</span>}</span>)}</code></pre></div>
      <div className="graph-panel"><div className="panel-header"><span>MODULE GRAPH</span><span>{stage.dependencies} DEPENDENCIES</span></div><a className="graph-image-link" href={`/demo/${stage.source}.svg`} target="_blank" rel="noreferrer" aria-label={`${stage.label} 그래프 원본 크게 보기`}><img src={`/demo/${stage.source}.svg`} width="640" height="320" alt={bad ? "5개 모듈의 의존 그래프. data에서 auth로 향하는 계층 위반 의존이 빨간색으로 강조되어 있습니다." : "5개 모듈의 정상 의존 그래프. ui에서 orders와 auth, orders에서 data, auth에서 config로 의존합니다."} /><span className="graph-expand" aria-hidden="true">원본 보기 ↗</span></a><div className="graph-legend"><span><i className="legend-dependency" />의존 관계</span><span><i className="legend-layer" />계층 규칙</span><span><i className="legend-violation" />위반 경로</span></div></div>
    </div>
    <div className="diagnostic" aria-live="polite" aria-atomic="true"><div className="diagnostic-status"><span aria-hidden="true">{bad ? "!" : "✓"}</span>{stage.status}</div><div><p>{stage.message}</p><span>{stage.extra}</span></div></div>
    <div className="demo-footnote"><p>이음 실행 파일로 생성한 검사 결과와 그래프입니다.<br />단계를 선택하면 저장된 결과가 전환됩니다.</p><div><a href={`/demo/${stage.source}.ieum`} download>예제 소스 ↓</a><a href={`/demo/${stage.source}.log`} target="_blank" rel="noreferrer">검사 로그 ↗</a></div></div>
  </div>;
}

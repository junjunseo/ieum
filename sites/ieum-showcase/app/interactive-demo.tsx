"use client";

import { useState } from "react";
import { siteAsset } from "./site-config";

const modules = {
  ui: { label: "화면", x: 200, y: 35 },
  orders: { label: "주문", x: 88, y: 118 },
  auth: { label: "로그인", x: 312, y: 118 },
  data: { label: "저장소", x: 88, y: 218 },
  config: { label: "설정", x: 312, y: 218 },
} as const;
type ModuleId = keyof typeof modules;
type ChallengeNode = "auth" | "data";

type Edge = { from: ModuleId; to: ModuleId; path: string };
const baseEdges: readonly Edge[] = [
  { from: "ui", to: "orders", path: "M200 62 V73 H88 V90" },
  { from: "ui", to: "auth", path: "M200 62 V73 H312 V90" },
  { from: "orders", to: "data", path: "M88 145 V190" },
  { from: "auth", to: "config", path: "M312 145 V190" },
];
const outcomes = {
  auth: {
    asset: "five_login_uses_data",
    edge: { from: "auth", to: "data", path: "M254 118 H202 Q188 118 188 132 V204 Q188 218 174 218 H146" },
    code: "module auth depends config, data",
    title: "검사 통과",
    description: "로그인이 저장소를 사용합니다. 상위 모듈에서 하위 모듈로 가는 연결입니다.",
    tone: "accepted",
  },
  data: {
    asset: "five_violation",
    edge: { from: "data", to: "auth", path: "M146 218 H174 Q188 218 188 204 V132 Q188 118 202 118 H254" },
    code: "module data depends auth",
    title: "계층 위반 1건",
    description: "하위 계층인 저장소가 상위 계층인 로그인에 의존합니다.",
    tone: "rejected",
  },
} as const;
type Outcome = (typeof outcomes)[ChallengeNode];

function ModuleGraph({
  selected,
  highlighted,
  onSelect,
  challenge = false,
  outcome,
}: {
  selected: ModuleId | null;
  highlighted?: ReadonlySet<ModuleId>;
  onSelect: (id: ModuleId) => void;
  challenge?: boolean;
  outcome?: Outcome;
}) {
  const tones = ["normal", "muted", "active", "accepted", "rejected"] as const;
  const graphEdges = baseEdges.map(edge => ({
    ...edge,
    tone: !highlighted ? "normal" : highlighted.has(edge.from) && highlighted.has(edge.to) ? "active" : "muted",
  }));
  // Draw highlighted paths last where the two branches share a stem.
  graphEdges.sort((left, right) => Number(left.tone === "active") - Number(right.tone === "active"));

  return (
    <div className="module-board" role="group" aria-label="모듈 의존 관계 그래프">
      <svg viewBox="0 0 400 260" preserveAspectRatio="none" aria-hidden="true">
        <defs>
          {tones.map(tone => (
            <marker key={tone} id={`demo-arrow-${tone}`} className={`module-edge ${tone}`}
              viewBox="0 0 8 8" refX="7" refY="4" markerWidth="6" markerHeight="6" orient="auto">
              <path d="M0 0 L8 4 L0 8 Z" fill="currentColor" />
            </marker>
          ))}
        </defs>
        {[...graphEdges, ...(outcome ? [{ ...outcome.edge, tone: outcome.tone }] : [])].map(edge => (
          <path key={`${edge.from}-${edge.to}`} className={`module-edge ${edge.tone}`}
            d={edge.path} fill="none" stroke="currentColor" vectorEffect="non-scaling-stroke"
            markerEnd={`url(#demo-arrow-${edge.tone})`} />
        ))}
      </svg>
      {(Object.keys(modules) as ModuleId[]).map(id => {
        const { label, x, y } = modules[id];
        const position = { left: `${x / 4}%`, top: `${y * 100 / 260}%` };
        const tone = id === selected ? "selected" : highlighted?.has(id) ? "highlighted" : "";
        const className = `module-node ${tone}`;
        return challenge && id !== "auth" && id !== "data" ? (
          <span key={id} className={`${className} passive`} style={position}>{label}</span>
        ) : (
          <button key={id} type="button" className={className} style={position}
            aria-pressed={id === selected} onClick={() => onSelect(id)}>{label}</button>
        );
      })}
    </div>
  );
}

export function ConnectionChallenge() {
  const [start, setStart] = useState<ChallengeNode | null>(null);
  const [result, setResult] = useState<ChallengeNode | null>(null);
  const outcome = result ? outcomes[result] : undefined;

  function choose(id: ModuleId) {
    if (id !== "auth" && id !== "data") return;
    if (result || start === null) {
      setStart(id);
      setResult(null);
    } else if (id === start) {
      setStart(null);
    } else {
      setResult(start);
    }
  }

  function reverse() {
    if (!result) return;
    const other = result === "auth" ? "data" : "auth";
    setStart(other);
    setResult(other);
  }

  function reset() {
    setStart(null);
    setResult(null);
  }

  return (
    <section className="interactive-panel" aria-labelledby="connection-title">
      <div className="interactive-intro">
        <h3 id="connection-title">직접 연결해 보세요.</h3>
        <p>로그인에 필요한 사용자 정보가 저장소에 있습니다. 두 모듈을 어느 방향으로 연결할까요?</p>
      </div>
      <ModuleGraph challenge selected={start} onSelect={choose} outcome={outcome} />
      <div className={`interactive-feedback ${outcome?.tone ?? ""}`} aria-live="polite" aria-atomic="true">
        {outcome ? <>
          <h4>{modules[outcome.edge.from].label} → {modules[outcome.edge.to].label}<span>{outcome.title}</span></h4>
          <p>{outcome.description}</p>
        </> : (
          <p>{start ? `${modules[start].label}에서 출발합니다. 연결할 모듈을 눌러보세요.` : "출발 모듈과 도착 모듈을 차례로 눌러보세요."}</p>
        )}
      </div>
      {outcome && <>
        <div className="interactive-actions">
          <button type="button" onClick={reverse}>반대 방향으로 연결</button>
          <button type="button" onClick={reset}>다시 선택</button>
        </div>
        <details className="interactive-evidence" key={outcome.asset}>
          <summary>실제 이음 검사 결과 보기</summary>
          <code>{outcome.code}</code>
          <div>
            <a href={siteAsset(`/demo/${outcome.asset}.ieum`)} download>예제 소스</a>
            <a href={siteAsset(`/demo/${outcome.asset}.log`)} target="_blank" rel="noreferrer">검사 로그</a>
          </div>
        </details>
      </>}
    </section>
  );
}

function usersOf(module: ModuleId) {
  const direct = new Set(baseEdges.filter(edge => edge.to === module).map(edge => edge.from));
  const visited = new Set([module, ...direct]);
  const queue = [...direct];

  for (const current of queue) {
    for (const edge of baseEdges) {
      if (edge.to === current && !visited.has(edge.from)) {
        visited.add(edge.from);
        queue.push(edge.from);
      }
    }
  }

  return {
    direct: [...direct],
    indirect: queue.filter(id => !direct.has(id)),
    highlighted: visited,
  };
}

export function DependencyExplorer() {
  const [selected, setSelected] = useState<ModuleId | null>(null);
  const users = selected ? usersOf(selected) : undefined;
  const names = (items: readonly ModuleId[]) => items.map(id => modules[id].label).join(", ") || "없음";

  return (
    <section className="interactive-panel" aria-labelledby="explore-graph-title">
      <div className="interactive-intro">
        <h3 id="explore-graph-title">누가 이 모듈을 쓰나요?</h3>
        <p>모듈 하나를 눌러보세요. 그 모듈을 사용하는 곳과 사이에 거치는 연결이 강조됩니다.</p>
      </div>
      <ModuleGraph selected={selected} highlighted={users?.highlighted} onSelect={setSelected} />
      <div className="interactive-feedback" aria-live="polite" aria-atomic="true">
        {selected && users ? <>
          <h4>{modules[selected].label}에 의존하는 모듈</h4>
          <dl className="module-users">
            <div><dt>직접</dt><dd>{names(users.direct)}</dd></div>
            <div><dt>간접</dt><dd>{names(users.indirect)}</dd></div>
          </dl>
        </> : <p>저장소를 선택하면 주문과 화면이 어떻게 연결되는지 볼 수 있어요.</p>}
      </div>
    </section>
  );
}

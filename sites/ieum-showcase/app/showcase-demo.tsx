"use client";

import { useState } from "react";
import Demonstration from "./demonstration";
import { ConnectionChallenge, DependencyExplorer } from "./interactive-demo";
import "./interactive-demo.css";

const modes = [
  { id: "connection", label: "연결 방향 고르기" },
  { id: "explore", label: "의존 관계 탐색" },
  { id: "example", label: "검사 예제" },
] as const;

export default function ShowcaseDemo() {
  const [mode, setMode] = useState<(typeof modes)[number]["id"]>("connection");

  return <>
    <div className="showcase-modes" role="group" aria-label="데모 모드">
      {modes.map(({ id, label }) => (
        <button key={id} type="button" aria-pressed={mode === id} onClick={() => setMode(id)}>
          {label}
        </button>
      ))}
    </div>
    {mode === "connection" && <ConnectionChallenge />}
    {mode === "explore" && <DependencyExplorer />}
    {mode === "example" && <Demonstration />}
  </>;
}

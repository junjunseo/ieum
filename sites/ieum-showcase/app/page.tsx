import Demonstration from "./demonstration";
import { site } from "./site-config";

function Score() {
  const starts = [200, 20, 70, 155, 20, 120, 20, 95, 20, 85, 20, 75, 155, 55, 95, 20];
  const ends = [960, 870, 960, 960, 840, 960, 610, 960, 920, 960, 835, 940, 960, 925, 960, 940];
  const stops = [760, 170, 560, 410, 250, 875, -1, 180, 795, 490, 765, 260, 645, 855, 555, 220];
  return <svg className="score" viewBox="0 0 980 490" fill="none" aria-hidden="true">
    {starts.map((start, i) => <g key={i}>
      <path d={`M${start} ${25 + i * 28}H${ends[i]}`} className="score-line" />
      {stops[i] >= 0 && <path d={`M${stops[i]} ${20 + i * 28}v10`} className="score-tick" />}
    </g>)}
    <path d="M228 249H470V221H635V193H804" className="score-route" />
    <circle cx="804" cy="193" r="6" className="score-dot" />
  </svg>;
}

function SectionLabel({ number, children }: { number: string; children: React.ReactNode }) {
  return <p className="section-label"><span>{number}</span>{children}</p>;
}

export default function Home() {
  const videoId = site.youtubeId;
  return <>
    <a className="skip-link" href="#main">본문으로 이동</a>
    <header className="site-header wrap">
      <a className="wordmark" href="#main" aria-label="이음 처음으로">이음<span>IEUM</span><i aria-hidden="true" /></a>
      <nav aria-label="주요 메뉴"><a href="#about">소개</a><a href="#demo">데모</a><a href="#film">영상</a><a href={site.github} target="_blank" rel="noreferrer">GitHub <span aria-hidden="true">↗</span></a></nav>
    </header>
    <main id="main">
      <section className="hero wrap" aria-labelledby="hero-title">
        <div className="hero-top"><p className="eyebrow">A LANGUAGE FOR STRUCTURE</p><p className="edition">2026 · GRADUATION PROJECT</p></div>
        <div className="hero-heading"><h1 id="hero-title">설계 규칙을<br />언어에 담다<span className="red">.</span></h1><p>모듈의 의존 관계와 계층을 선언하고,<br className="desktop-break" /> 그 규칙을 어긴 연결을 실행 전에 찾아내는<br className="desktop-break" /> 프로그래밍 언어, 이음.</p></div>
        <figure className="hero-art"><Score /><figcaption><span>연결에는 방향이 있다.</span><span>CONNECTIONS, WITH INTENTION.</span></figcaption></figure>
        <div className="hero-bottom"><a className="text-link" href="#demo">연결의 규칙 살펴보기 <span aria-hidden="true">↓</span></a><span className="small-mono">01 — 05 / SCROLL TO EXPLORE</span></div>
      </section>

      <section className="section wrap about" id="about" aria-labelledby="about-title">
        <SectionLabel number="01">WHY IEUM</SectionLabel>
        <div className="section-grid">
          <h2 id="about-title">기능이 늘어날 때,<br />구조도 지켜지고 있을까?</h2>
          <div className="body-copy">
            <p>바이브코딩으로 기능을 빠르게 추가하다 보면, 여러 모듈의 함수를 호출하는 과정에서 의도하지 않은 연결이 생길 수 있습니다. 이런 연결이 쌓이면 처음 정한 계층을 지키기 어려워지고, 기능 하나를 고칠 때 어디까지 영향이 갈지도 파악하기 어려워집니다.</p>
            <p>이음에서는 <strong>각 모듈이 무엇에 의존할 수 있는지, 어느 계층이 위에 있는지</strong>를 코드에 선언합니다. 이음은 그 선언을 바탕으로 구조를 검사합니다.</p>
          </div>
        </div>
        <div className="principle"><span className="small-mono">THE IDEA</span><p>설계 의도를, 검사할 수 있는 규칙으로.</p><span className="red" aria-hidden="true">↳</span></div>
      </section>

      <section className="demo-section" id="demo" aria-labelledby="demo-title"><div className="wrap">
        <SectionLabel number="02">SEE THE CONNECTIONS</SectionLabel>
        <div className="demo-intro"><div><h2 id="demo-title">하나의 연결이 바뀌면.</h2><p>다섯 개 모듈로 이음의 구조 검사를 살펴보세요.</p></div><span className="small-mono">5 MODULES / 1 CHANGED DEPENDENCY</span></div>
        <Demonstration />
      </div></section>

      <section className="section wrap language" id="language" aria-labelledby="language-title">
        <SectionLabel number="03">RULES IN THE LANGUAGE</SectionLabel>
        <div className="section-grid"><div><h2 id="language-title">연결과 경계를<br />코드로 표현합니다.</h2><p className="section-note">구조 규칙이 별도 문서에만 머무르지 않도록.<br />선언한 관계를 언어의 검사 과정에 포함합니다.</p><a className="text-link" href={`${site.github}/blob/main/docs/GRAMMAR.md`} target="_blank" rel="noreferrer">문법 자세히 보기 <span aria-hidden="true">↗</span></a></div>
          <div className="syntax-list">
            <article><div className="syntax-title"><span>01</span><h3>module</h3><p>역할의 단위</p></div><code><b>module</b> data</code><p>코드가 어느 모듈에 속하는지 선언합니다.</p></article>
            <article><div className="syntax-title"><span>02</span><h3>depends</h3><p>의존 관계</p></div><code>module auth <b>depends</b> data</code><p>해당 모듈이 사용할 다른 모듈을 명시합니다.</p></article>
            <article><div className="syntax-title"><span>03</span><h3>layer</h3><p>계층의 방향</p></div><code><b>layer</b> auth <b>above</b> data</code><p>모듈 사이의 상하 관계를 정합니다. 하위 모듈이 상위 모듈에 직접 또는 간접적으로 의존하면 위반으로 보고합니다.</p></article>
          </div>
        </div>
      </section>

      <section className="film-section" id="film" aria-labelledby="film-title"><div className="wrap">
        <SectionLabel number="04">THE PROJECT FILM</SectionLabel>
        <div className="film-layout">
          <div>
            <h2 id="film-title">이음이 만들어진 이유,<br />그리고 작동하는 모습.</h2>
            <p>모듈 사이에 연결 하나가 추가될 때 어떤 일이 일어나는지, 이음이 그 연결을 어떻게 검사하는지 영상으로 살펴보세요.</p>
            <p><a className="text-link" href={`https://www.youtube.com/watch?v=${videoId}`} target="_blank" rel="noreferrer">YouTube에서 보기</a></p>
          </div>
          <div className="video-frame">
            <iframe src={`https://www.youtube-nocookie.com/embed/${videoId}`} title="이음 프로젝트 소개 및 시연 영상" loading="lazy" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" allowFullScreen />
          </div>
        </div>
      </div></section>

      <section className="section wrap explore" aria-labelledby="explore-title">
        <SectionLabel number="05">UNDER THE HOOD</SectionLabel>
        <div className="section-grid">
          <div>
            <h2 id="explore-title">이음은 어디까지<br />구현됐나요?</h2>
            <p className="section-note">C++17로 구현한 프로그래밍 언어 PoC입니다. 소스 코드와 실행 예제를 GitHub에서 확인할 수 있습니다.</p>
            <a href={site.github} className="repo-link" target="_blank" rel="noreferrer">GitHub에서 살펴보기 <span aria-hidden="true">↗</span></a>
          </div>
          <div className="details-list">
            <details open>
              <summary>지금 무엇을 할 수 있나요?<span aria-hidden="true">+</span></summary>
              <div><p>모듈의 순환 의존, 계층을 거스르는 의존, 허용 범위를 벗어난 함수 호출을 실행 전에 검사합니다. 함수, 조건문, 반복문, 리스트와 레코드, 여러 파일로 나눈 모듈을 지원합니다. 모듈의 의존 관계는 그래프로 확인할 수 있습니다.</p><a href={`${site.github}/blob/main/README.md`} target="_blank" rel="noreferrer">구현 범위와 실행 방법 ↗</a></div>
            </details>
            <details>
              <summary>왜 구조 규칙을 언어에 넣었나요?<span aria-hidden="true">+</span></summary>
              <div><p>기능이 정상적으로 동작해도, 그 기능을 구현하는 과정에서 처음 정한 모듈 경계가 깨질 수 있습니다. 이음에서는 팀이 합의한 구조 규칙이 언어의 기본 검사 과정에 포함됩니다. 여러 사람이 기능을 나눠 만들거나 AI가 생성한 코드를 합칠 때도 같은 기준이 적용되므로, 리뷰에 앞서 어떤 연결이 설계에서 벗어났는지 확인할 수 있습니다.</p></div>
            </details>
            <details>
              <summary>어디까지 검사할 수 있나요?<span aria-hidden="true">+</span></summary>
              <div><p>어떤 역할을 별도 모듈로 나눌지, 모듈이 어느 방향으로 의존해야 할지는 개발자가 정합니다. 이음은 그렇게 선언한 규칙을 기준으로 현재 코드의 연결이 설계와 맞는지 검사합니다.</p></div>
            </details>
          </div>
        </div>
      </section>
    </main>
    <footer className="wrap footer"><div className="footer-brand">이음<span>IEUM</span></div><p>2026 컴퓨터공학 졸업프로젝트<br /><span>C211137 임준서</span></p><a href="#main" className="text-link">처음으로 <span aria-hidden="true">↑</span></a></footer>
  </>;
}

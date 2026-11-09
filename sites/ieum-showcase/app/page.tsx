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
        <div className="hero-heading"><h1 id="hero-title">설계 규칙을<br />언어에 담다<span className="red">.</span></h1><p>모듈의 의존 관계와 계층을 선언하고,<br className="desktop-break" /> 구조적 위반을 실행 전에 검사하는<br className="desktop-break" /> 프로그래밍 언어, 이음.</p></div>
        <figure className="hero-art"><Score /><figcaption><span>연결에는 방향이 있다.</span><span>CONNECTIONS, WITH INTENTION.</span></figcaption></figure>
        <div className="hero-bottom"><a className="text-link" href="#demo">연결의 규칙 살펴보기 <span aria-hidden="true">↓</span></a><span className="small-mono">01 — 05 / SCROLL TO EXPLORE</span></div>
      </section>

      <section className="section wrap about" id="about" aria-labelledby="about-title">
        <SectionLabel number="01">WHY IEUM</SectionLabel>
        <div className="section-grid"><h2 id="about-title">기능이 늘어날 때,<br />구조도 지켜지고 있을까?</h2><div className="body-copy"><p>바이브코딩으로 기능을 추가하고 수정하다 보면, 여러 모듈의 함수를 호출하는 과정에서 의도하지 않은 의존 관계가 생길 수 있습니다. 연결이 쌓일수록 처음 설계한 계층을 지키기는 어려워집니다.</p><p>이음은 <strong>어떤 모듈을 사용할 수 있는지, 어떤 계층이 위에 있는지</strong>를 코드에 선언합니다. 그리고 그 규칙을 어긴 연결을 실행 전에 찾아냅니다.</p></div></div>
        <div className="principle"><span className="small-mono">THE IDEA</span><p>설계 의도를, 검사할 수 있는 규칙으로.</p><span className="red" aria-hidden="true">↳</span></div>
      </section>

      <section className="demo-section" id="demo" aria-labelledby="demo-title"><div className="wrap">
        <SectionLabel number="02">SEE THE CONNECTIONS</SectionLabel>
        <div className="demo-intro"><div><h2 id="demo-title">하나의 연결이 바뀌면.</h2><p>다섯 개 모듈로 살펴보는 이음의 구조 검사.</p></div><span className="small-mono">5 MODULES / 1 CHANGED DEPENDENCY</span></div>
        <Demonstration />
      </div></section>

      <section className="section wrap language" id="language" aria-labelledby="language-title">
        <SectionLabel number="03">RULES IN THE LANGUAGE</SectionLabel>
        <div className="section-grid"><div><h2 id="language-title">연결과 경계를<br />코드로 표현합니다.</h2><p className="section-note">구조 규칙이 별도 문서에만 머무르지 않도록.<br />선언한 관계를 언어의 검사 과정에 포함합니다.</p><a className="text-link" href={`${site.github}/blob/main/docs/GRAMMAR.md`} target="_blank" rel="noreferrer">문법 자세히 보기 <span aria-hidden="true">↗</span></a></div>
          <div className="syntax-list">
            <article><div className="syntax-title"><span>01</span><h3>module</h3><p>역할의 단위</p></div><code><b>module</b> data</code><p>모듈을 선언해 코드가 속할 경계를 정합니다.</p></article>
            <article><div className="syntax-title"><span>02</span><h3>depends</h3><p>허용된 연결</p></div><code>module auth <b>depends</b> data</code><p>사용할 모듈을 명시합니다. 함수 호출은 현재 모듈과 직접 의존한 모듈 안에서만 허용합니다.</p></article>
            <article><div className="syntax-title"><span>03</span><h3>layer</h3><p>계층의 방향</p></div><code><b>layer</b> auth <b>above</b> data</code><p>auth가 data보다 상위 계층임을 선언합니다. data에서 auth로 향하는 직접·간접 의존을 검사합니다.</p></article>
          </div>
        </div>
      </section>

      <section className="film-section" id="film" aria-labelledby="film-title"><div className="wrap">
        <SectionLabel number="04">THE PROJECT FILM</SectionLabel>
        <div className="film-layout"><div><h2 id="film-title">이음이 만들어진 이유,<br />그리고 작동하는 모습.</h2><p>프로젝트의 배경부터 코드 작성,<br />검사 결과까지 하나의 영상으로.</p></div>
          {videoId ? <div className="video-frame"><iframe src={`https://www.youtube-nocookie.com/embed/${videoId}`} title="이음 프로젝트 소개 및 시연 영상" loading="lazy" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" allowFullScreen /></div> : <div className="film-placeholder"><div className="film-lines" aria-hidden="true"><i /><i /><i /><i /><i /></div><span className="film-symbol" aria-hidden="true">↗</span><p>설명 영상 준비 중</p><span>곧 이곳에서 만나보세요.</span><a href="#demo">먼저 인터랙티브 데모 보기 <span aria-hidden="true">↑</span></a></div>}
        </div>
      </div></section>

      <section className="section wrap explore" aria-labelledby="explore-title">
        <SectionLabel number="05">UNDER THE HOOD</SectionLabel>
        <div className="section-grid"><div><h2 id="explore-title">작은 언어에서 시작한<br />구조에 대한 실험.</h2><p className="section-note">C++17로 구현한 프로그래밍 언어 PoC.<br />현재 구현과 한계를 함께 공개합니다.</p><a href={site.github} className="repo-link" target="_blank" rel="noreferrer">GitHub에서 살펴보기 <span aria-hidden="true">↗</span></a></div>
          <div className="details-list">
            <details open><summary>현재 무엇을 할 수 있나요?<span aria-hidden="true">+</span></summary><div><p>모듈 순환 의존, 계층 역행, 선언되지 않은 모듈과 잘못된 호출을 실행 전에 검사합니다. 함수, 조건문과 반복문, 리스트·레코드, 기본 입출력과 여러 파일 모듈을 지원하며 의존 관계를 DOT 그래프로 출력할 수 있습니다.</p><a href={`${site.github}/blob/main/README.md`} target="_blank" rel="noreferrer">구현 범위와 실행 방법 ↗</a></div></details>
            <details><summary>어떤 개발에 가치를 두나요?<span aria-hidden="true">+</span></summary><div><p>여러 모듈을 나누어 개발하면서, 허용된 의존 방향을 지속적으로 확인하고 싶은 경우에 초점을 맞춥니다. 사람이 작성하든 AI의 도움을 받든, 코드 변경을 선언된 구조 규칙에 맞춰 검사한다는 접근입니다.</p></div></details>
            <details><summary>C++나 Java 대신 써야 하나요?<span aria-hidden="true">+</span></summary><div><p>이음은 범용 언어 전체를 대체하는 완성형 제품이 아닙니다. 기존 언어에서도 도구와 규칙으로 구조를 관리할 수 있습니다. 이음은 모듈 의존 관계와 계층 제약을 언어 문법과 기본 검사 과정에 함께 두는 접근을 실험합니다.</p></div></details>
            <details><summary>모든 설계 오류를 막아주나요?<span aria-hidden="true">+</span></summary><div><p>검사는 코드에 선언된 관계와 이음이 지원하는 규칙을 기준으로 합니다. 모든 설계 문제나 코드 품질을 판단하거나, 좋은 구조를 자동으로 만들어주는 것은 아닙니다.</p></div></details>
          </div>
        </div>
      </section>
    </main>
    <footer className="wrap footer"><div className="footer-brand">이음<span>IEUM</span></div><p>2026 컴퓨터공학 졸업프로젝트<br /><span>C211137 임준서</span></p><a href="#main" className="text-link">처음으로 <span aria-hidden="true">↑</span></a></footer>
  </>;
}

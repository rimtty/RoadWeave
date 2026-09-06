'use client';
import {
  DemoDirectionIcon,
  DemoVoiceIcon,
  DemoSignalIcon,
} from './demo/graphics';
import { StudyDemo } from './demo/studio';
import { DemoText } from './demo/bindings';
import type { ReactNode } from 'react';

import './curve-studies.css';

function CurveCard({
  number,
  name,
  idea,
  theme,
  children,
}: {
  number: number;
  name: string;
  idea: string;
  theme: string;
  children: ReactNode;
}) {
  return (
    <article className="cv-card" aria-labelledby={`sample-${number}`}>
      <header>
        <h3 id={`sample-${number}`}>
          <span>{number}</span>
          {name}
        </h3>
        <p>{idea}</p>
      </header>
      <StudyDemo id={number} title={name} screenClass={`cv-screen cv-${theme}`}>
        <div className="cv-face">
          <div className="cv-status">
            <span>
              <i aria-hidden="true" />
              <DemoText template="{voice}" />
            </span>
            <DemoVoiceIcon aria-hidden="true" />
          </div>
          <div className="cv-body">{children}</div>
          <div className="cv-footer">
            <span>
              <DemoText template="{count}人 · {connection}" />
            </span>
            <DemoSignalIcon aria-hidden="true" />
          </div>
        </div>
      </StudyDemo>
    </article>
  );
}
function Speaker() {
  return (
    <div className="cv-speaker">
      <span>
        <DemoText template="{speakerLabel}" />
      </span>
      <strong>
        <DemoText template="{name}" />
      </strong>
    </div>
  );
}
function Distance() {
  return (
    <strong className="cv-distance">
      <DemoText template="{distance}" />
      <span>m</span>
    </strong>
  );
}
function Ahead() {
  return (
    <span className="cv-ahead">
      <DemoDirectionIcon aria-hidden="true" />
      <DemoText template="{direction}" />
    </span>
  );
}

export default function CurveStudies() {
  return (
    <section
      className="cv-section"
      id="curve-studies"
      aria-labelledby="curve-heading"
    >
      <header className="cv-section-heading">
        <div>
          <p className="cv-eyebrow">CHROMA / CURVE STUDIES / 49—54</p>
          <h2 id="curve-heading">色は鮮やかに、境界はやわらかく。</h2>
          <p>黒に溶け込む切り欠きと、なめらかな曲線。6つの新しい表情。</p>
        </div>
        <span className="cv-signature" aria-hidden="true">
          <i />
          <i />
        </span>
      </header>
      <div className="cv-intro">
        <p>
          受信中のAKI・前方120mを共通に比較。54には後方REN・85mも表示します。
        </p>
        <span>240 × 320基準 / 操作デモ</span>
      </div>
      <div className="cv-grid">
        <CurveCard
          number={49}
          name="Cobalt Cove"
          idea="コバルトの面から、黒い入り江へ。"
          theme="cobalt"
        >
          <svg
            className="cv-cobalt-surface"
            viewBox="0 0 240 180"
            preserveAspectRatio="none"
            aria-hidden="true"
          >
            <path
              d="M0 0H240V78Q240 103 215 103H183Q158 103 158 130V150Q158 176 132 176H0Z"
              fill="#3260ff"
            />
          </svg>
          <div className="cv-cobalt-name">
            <Speaker />
          </div>
          <div className="cv-cobalt-distance">
            <Ahead />
            <Distance />
          </div>
          <p className="cv-caption">
            <DemoText template="{target}の音声" />
          </p>
        </CurveCard>
        <CurveCard
          number={50}
          name="Mint Current"
          idea="ミントの曲線で、声と距離をつなぐ。"
          theme="current"
        >
          <Speaker />
          <svg
            className="cv-current-line"
            viewBox="0 0 220 55"
            preserveAspectRatio="none"
            aria-hidden="true"
          >
            <path
              d="M0 42H57C96 42 95 12 134 12H220"
              fill="none"
              stroke="#3ff7ba"
              strokeWidth="5"
              strokeLinecap="round"
            />
            <circle cx="57" cy="42" r="5" fill="#3ff7ba" />
          </svg>
          <div className="cv-current-distance">
            <Ahead />
            <Distance />
          </div>
          <p className="cv-caption">
            <DemoText template="{name}までの距離" />
          </p>
        </CurveCard>
        <CurveCard
          number={51}
          name="Fuchsia Dock"
          idea="鮮やかなピンクに、声の居場所。"
          theme="fuchsia"
        >
          <svg
            className="cv-fuchsia-surface"
            viewBox="0 0 240 230"
            preserveAspectRatio="none"
            aria-hidden="true"
          >
            <path
              d="M240 0V230H227Q191 230 191 194V160Q191 134 165 134H160Q134 134 134 108V92Q134 66 160 66H180Q209 66 209 37V0Z"
              fill="#ff5daf"
            />
          </svg>
          <div className="cv-fuchsia-name">
            <Speaker />
            <span className="cv-voice-dock">
              <DemoVoiceIcon aria-hidden="true" />
            </span>
          </div>
          <div className="cv-fuchsia-distance">
            <Ahead />
            <Distance />
          </div>
          <p className="cv-caption">
            <DemoText template="{target}の音声" />
          </p>
        </CurveCard>
        <CurveCard
          number={52}
          name="Citrus Nest"
          idea="丸い切り欠きに、ライムの明るさ。"
          theme="citrus"
        >
          <div className="cv-citrus-pod">
            <svg
              viewBox="0 0 208 116"
              preserveAspectRatio="none"
              aria-hidden="true"
            >
              <path
                d="M30 0H178Q208 0 208 30V47Q208 65 190 65H181Q162 65 162 85V94Q162 116 140 116H30Q0 116 0 86V30Q0 0 30 0Z"
                fill="#d0ff43"
              />
            </svg>
            <Speaker />
            <span className="cv-citrus-voice">
              <DemoVoiceIcon aria-hidden="true" />
            </span>
          </div>
          <div className="cv-citrus-distance">
            <Ahead />
            <Distance />
          </div>
        </CurveCard>
        <CurveCard
          number={53}
          name="Prism Contour"
          idea="色の移ろいを、細い輪郭に閉じ込める。"
          theme="prism"
        >
          <div className="cv-prism-pod">
            <div>
              <Speaker />
              <DemoVoiceIcon aria-hidden="true" />
            </div>
          </div>
          <div className="cv-prism-distance">
            <Ahead />
            <Distance />
          </div>
          <div className="cv-prism-underline" aria-hidden="true" />
          <p className="cv-caption">
            <DemoText template="{target}の音声" />
          </p>
        </CurveCard>
        <CurveCard
          number={54}
          name="Tidal Pair"
          idea="青とコーラル、前後を分ける曲面。"
          theme="tidal"
        >
          <div className="cv-tidal-front">
            <svg
              viewBox="0 0 208 124"
              preserveAspectRatio="none"
              aria-hidden="true"
            >
              <path
                d="M25 0H183Q208 0 208 25V54Q208 78 184 78H170Q147 78 147 101V106Q147 124 129 124H25Q0 124 0 99V25Q0 0 25 0Z"
                fill="#76c9ff"
              />
            </svg>
            <div className="cv-tidal-who">
              <Ahead />
              <strong>
                <DemoText template="{name}" />
              </strong>
            </div>
            <Distance />
          </div>
          <div className="cv-tidal-rear">
            <span className="cv-ahead">
              <DemoDirectionIcon secondary aria-hidden="true" />
              <DemoText template="{rearDirection}" />
              <b>
                <DemoText template="{rearName}" />
              </b>
            </span>
            <strong>
              <DemoText template="{rearDistance}" />
              <span>m</span>
            </strong>
          </div>
          <p className="cv-caption">
            <DemoVoiceIcon aria-hidden="true" />
            <DemoText template="{voiceSentence}" />
          </p>
        </CurveCard>
      </div>
      <footer className="cv-section-footer">
        <span>COLOR MEETS CURVE.</span>
        <p>
          名前と距離は単色の読みやすい領域に配置。曲線は情報を区切る静止した装飾です。色面を増やした案も含むため、AMOLEDでの消費電力と日光下の視認性は実機で比較します。
        </p>
      </footer>
    </section>
  );
}

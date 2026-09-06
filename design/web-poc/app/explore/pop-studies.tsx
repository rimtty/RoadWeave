'use client';
import {
  DemoDigits,
  DemoDirectionIcon,
  DemoVoiceIcon,
  DemoSignalIcon,
} from './demo/graphics';
import { StudyDemo } from './demo/studio';
import { DemoText } from './demo/bindings';
/* oxlint-disable jsx-a11y/prefer-tag-over-role -- Inline SVG dot numerals use an accessible image role. */
import type { ReactNode } from 'react';
import { ChevronUp } from 'lucide-react';
import './pop-studies.css';

function PopCard({
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
    <article className="pp-card" aria-labelledby={`sample-${number}`}>
      <header>
        <h3 id={`sample-${number}`}>
          <span>{number}</span>
          {name}
        </h3>
        <p>{idea}</p>
      </header>
      <StudyDemo id={number} title={name} screenClass={`pp-screen pp-${theme}`}>
        <div className="pp-face">
          <div className="pp-status">
            <span>
              <i aria-hidden="true" />
              <DemoText template="{voice}" />
            </span>
            <DemoVoiceIcon aria-hidden="true" />
          </div>
          <div className="pp-body">{children}</div>
          <div className="pp-footer">
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

function Distance({ value = '120' }: { value?: string }) {
  return (
    <strong className="pp-distance">
      <DemoText template={value === '85' ? '{rearDistance}' : '{distance}'} />
      <span>m</span>
    </strong>
  );
}
function Ahead() {
  return (
    <span className="pp-ahead">
      <DemoDirectionIcon aria-hidden="true" />
      <DemoText template="{direction}" />
    </span>
  );
}
function Speaker() {
  return (
    <div className="pp-speaker">
      <span>
        <DemoText template="{speakerLabel}" />
      </span>
      <strong>
        <DemoText template="{name}" />
      </strong>
    </div>
  );
}

function PixelDistance() {
  return (
    <div className="pp-pixel-distance">
      <DemoDigits square />
      <span>m</span>
    </div>
  );
}

export default function PopStudies() {
  return (
    <section
      className="pp-section"
      id="pop-studies"
      aria-labelledby="pop-heading"
    >
      <header className="pp-section-heading">
        <div>
          <p className="pp-eyebrow">BLACK EDITION / PLAY SERIES / 41—48</p>
          <h2 id="pop-heading">黒に、ちょっと遊び心。</h2>
          <p>ライム、レモン、コーラル。色と形で、仲間の声に表情を。</p>
        </div>
        <span className="pp-section-mark" aria-hidden="true">
          <i />
          <i />
          <i />
        </span>
      </header>
      <div className="pp-intro">
        <p>
          受信中のAKI・前方120mを共通に、8つの配置で比較。44は後方REN・85mも表示します。
        </p>
        <span>純黒 / 240 × 320基準 / 操作デモ</span>
      </div>
      <div className="pp-grid">
        <PopCard
          number={41}
          name="Lemon Loop"
          idea="レモン色の弧に、距離をひとつ。"
          theme="lemon"
        >
          <div className="pp-lemon-dial">
            <svg viewBox="0 0 200 190" aria-hidden="true">
              <path
                d="M44 166A80 80 0 1 1 156 166"
                fill="none"
                stroke="currentColor"
                strokeWidth="3"
                strokeLinecap="round"
              />
              <circle cx="44" cy="166" r="4" fill="currentColor" />
            </svg>
            <div>
              <Ahead />
              <Distance />
            </div>
          </div>
          <div className="pp-lemon-name">
            <DemoVoiceIcon aria-hidden="true" />
            <strong>
              <DemoText template="{name}" />
            </strong>
            <span>の声</span>
          </div>
        </PopCard>
        <PopCard
          number={42}
          name="Lime Arrow"
          idea="ポンと大きな、前方マーク。"
          theme="lime"
        >
          <div className="pp-lime-arrow">
            <ChevronUp aria-hidden="true" />
            <span>
              <DemoText template="{direction}" />
            </span>
          </div>
          <Distance />
          <div className="pp-lime-name">
            <span>
              <DemoText template="{speakerLabel}" />
            </span>
            <strong>
              <DemoText template="{name}" />
            </strong>
          </div>
        </PopCard>
        <PopCard
          number={43}
          name="Sky Capsule"
          idea="声を包む、水色のカプセル。"
          theme="sky"
        >
          <span className="pp-caption">
            <DemoText template="{speakerLabel}" />
          </span>
          <div className="pp-sky-capsule">
            <strong>
              <DemoText template="{name}" />
            </strong>
            <DemoVoiceIcon aria-hidden="true" />
          </div>
          <div className="pp-sky-distance">
            <Ahead />
            <Distance />
          </div>
          <p className="pp-caption">
            <DemoText template="{target}の音声" />
          </p>
        </PopCard>
        <PopCard
          number={44}
          name="Candy Duo"
          idea="前後を分ける、ふたつの色。"
          theme="candy"
        >
          <div className="pp-candy-front">
            <div>
              <Ahead />
              <strong>
                <DemoText template="{name}" />
              </strong>
            </div>
            <Distance />
          </div>
          <div className="pp-candy-rear">
            <div>
              <span className="pp-ahead">
                <DemoDirectionIcon secondary aria-hidden="true" />
                <DemoText template="{rearDirection}" />
              </span>
              <strong>
                <DemoText template="{rearName}" />
              </strong>
            </div>
            <Distance value="85" />
          </div>
          <p className="pp-caption">
            <DemoVoiceIcon aria-hidden="true" />
            <DemoText template="{voiceSentence}" />
          </p>
        </PopCard>
        <PopCard
          number={45}
          name="Coral Relay"
          idea="名前に添える、コーラルの耳。"
          theme="coral"
        >
          <div className="pp-coral-voice">
            <Speaker />
            <div className="pp-coral-mark" aria-hidden="true">
              <i />
              <i />
              <i />
            </div>
          </div>
          <div className="pp-coral-divider" aria-hidden="true" />
          <div className="pp-coral-distance">
            <Ahead />
            <Distance />
          </div>
          <p className="pp-caption">
            <DemoText template="{target}の音声" />
          </p>
        </PopCard>
        <PopCard
          number={46}
          name="Mint Pixel"
          idea="ミントのピクセル、ピンクの受信印。"
          theme="mint"
        >
          <Speaker />
          <Ahead />
          <PixelDistance />
          <div className="pp-mint-rule" aria-hidden="true">
            <i />
            <i />
            <i />
            <i />
            <i />
          </div>
          <p className="pp-caption">
            <DemoText template="{name}までの距離" />
          </p>
        </PopCard>
        <PopCard
          number={47}
          name="Violet Echo"
          idea="声のまわりに、紫の響き。"
          theme="violet"
        >
          <div className="pp-violet-voice">
            <div className="pp-echo-left" aria-hidden="true">
              <i />
              <i />
            </div>
            <Speaker />
            <div className="pp-echo-right" aria-hidden="true">
              <i />
              <i />
            </div>
          </div>
          <div className="pp-violet-distance">
            <Ahead />
            <Distance />
          </div>
          <p className="pp-caption">
            <DemoText template="{target}の音声" />
          </p>
        </PopCard>
        <PopCard
          number={48}
          name="Rally Frame"
          idea="二色のコーナーで、視線をそろえる。"
          theme="rally"
        >
          <div className="pp-rally-name">
            <span className="pp-caption">
              <DemoText template="{speakerLabel}" />
            </span>
            <strong>
              <DemoText template="{name}" />
            </strong>
            <span className="pp-rally-corner" aria-hidden="true" />
          </div>
          <div className="pp-rally-distance">
            <Ahead />
            <Distance />
          </div>
          <p className="pp-caption">
            <DemoText template="{name}までの距離" />
          </p>
        </PopCard>
      </div>
      <footer className="pp-section-footer">
        <span>PLAYFUL, STILL READABLE.</span>
        <p>
          色に加えて、名前・前後・受信状態を文字で表示。音声を示す棒や弧は送受信に連動します。距離の枠や目盛りは速度・進捗・安全距離を示す計器ではありません。実機の視認性・消費電力は未測定です。
        </p>
      </footer>
    </section>
  );
}

'use client';
import {
  DemoDigits,
  DemoDirectionIcon,
  DemoVoiceIcon,
  DemoSignalIcon,
} from './demo/graphics';
import { StudyDemo } from './demo/studio';
import { DemoText } from './demo/bindings';
/* oxlint-disable jsx-a11y/prefer-tag-over-role -- Inline SVG dot numerals need an accessible image role; HTML img cannot contain their geometry. */
import type { ReactNode } from 'react';

import './amoled-studies.css';

function OledCard({
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
    <article className="oe-card" aria-labelledby={`sample-${number}`}>
      <header>
        <h3 id={`sample-${number}`}>
          <span>{number}</span>
          {name}
        </h3>
        <p>{idea}</p>
      </header>
      <StudyDemo id={number} title={name} screenClass={`oe-screen oe-${theme}`}>
        <div className="oe-face">
          <div className="oe-status">
            <span>
              <i aria-hidden="true" />
              <DemoText template="{voice}" />
            </span>
            <DemoVoiceIcon aria-hidden="true" />
          </div>
          <div className="oe-body">{children}</div>
          <div className="oe-footer">
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

function Name({ label = '話している人' }: { label?: string }) {
  return (
    <div className="oe-name">
      <span>
        <DemoText
          template={label === '話している人' ? '{speakerLabel}' : label}
        />
      </span>
      <strong>
        <DemoText template="{name}" />
      </strong>
    </div>
  );
}
function Ahead() {
  return (
    <span className="oe-ahead">
      <DemoDirectionIcon aria-hidden="true" />
      <DemoText template="{direction}" />
    </span>
  );
}
function Metres() {
  return (
    <strong className="oe-metres">
      <DemoText template="{distance}" />
      <span>m</span>
    </strong>
  );
}
function DotDistance() {
  return (
    <div className="oe-dot-distance">
      <DemoDigits />
      <span>m</span>
    </div>
  );
}

export default function AmoledStudies() {
  return (
    <section
      className="oe-section"
      id="amoled-studies"
      aria-labelledby="amoled-heading"
    >
      <header className="oe-section-heading">
        <div>
          <p className="oe-eyebrow">BLACK EDITION / 33—40</p>
          <h2 id="amoled-heading">黒を余白に、声を鮮明に。</h2>
          <p>名前と距離を大きく。色は、必要なところに少しだけ。</p>
        </div>
        <span className="oe-edition">08 NEW STUDIES</span>
      </header>
      <div className="oe-notes">
        <p>
          <b>純黒の背景 / 大きな文字 / 小さなアクセント</b>
          <span>
            240 × 320基準の縦画面。全案でAKIから受信中・前方120mを表示。
          </span>
        </p>
        <p>
          AMOLEDを想定した操作デモです。日光下の視認性と消費電力は実機での評価が必要です。
        </p>
      </div>
      <div className="oe-grid">
        <OledCard
          number={33}
          name="Noir"
          idea="白い文字と、何もない余白。"
          theme="noir"
        >
          <Name />
          <div className="oe-noir-distance">
            <Ahead />
            <Metres />
          </div>
          <p className="oe-caption">
            <DemoText template="{target}の音声" />
          </p>
        </OledCard>
        <OledCard
          number={34}
          name="Meridian"
          idea="細い青線で、読む順序をつくる。"
          theme="meridian"
        >
          <Name />
          <div className="oe-meridian-axis">
            <Ahead />
            <Metres />
          </div>
        </OledCard>
        <OledCard
          number={35}
          name="Interval"
          idea="距離を上に、声を下に。"
          theme="interval"
        >
          <div className="oe-interval-distance">
            <Ahead />
            <Metres />
          </div>
          <div className="oe-interval-speaker">
            <Name />
            <DemoVoiceIcon aria-hidden="true" />
          </div>
        </OledCard>
        <OledCard
          number={36}
          name="Trace"
          idea="一本の軌跡と、前を走る仲間。"
          theme="trace"
        >
          <div className="oe-trace-head">
            <span>
              <DemoText template="{direction}の仲間" />
            </span>
            <strong>
              <DemoText template="{name}" />
            </strong>
          </div>
          <div className="oe-trace-distance">
            <div className="oe-trace-path" aria-hidden="true">
              <DemoDirectionIcon />
              <i />
            </div>
            <Metres />
          </div>
          <p className="oe-caption">
            <DemoText template="{voiceSentence}" />
          </p>
        </OledCard>
        <OledCard
          number={37}
          name="Ember"
          idea="琥珀の輪郭で、声を包む。"
          theme="ember"
        >
          <div className="oe-ember-ring">
            <Name />
          </div>
          <div className="oe-ember-distance">
            <Ahead />
            <Metres />
          </div>
        </OledCard>
        <OledCard
          number={38}
          name="Index"
          idea="左端を揃えた、静かな赤。"
          theme="index"
        >
          <Name />
          <div className="oe-index-rule" aria-hidden="true">
            <i />
          </div>
          <div className="oe-index-distance">
            <Ahead />
            <Metres />
          </div>
          <p className="oe-caption">
            <DemoText template="{target}の音声" />
          </p>
        </OledCard>
        <OledCard
          number={39}
          name="Raster"
          idea="点は数字に、名前はくっきり。"
          theme="raster"
        >
          <Name />
          <Ahead />
          <DotDistance />
          <p className="oe-caption">
            <DemoText template="{name}までの距離" />
          </p>
        </OledCard>
        <OledCard
          number={40}
          name="Duplex"
          idea="前と後ろを、2本の水平線に。"
          theme="duplex"
        >
          <div className="oe-duplex-front">
            <div>
              <span className="oe-ahead">
                <DemoDirectionIcon aria-hidden="true" />
                <DemoText template="{direction}" />
              </span>
              <strong>
                <DemoText template="{name}" />
              </strong>
            </div>
            <Metres />
          </div>
          <div className="oe-duplex-rear">
            <div>
              <span className="oe-ahead">
                <DemoDirectionIcon secondary aria-hidden="true" />
                <DemoText template="{rearDirection}" />
              </span>
              <strong>
                <DemoText template="{rearName}" />
              </strong>
            </div>
            <strong className="oe-metres">
              <DemoText template="{rearDistance}" />
              <span>m</span>
            </strong>
          </div>
          <p className="oe-caption">
            <DemoVoiceIcon aria-hidden="true" />
            <DemoText template="{voiceSentence}" />
          </p>
        </OledCard>
      </div>
      <div className="oe-section-footer">
        <span>ROADWEAVE / BLACK EDITION</span>
        <p>
          大きな色面・発光する影・点滅を省き、黒地に情報だけを残しました。背景は全案
          #000000。
        </p>
      </div>
    </section>
  );
}

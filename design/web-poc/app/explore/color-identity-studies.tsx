'use client';

import { StudyDemo } from './demo/studio';
import { DemoText } from './demo/bindings';
import type { CSSProperties, ReactNode } from 'react';
import './color-identity-studies.css';

const colors = {
  acid: '#c6ff00',
  pink: '#ff16a5',
  blue: '#397dff',
  orange: '#ff6b00',
};
type Member = keyof typeof colors;
const labels = {
  acid: 'アシッドライム',
  pink: 'ホットピンク',
  blue: 'エレクトリックブルー',
  orange: 'オレンジ',
};

function Distance({
  direction: _direction = '前方',
  value = '120',
}: {
  direction?: string;
  value?: string;
}) {
  return (
    <div className="ci-distance">
      <span>
        <DemoText
          template={value === 'secondary' ? '{rearDirection}' : '{direction}'}
        />
      </span>
      <strong>
        <DemoText
          template={value === 'secondary' ? '{rearDistance}' : '{distance}'}
        />
        <small>m</small>
      </strong>
    </div>
  );
}
function Card({
  number,
  title,
  idea,
  theme,
  member = 'acid',
  children,
}: {
  number: number;
  title: string;
  idea: string;
  theme: string;
  member?: Member;
  children: ReactNode;
}) {
  return (
    <article className="ci-card" aria-labelledby={`sample-${number}`}>
      <header>
        <h3 id={`sample-${number}`}>
          <span>{number}</span>
          {title}
        </h3>
        <p>{idea}</p>
      </header>
      <StudyDemo
        id={number}
        title={title}
        screenClass={`ci-screen ci-${theme}`}
        style={{ '--ci-accent': colors[member] } as CSSProperties}
      >
        <div className="ci-face">
          <div className="ci-status">
            <span>
              <DemoText template="{voice}" />
              <span className="ci-sr-only">
                ：<DemoText template="{name}" />
                のメンバー
              </span>
            </span>
            <span>
              <DemoText template="{count}人" />
            </span>
          </div>
          <div className="ci-body">{children}</div>
          <div className="ci-footer">
            <span>
              <i aria-hidden="true" />
              自分<span className="ci-sr-only">：オレンジ</span>
            </span>
            <span>
              <DemoText template="{connection}" />
            </span>
          </div>
        </div>
      </StudyDemo>
    </article>
  );
}
export default function ColorIdentityStudies() {
  return (
    <section
      className="ci-section"
      id="color-identity-studies"
      aria-labelledby="color-identity-heading"
    >
      <header className="ci-section-heading">
        <div>
          <p className="ci-eyebrow">COLOR ID / REWORKED 55—60</p>
          <h2 id="color-identity-heading">黒に、強い色をひとつ。</h2>
          <p>誰の声かは色で。距離と状態は、大きく。</p>
        </div>
        <span className="ci-section-badge">NEON / MINIMAL</span>
      </header>
      <div className="ci-palette" aria-label="メンバーの識別色">
        {(['acid', 'pink', 'blue', 'orange'] as const).map((member) => (
          <span key={member}>
            <i style={{ background: colors[member] }} aria-hidden="true" />
            {labels[member]}
            {member === 'orange' ? '（自分）' : ''}
          </span>
        ))}
      </div>
      <div className="ci-grid">
        <Card
          number={55}
          title="Acid Strip"
          idea="一枚の色帯と、余白に置く大きな距離。"
          theme="strip"
        >
          <div className="ci-color-strip" aria-hidden="true" />
          <Distance />
          <p className="ci-caption">
            <DemoText template="{target}の音声" />
          </p>
        </Card>
        <Card
          number={56}
          title="Side Current"
          idea="声の主を示す色を、画面の片側へ。"
          theme="rail"
        >
          <div className="ci-rail-distance">
            <Distance />
          </div>
          <p className="ci-caption">
            <DemoText template="{target}の音声" />
          </p>
        </Card>
        <Card
          number={57}
          title="Hot Pink Dock"
          idea="鮮烈なピンクと、丸く沈み込む黒い面。"
          theme="dock"
          member="pink"
        >
          <div className="ci-color-panel">
            <Distance direction="後方" value="85" />
          </div>
          <p className="ci-caption">
            <DemoText template="{target}の音声" />
          </p>
        </Card>
        <Card
          number={58}
          title="Two Lanes"
          idea="前後の仲間を、色と距離の二段で。"
          theme="pair"
        >
          <div className="ci-pair-primary">
            <Distance />
            <span className="ci-caption">
              <DemoText template="{voice}" />
            </span>
          </div>
          <div
            className="ci-pair-secondary"
            style={
              { '--ci-accent': 'var(--demo-other-color)' } as CSSProperties
            }
          >
            <span className="ci-sr-only">
              <DemoText template="{rearName}のメンバー" />
            </span>
            <Distance direction="後方" value="secondary" />
          </div>
        </Card>
        <Card
          number={59}
          title="Electric Cove"
          idea="青い曲面を、黒い情報面につなぐ。"
          theme="field"
          member="blue"
        >
          <div className="ci-color-panel">
            <Distance direction="後方" value="240" />
          </div>
          <p className="ci-caption">
            <DemoText template="{target}の音声" />
          </p>
        </Card>
        <Card
          number={60}
          title="Quiet Line"
          idea="位置が途切れても、話者の色はそのまま。"
          theme="hold"
        >
          <div className="ci-color-strip" aria-hidden="true" />
          <Distance direction="位置未更新" value="—" />
          <p className="ci-caption">
            <DemoText template="{connection}" />
          </p>
        </Card>
      </div>
      <details className="ci-product-note">
        <summary>色の割り当てと、将来のアプリ設定</summary>
        <div>
          <p>
            人物ごとの図形やアバターは使わず、同じ形式の色帯・色面で識別します。色はメンバーに固定し、グループ内の全端末で同じ対応を保つ想定です。オレンジは、このサンプルの端末のユーザーに割り当てた色です。状態は「受信中」「位置未更新」の文字で伝えます。
          </p>
          <p>
            本体にソフトウェアキーボードは設けません。任意の表示名が必要な場合は、停車中にBLE接続したスマートフォンアプリで設定する方式を製品化前に検討します。色の重複・再参加・グループ人数の上限や、実際の表示環境での識別性も別途検証する項目です。模擬操作と車列・GPS画面を試せます。実際の通信やBLEアプリは実装していません。
          </p>
        </div>
      </details>
      <footer className="ci-section-footer">
        <span>LESS TO READ. MORE TO SEE.</span>
        <p>
          240 ×
          320基準。55・56・58はライムの仲間が前方120m、57はピンクの仲間が後方85m、59はブルーの仲間が後方240m。60はライムの仲間の位置未更新です。
        </p>
      </footer>
    </section>
  );
}

'use client';
import {
  DemoDigits,
  DemoDirectionIcon,
  DemoVoiceIcon,
  DemoSignalIcon,
} from './demo/graphics';
import { StudyDemo } from './demo/studio';
import { DemoText } from './demo/bindings';
/* oxlint-disable jsx-a11y/prefer-tag-over-role -- Inline SVG dot numerals are accessible images, not HTML image elements. */
import type { ReactNode } from 'react';
import { Radio } from 'lucide-react';
import './driving-studies.css';

function DriveCard({
  number,
  name,
  purpose,
  theme,
  state: _state = '受信中',
  children,
  footer: _footer = '4人接続',
  note,
}: {
  number: number;
  name: string;
  purpose: string;
  theme: string;
  state?: string;
  children: ReactNode;
  footer?: string;
  note: string;
}) {
  return (
    <article className="dv-card" aria-labelledby={`sample-${number}`}>
      <header className="dv-card-label">
        <h3 id={`sample-${number}`}>
          <span>{number}</span>
          {name}
        </h3>
        <p>{purpose}</p>
      </header>
      <StudyDemo id={number} title={name} screenClass={`dv-screen dv-${theme}`}>
        <div className="dv-face">
          <div className="dv-status">
            <span>
              <DemoVoiceIcon aria-hidden="true" />
              <DemoText template="{voice}" />
            </span>
            <span className="dv-status-mark" aria-hidden="true" />
          </div>
          <div className="dv-content">{children}</div>
          <div className="dv-footer">
            <span>
              <DemoText template="{count}人 · {connection}" />
            </span>
            <DemoSignalIcon aria-hidden="true" />
          </div>
        </div>
      </StudyDemo>
      <p className="dv-card-note">{note}</p>
    </article>
  );
}

function Distance({
  value = '120',
  direction: _direction = '前方',
  name,
  large = false,
}: {
  value?: string;
  direction?: '前方' | '後方';
  name?: string;
  large?: boolean;
}) {
  return (
    <div className={`dv-distance ${large ? 'dv-distance-large' : ''}`}>
      <span className="dv-direction">
        <DemoDirectionIcon secondary={value === '85'} aria-hidden="true" />
        <DemoText
          template={value === '85' ? '{rearDirection}' : '{direction}'}
        />
        {name && (
          <b>
            <DemoText template={name === 'REN' ? '{rearName}' : '{name}'} />
          </b>
        )}
      </span>
      <strong>
        <DemoText template={value === '85' ? '{rearDistance}' : '{distance}'} />
        <span>m</span>
      </strong>
    </div>
  );
}

function Speaker({ japanese = false }: { japanese?: boolean }) {
  return (
    <div className="dv-speaker">
      <span>
        <DemoText template="{speakerLabel}" />
      </span>
      <strong>
        <DemoText template={japanese ? '{japanese}' : '{name}'} />
      </strong>
    </div>
  );
}

function DriveDigits({ value = '120' }: { value?: string }) {
  return (
    <DemoDigits
      className="dv-digits"
      field={value === '85' ? 'rearDistance' : 'distance'}
      radius={2.3}
    />
  );
}

export default function DrivingStudies() {
  return (
    <section
      className="dv-section"
      id="driving-studies"
      aria-labelledby="driving-heading"
    >
      <header className="dv-heading">
        <div>
          <p className="dv-eyebrow">DRIVING STUDIES / 17—32</p>
          <h2 id="driving-heading">視線は短く、情報は大きく。</h2>
          <p>運転中の確認を考えた、もう16の表情。</p>
        </div>
        <span className="dv-new">NEW / 16 DESIGNS</span>
      </header>
      <div className="dv-principles" aria-label="走行用UIの情報優先順位">
        <div>
          <span>01 / 状態</span>
          <strong>届いている？</strong>
          <p>受信・送信・待受・未接続を、文字と記号で区別。</p>
        </div>
        <div>
          <span>02 / 発話者</span>
          <strong>誰の声？</strong>
          <p>短い名前を大きく。アイコンや顔を覚えなくても読める。</p>
        </div>
        <div>
          <span>03 / 相対距離</span>
          <strong>誰が、どちらに？</strong>
          <p>前後・相手・mを数字と一緒に。位置未更新なら数値を隠す。</p>
        </div>
        <div>
          <span>04 / 操作</span>
          <strong>画面を探さない。</strong>
          <p>走行中は物理PTTと音量。参加・設定は停車中の画面へ。</p>
        </div>
      </div>
      <div className="dv-reading-spec">
        <p>
          <b>240 × 320 の縦画面を基準に設計</b>
          <span>
            主情報 44–72px相当 / 状態 24px / 補助情報
            14–18px。表示幅に比例して拡大。
          </span>
        </p>
        <p>
          受信例：AKIが前方120m。25–28は待受・送信・切断・位置未更新の別場面です。
        </p>
      </div>

      <div className="dv-family-heading">
        <span>A / VOICE FIRST</span>
        <h3>誰の声か、すぐ分かる。</h3>
        <p>17–20 · 発話者を最優先にした4案</p>
      </div>
      <div className="dv-grid">
        <DriveCard
          number={17}
          name="Clarity"
          purpose="白地 × 名前を中央に"
          theme="clarity"
          note="余白を名前の周囲に集め、状態→名前→距離を縦に読む。"
        >
          <Speaker />
          <Distance />
          <div className="dv-receive-line">
            <DemoVoiceIcon aria-hidden="true" />
            <DemoText template="{target}の音声" />
          </div>
        </DriveCard>
        <DriveCard
          number={18}
          name="Broadcast"
          purpose="暗色 × 太い帯"
          theme="broadcast"
          note="上部の受信帯を固定。名前と距離の2段で読み終える。"
        >
          <div className="dv-broadcast-name">
            <span>
              <DemoText template="{speakerLabel}" />
            </span>
            <strong>
              <DemoText template="{name}" />
            </strong>
          </div>
          <div className="dv-broadcast-distance">
            <DemoDirectionIcon aria-hidden="true" />
            <div>
              <span>
                <DemoText template="{direction}" />
              </span>
              <strong>
                <DemoText template="{distance}" />
                <span>m</span>
              </strong>
            </div>
          </div>
        </DriveCard>
        <DriveCard
          number={19}
          name="Voice Seal"
          purpose="名前を囲う静かな輪"
          theme="seal"
          note="輪は静止した目印。装飾の波形を省き、名前を大きく保つ。"
        >
          <div className="dv-seal-ring">
            <Speaker />
          </div>
          <Distance />
          <p className="dv-plain-label">
            <DemoText template="{target}の音声" />
          </p>
        </DriveCard>
        <DriveCard
          number={20}
          name="Callsign"
          purpose="日本語の呼び名 × 左揃え"
          theme="callsign"
          note="短い日本語の呼び名を太く表示。左端を揃え、目の移動を減らす。"
        >
          <Speaker japanese />
          <div className="dv-callsign-rule" />
          <Distance />
          <span className="dv-call-target">
            <DemoText template="{target}の音声" />
          </span>
        </DriveCard>
      </div>

      <div className="dv-family-heading">
        <span>B / DISTANCE FIRST</span>
        <h3>前か、後ろか。何mか。</h3>
        <p>21–24 · 相手との距離を主役にした4案</p>
      </div>
      <div className="dv-grid">
        <DriveCard
          number={21}
          name="Lead"
          purpose="大きな数字 × ライム"
          theme="lead"
          note="120mを最大に。前方とAKIを同じ領域に置き、対象を明確に。"
        >
          <div className="dv-lead-who">
            <span>
              <DemoText template="{speakerLabel}" />
            </span>
            <strong>
              <DemoText template="{name}" />
            </strong>
          </div>
          <Distance large />
          <p className="dv-plain-label">
            <DemoText template="{name}までの距離" />
          </p>
        </DriveCard>
        <DriveCard
          number={22}
          name="Front / Rear"
          purpose="前後2段 × モノクロ"
          theme="front-rear"
          note="前方と後方を固定の上下段に。受信中の相手には音声記号を添える。"
        >
          <div className="dv-pair-a">
            <Distance name="AKI" />
            <span className="dv-pair-voice">
              <DemoVoiceIcon aria-hidden="true" />
              <DemoText template="{voiceSentence}" />
            </span>
          </div>
          <div className="dv-pair-b">
            <Distance name="REN" direction="後方" value="85" />
          </div>
        </DriveCard>
        <DriveCard
          number={23}
          name="In Line"
          purpose="縦の隊列 × 深い青"
          theme="in-line"
          note="上に前方、中央に自分、下に後方。地図の回転や細かな目盛りを省く。"
        >
          <div className="dv-line-peer">
            <Distance name="AKI" />
          </div>
          <div className="dv-line-self">
            <i />
            <span>あなた</span>
          </div>
          <div className="dv-line-peer">
            <Distance name="REN" direction="後方" value="85" />
          </div>
        </DriveCard>
        <DriveCard
          number={24}
          name="Direction"
          purpose="前方記号 × アンバー"
          theme="waymark"
          note="方向の記号を左に独立。下部には発話者だけを強く残す。"
        >
          <div className="dv-direction-main">
            <DemoDirectionIcon aria-hidden="true" />
            <div>
              <span>
                <DemoText template="{direction}" />
              </span>
              <strong>
                <DemoText template="{distance}" />
                <span>m</span>
              </strong>
            </div>
          </div>
          <div className="dv-direction-name">
            <DemoVoiceIcon aria-hidden="true" />
            <strong>
              <DemoText template="{name}" />
            </strong>
          </div>
          <p className="dv-plain-label">
            <DemoText template="{speakerLabel}" />
          </p>
        </DriveCard>
      </div>

      <div className="dv-family-heading">
        <span>C / STATE FIRST</span>
        <h3>話せる、送っている、届かない。</h3>
        <p>25–28 · 状態の変化を読み違えない4案</p>
      </div>
      <div className="dv-grid">
        <DriveCard
          number={25}
          name="Ready"
          purpose="待受 × 接続の確認"
          theme="ready"
          state="待受"
          note="名前の代わりに次の送信先。操作説明は1行、画面内にボタンは置かない。"
        >
          <div className="dv-ready-symbol">
            <DemoVoiceIcon aria-hidden="true" />
          </div>
          <span className="dv-state-label">
            <DemoText template="{voice}" />
          </span>
          <strong className="dv-state-title">
            <DemoText template="{stateFocus}" />
          </strong>
          <div className="dv-state-message">
            <Radio aria-hidden="true" />
            <span>
              <DemoText template="{hint}" />
            </span>
          </div>
        </DriveCard>
        <DriveCard
          number={26}
          name="On Air"
          purpose="送信中 × 自分の声"
          theme="on-air"
          state="送信中"
          note="受信のAKI表示と明確に分離。送信先と終了方法を同時に示す。"
        >
          <DemoVoiceIcon className="dv-on-air-icon" aria-hidden="true" />
          <span className="dv-state-label">
            <DemoText template="{voice}" />
          </span>
          <strong className="dv-state-title">
            <DemoText template="{stateFocus}" />
          </strong>
          <div className="dv-tx-indicator">
            <i />
            <i />
            <i />
            <i />
            <i />
          </div>
          <p className="dv-state-message">
            <DemoText template="{hint}" />
          </p>
        </DriveCard>
        <DriveCard
          number={27}
          name="Link Lost"
          purpose="未接続 × 送信不可"
          theme="link-lost"
          state="未接続"
          footer="再接続中"
          note="古い名前と距離を残さず、送信できない事実を最優先にする。"
        >
          <DemoVoiceIcon className="dv-offline-icon" aria-hidden="true" />
          <strong className="dv-state-title">
            <DemoText template="{voice}" />
          </strong>
          <p className="dv-offline-copy">
            <DemoText template="{voiceSentence}" />
          </p>
          <div className="dv-offline-rule" />
          <p className="dv-plain-label">
            <DemoText template="{hint}" />
          </p>
        </DriveCard>
        <DriveCard
          number={28}
          name="Position Hold"
          purpose="音声は接続 × 位置未更新"
          theme="position-hold"
          footer="音声は接続中"
          note="音声の接続と位置の鮮度を分離。未更新の120mを現位置として見せない。"
        >
          <Speaker />
          <div className="dv-stale-block">
            <span>
              <DemoDirectionIcon aria-hidden="true" />
              <DemoText template="{direction}" />
            </span>
            <strong>
              <DemoText template="{distance}" />
              <span>m</span>
            </strong>
            <p>
              <DemoText template="{positionHint}" />
            </p>
          </div>
        </DriveCard>
      </div>

      <div className="dv-family-heading">
        <span>D / DOT, REFINED</span>
        <h3>ドットの美しさを、読みやすく。</h3>
        <p>29–32 · 密度を減らし、大きな数字に絞った4案</p>
      </div>
      <div className="dv-grid">
        <DriveCard
          number={29}
          name="Dot / Bold"
          purpose="白い数字 × 赤い受信印"
          theme="dot-bold"
          note="太い点の数字だけを使う。発話者・方向・状態は連続した通常文字に。"
        >
          <Speaker />
          <span className="dv-dot-direction">
            <DemoDirectionIcon aria-hidden="true" />
            <DemoText template="{direction}" />
          </span>
          <div className="dv-dot-number">
            <DriveDigits />
            <span>m</span>
          </div>
          <div className="dv-dot-divider" />
          <p className="dv-plain-label">
            <DemoText template="{name}までの距離" />
          </p>
        </DriveCard>
        <DriveCard
          number={30}
          name="Dot / Daylight"
          purpose="白地のドット × 青い文字"
          theme="dot-daylight"
          note="明るい背景でもドットを比較。目盛りやグリッドを外し、数字の輪郭を残す。"
        >
          <div className="dv-daylight-name">
            <strong>
              <DemoText template="{name}" />
            </strong>
            <span>
              <DemoText template="{speakerLabel}" />
            </span>
          </div>
          <span className="dv-dot-direction">
            <DemoDirectionIcon aria-hidden="true" />
            <DemoText template="{direction}" />
          </span>
          <div className="dv-dot-number">
            <DriveDigits />
            <span>m</span>
          </div>
          <p className="dv-plain-label">
            <DemoText template="{target}の音声" />
          </p>
        </DriveCard>
        <DriveCard
          number={31}
          name="Dot / Amber"
          purpose="暗色の琥珀色 × 距離中心"
          theme="dot-amber"
          note="暗色用の色を比較する案。実機の輝度は別に調整し、色だけで状態を示さない。"
        >
          <div className="dv-amber-target">
            <DemoDirectionIcon aria-hidden="true" />
            <span>
              <DemoText template="{direction}" />
            </span>
            <strong>
              <DemoText template="{name}" />
            </strong>
          </div>
          <div className="dv-dot-number">
            <DriveDigits />
            <span>m</span>
          </div>
          <div className="dv-amber-voice">
            <DemoVoiceIcon aria-hidden="true" />
            <span>
              <DemoText template="{voiceSentence}" />
            </span>
          </div>
        </DriveCard>
        <DriveCard
          number={32}
          name="Dot / Frame"
          purpose="連続文字の名前 × 点の枠"
          theme="dot-frame"
          note="ドットを枠へ移す案。名前と距離は太い通常文字で、読みやすさを比較する。"
        >
          <div className="dv-dot-frame-box">
            <Speaker />
          </div>
          <Distance large />
          <p className="dv-plain-label">
            <DemoText template="{target}の音声" />
          </p>
        </DriveCard>
      </div>
      <details className="dv-rationale">
        <summary>設計の根拠と、実機で確認すること</summary>
        <div>
          <p>
            車載表示に関するTransport
            Canadaのガイドラインを参考に、情報量を減らす・状態を明確にする・走行中の操作を絞る方針を採用しました。文字サイズと16案の構成はRoadWeave向けの設計仮説です。
          </p>
          <p>
            画面は比較用の操作デモです。実際の見やすさは、画面の物理サイズ・取付位置・視距離・日光や夜間の輝度・振動で変わります。まず停車状態とシミュレータで、発話者・前後・距離・送受信状態の識別を評価します。このボードは走行中の安全性を検証したものではありません。
          </p>
          <p>
            色に頼らず文字と記号を併記し、自動切替・点滅・スクロールを省略。短い呼び名を停車中に設定する想定です。距離は仮の相対位置で、車間距離の安全判定には使いません。
          </p>
          <a
            href="https://tc.canada.ca/en/road-transportation/stay-safe-when-driving/transport-canada-guidelines-limit-distraction-visual-displays-vehicles"
            target="_blank"
            rel="noreferrer"
          >
            参考：Transport Canada — Visual displays in vehicles ↗
          </a>
        </div>
      </details>
    </section>
  );
}

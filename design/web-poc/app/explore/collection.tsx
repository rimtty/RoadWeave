'use client';
import {
  DemoDigits,
  DemoDirectionIcon,
  DemoVoiceIcon,
  DemoSignalIcon,
  DemoPositionGroup,
} from './demo/graphics';
import { StudyDemo } from './demo/studio';
import { DemoText, useView, useStudy } from './demo/bindings';
/* oxlint-disable jsx-a11y/prefer-tag-over-role -- Inline SVG diagrams need an accessible image role; an HTML img cannot contain their geometry. */

import Link from 'next/link';
import { useState } from 'react';
import { StudyPageProvider, StudyPageTabs, type StudyPage } from './demo/pages';
import type { CSSProperties, ReactNode } from 'react';
import {
  ArrowDown,
  ArrowLeft,
  BatteryMedium,
  Check,
  ChevronRight,
  Headphones,
  MapPin,
  Mic,
  Navigation,
  Radio,
  Users,
} from 'lucide-react';
import './explore.css';
import { fleetView } from './demo/fleet-model';
import DrivingStudies from './driving-studies';
import AmoledStudies from './amoled-studies';
import PopStudies from './pop-studies';
import CurveStudies from './curve-studies';
import ColorIdentityStudies from './color-identity-studies';

function Sample({
  number,
  name,
  idea,
  tone,
  children,
}: {
  number: string;
  name: string;
  idea: string;
  tone: string;
  children: ReactNode;
}) {
  const related = (
    { together: 'Circle', apex: 'Pulse', orbit: 'Compass' } as Record<
      string,
      string | undefined
    >
  )[tone];
  return (
    <article className="rw-sample" aria-labelledby={`sample-${number}`}>
      <div className="rw-label">
        <h2 id={`sample-${number}`}>
          <span>{number}</span>
          {name}
        </h2>
        <span>{idea}</span>
      </div>
      <StudyDemo
        id={Number(number)}
        title={name}
        screenClass={`rw-tile rw-${tone}`}
      >
        {children}
      </StudyDemo>
      {related && (
        <Link className="rw-related" href={`/#${related.toLowerCase()}`}>
          {related}の方向性を展開
          <span>
            操作デモへ
            <DemoDirectionIcon diagonal size={14} />
          </span>
        </Link>
      )}
    </article>
  );
}

function VoiceConvoy() {
  const { state } = useStudy();
  const fleet = fleetView(state);
  return (
    <div className="rw-convoy-track">
      {fleet.peers.map((peer) => (
        <div
          key={peer.id}
          className={`rw-convoy-peer ${peer.id === 'self' ? 'rw-self' : state.remote === peer.id ? 'rw-active' : ''}`}
          data-voice-peer={peer.id}
        >
          <i>{peer.id === 'self' ? <Navigation size={17} /> : peer.name[0]}</i>
          <span>
            <b>{peer.name}</b>
            {state.remote === peer.id && (
              <small>
                <DemoVoiceIcon size={14} />
                <DemoText template="{voice}" />
              </small>
            )}
          </span>
          {peer.id === 'self' ? (
            <small>基準位置</small>
          ) : (
            <strong>
              {fleet.available ? peer.distance : '—'}
              <small>
                m{' '}
                {fleet.available
                  ? peer.along < 0
                    ? '後方'
                    : peer.along > 0
                      ? '前方'
                      : '同位置'
                  : '未更新'}
              </small>
            </strong>
          )}
        </div>
      ))}
    </div>
  );
}

function Bars({ count = 19 }: { count?: number }) {
  return (
    <div className="rw-bars" aria-hidden="true">
      {Array.from({ length: count }, (_, i) => (
        <i
          key={i}
          style={{
            height: `${[12, 22, 34, 18, 42, 56, 31, 48, 68, 43, 60, 28, 46, 20, 35, 15, 24, 39, 16][i % 19]}%`,
          }}
        />
      ))}
    </div>
  );
}

function Status({ label = 'HAKONE RIDE' }: { label?: string }) {
  return (
    <div className="rw-status">
      <span>{label}</span>
      <span>
        <DemoSignalIcon size={14} />
        <BatteryMedium size={18} />
      </span>
    </div>
  );
}

function Radar({ light = false }: { light?: boolean }) {
  const view = useView();
  return (
    <svg
      className="rw-radar-svg"
      viewBox="0 0 300 250"
      role="img"
      aria-label={`相対位置：${view.name} ${view.direction} ${view.distance}m`}
    >
      <defs>
        <radialGradient id={light ? 'radar-light' : 'radar-dark'}>
          <stop stopColor={light ? '#e4f2ff' : '#253e73'} />
          <stop offset="1" stopColor={light ? '#fff' : '#101a32'} />
        </radialGradient>
      </defs>
      <circle
        cx="150"
        cy="124"
        r="106"
        fill={`url(#${light ? 'radar-light' : 'radar-dark'})`}
      />
      <g fill="none" stroke="currentColor" opacity=".22">
        <circle cx="150" cy="124" r="100" />
        <circle cx="150" cy="124" r="66" />
        <circle cx="150" cy="124" r="32" />
        <path d="M150 20V228M44 124H256" />
      </g>
      <path
        d="M150 124L179 25A103 103 0 0 0 121 25Z"
        fill="currentColor"
        opacity=".07"
      />
      <g fill="currentColor" fontSize="12" textAnchor="middle">
        <text x="150" y="14">
          N
        </text>
        <text x="275" y="129">
          E
        </text>
        <text x="150" y="246">
          S
        </text>
        <text x="24" y="129">
          W
        </text>
      </g>
      <DemoPositionGroup baseX={181} baseY={63} centerY={124}>
        <circle cx="181" cy="63" r="16" fill="#b9e96c" opacity=".18" />
        <circle cx="181" cy="63" r="7" fill="#b9e96c" />
        <g fill="currentColor" fontSize="13">
          <text x="201" y="60">
            <DemoText template="{name}" />
          </text>
          <text x="201" y="77" opacity=".6">
            <DemoText template="{distance}m" />
          </text>
        </g>
      </DemoPositionGroup>
      <DemoPositionGroup
        baseX={117}
        baseY={168}
        centerY={124}
        peerSlot="secondary"
      >
        <circle cx="117" cy="168" r="5" fill="#9aaefa" />
        <text x="73" y="173" fill="currentColor" fontSize="13">
          <DemoText template="{rearName}" />
        </text>
      </DemoPositionGroup>
      <DemoPositionGroup baseX={171} baseY={208} centerY={124} peerSlot="third">
        <circle cx="171" cy="208" r="5" fill="#f5c297" />
        <text x="181" y="215" fill="currentColor" fontSize="13">
          <DemoText template="{thirdName}" />
        </text>
      </DemoPositionGroup>
      <path d="M150 113L160 137L150 132L140 137Z" fill="currentColor" />
    </svg>
  );
}

function DotText({
  text,
  className = '',
}: {
  text: string;
  className?: string;
}) {
  const field =
    text === '120'
      ? 'distance'
      : text === '85'
        ? 'rearDistance'
        : text === '240'
          ? 'thirdDistance'
          : text === 'AKI'
            ? 'name'
            : text === '4' || text === '04'
              ? 'count'
              : undefined;
  return (
    <DemoDigits
      className={`rw-dot-text ${className}`}
      field={field}
      text={field ? undefined : text}
      radius={1.9}
    />
  );
}

function DotWave() {
  return (
    <svg
      className="rw-dot-wave"
      viewBox="0 0 330 90"
      role="img"
      aria-label="音声の状態を表すドット波形"
    >
      {Array.from({ length: 47 }, (_, x) =>
        Array.from({ length: 13 }, (_, y) => {
          const height = [1, 2, 2, 3, 1, 3, 5, 4, 6, 3, 4, 2, 1][x % 13];
          return (
            <circle
              key={`${x}-${y}`}
              data-wave-lit={Math.abs(y - 6) < height}
              style={
                {
                  '--wave-delay': `${-x * 0.047 - Math.abs(y - 6) * 0.12}s`,
                } as CSSProperties
              }
              cx={4 + x * 7}
              cy={3 + y * 7}
              r="1.6"
              fill={
                Math.abs(y - 6) < height
                  ? x > 16 && x < 29
                    ? '#ff513d'
                    : '#d7d8cc'
                  : '#232521'
              }
            />
          );
        }),
      )}
    </svg>
  );
}

function DotRadar() {
  const view = useView();
  return (
    <svg
      className="rw-dot-radar"
      viewBox="0 0 260 220"
      role="img"
      aria-label={`ドットレーダー：${view.name} ${view.direction} ${view.distance}m`}
    >
      {[38, 69, 99].flatMap((radius, j) =>
        Array.from({ length: 30 + j * 24 }, (_, i) => {
          const angle = (i * Math.PI * 2) / (30 + j * 24);
          return (
            <circle
              key={`${j}-${i}`}
              cx={130 + Math.cos(angle) * radius}
              cy={110 + Math.sin(angle) * radius}
              r="1.2"
              fill="#61655c"
            />
          );
        }),
      )}
      <path
        d="M130 9V211M30 110H230"
        stroke="#40453c"
        strokeDasharray="1 5"
        strokeLinecap="round"
      />
      <DemoPositionGroup baseX={155} baseY={56} centerY={110}>
        <circle
          cx="155"
          cy="56"
          r="12"
          stroke="#ff513d"
          strokeWidth="2"
          strokeDasharray="1 5"
          fill="none"
          strokeLinecap="round"
        />
        <circle cx="155" cy="56" r="3" fill="#ff513d" />
        <g fill="#c7cabe" fontFamily="monospace" fontSize="10">
          <text x="177" y="55">
            <DemoText template="{name}" />
          </text>
          <text x="176" y="70" fill="#ff513d">
            <DemoText template="{distance}m" />
          </text>
        </g>
      </DemoPositionGroup>
      <DemoPositionGroup
        baseX={96}
        baseY={153}
        centerY={110}
        peerSlot="secondary"
      >
        <circle cx="96" cy="153" r="3" fill="#dedfd5" />
        <text x="61" y="157" fill="#c7cabe" fontSize="10">
          <DemoText template="{rearName}" />
        </text>
      </DemoPositionGroup>
      <DemoPositionGroup baseX={151} baseY={194} centerY={110} peerSlot="third">
        <circle cx="151" cy="194" r="3" fill="#dedfd5" />
        <text x="161" y="198" fill="#c7cabe" fontSize="10">
          <DemoText template="{thirdName}" />
        </text>
      </DemoPositionGroup>
      <path d="M130 100L138 119L130 115L122 119Z" fill="#dedfd5" />
    </svg>
  );
}

export default function Explore() {
  const [boardPage, setBoardPage] = useState<StudyPage>('voice');
  return (
    <StudyPageProvider page={boardPage}>
      <main className="rw-board">
        <header className="rw-topbar">
          <Link href="/" className="rw-wordmark">
            <Radio size={21} />
            RoadWeave<span>Design collection</span>
          </Link>
          <Link href="/" className="rw-back">
            <ArrowLeft size={15} />
            操作デモへ
          </Link>
        </header>
        <section className="rw-intro">
          <div>
            <p className="rw-kicker">EXPLORATIONS / 01—60</p>
            <h1>声と、距離と、そのあいだ。</h1>
            <p>
              60案すべてを、そのまま操作。各カードの「操作デモ」から、仲間の声・距離・PTTを試せます。
            </p>
            <a className="rw-dot-jump" href="#dot-studies">
              <span />
              ピクセル・ドットの4案へ
              <ArrowDown size={13} />
            </a>
            <a className="rw-drive-jump" href="#driving-studies">
              走行時の視認性を考えた16案へ <ArrowDown size={14} />
            </a>
            <a className="rw-oled-jump" href="#amoled-studies">
              黒基調の8案へ <ArrowDown size={14} />
            </a>
            <a className="rw-pop-jump" href="#pop-studies">
              色と遊び心を加えた8案へ <ArrowDown size={14} />
            </a>
            <a className="rw-curve-jump" href="#curve-studies">
              鮮やかな色と曲線の6案へ <ArrowDown size={14} />
            </a>
            <a className="rw-identity-jump" href="#color-identity-studies">
              ネオンカラーで見分ける改訂6案へ <ArrowDown size={14} />
            </a>
          </div>
          <div className="rw-context">
            <span>
              <i />
              基本のサンプル
            </span>
            <p>
              箱根 · 4人 · AKIから受信中
              <br />
              前方 120m / 後方 85m・240m
            </p>
          </div>
        </section>
        <div className="study-board-switch">
          <StudyPageTabs
            value={boardPage}
            onChange={setBoardPage}
            label="60案の比較画面"
          />
          <p>
            60案を一括で切り替え。各案の操作デモでも、同じ状態のまま声と車列を見比べられます。
          </p>
        </div>
        <div className="rw-grid">
          <Sample
            number="01"
            name="Together"
            idea="声を、近くに。"
            tone="together"
          >
            <div className="rw-tile-heading">
              <span>箱根ツーリング</span>
              <span className="rw-pill">
                <Users size={13} />
                <DemoText template="{count}人" />
              </span>
            </div>
            <div className="rw-person-feature">
              <div className="rw-person-orbit">
                <span>
                  <DemoText template="{initial}" />
                </span>
                <b>
                  <DemoVoiceIcon size={18} />
                </b>
              </div>
              <div>
                <small>
                  <DemoText template="{speakerLabel}" />
                </small>
                <h3>
                  <DemoText template="{name}" />
                </h3>
                <p>
                  <DemoText template="{direction} · {distance}m" />
                </p>
              </div>
            </div>
            <div className="rw-soft-bottom">
              <span>
                <i className="rw-avatar rw-lilac">
                  <DemoText template="{rearInitial}" />
                </i>
                <i className="rw-avatar rw-peach">
                  <DemoText template="{thirdInitial}" />
                </i>
                <i className="rw-avatar rw-blue">自</i>
              </span>
              <span>
                <DemoText template="{connection}" />
                <Check size={14} />
              </span>
            </div>
          </Sample>

          <Sample number="02" name="Apex" idea="一瞬で、読み取る。" tone="apex">
            <Status />
            <div className="rw-apex-distance">
              <span>
                <DemoDirectionIcon diagonal size={48} />
              </span>
              <div>
                <small>
                  <DemoText template="{name} / {direction}" />
                </small>
                <h3>
                  <DemoText template="{distance}" />
                  <span>m</span>
                </h3>
              </div>
            </div>
            <div className="rw-apex-line">
              <i />
              <i />
              <i />
              <i />
              <i />
              <i />
              <i />
              <i />
              <i />
              <i />
              <i />
              <i />
            </div>
            <div className="rw-apex-footer">
              <span>
                <DemoVoiceIcon size={18} />
                <DemoText template="{voiceSentence}" />
              </span>
              <span>
                <DemoText template="{rearDirection}" />
                <b>
                  <DemoText template="{rearDistance}" />
                </b>{' '}
                m
              </span>
            </div>
          </Sample>

          <Sample
            number="03"
            name="Orbit"
            idea="仲間を、ひとつの視界に。"
            tone="orbit"
          >
            <div className="rw-tile-heading">
              <span>Group radar</span>
              <span className="rw-subtle">
                <DemoText template="{range}m" />
              </span>
            </div>
            <Radar />
            <div className="rw-orbit-footer">
              <span>
                <i />
                <DemoText template="{voiceSentence}" />
              </span>
              <DemoVoiceIcon size={20} />
            </div>
          </Sample>

          <Sample
            number="04"
            name="Contour"
            idea="道の上で、つながる。"
            tone="contour"
          >
            <svg
              className="rw-map"
              viewBox="0 0 400 330"
              preserveAspectRatio="xMidYMid slice"
              role="img"
              aria-label="箱根の仮想地図。仲間の相対位置を表示"
            >
              <rect width="400" height="330" fill="#e9efdd" />
              <g fill="none" stroke="#b9cbb1" strokeWidth="1.1" opacity=".7">
                {Array.from({ length: 9 }, (_, i) => (
                  <path
                    key={i}
                    d={`M${-70 + i * 16} 0 C${120 + i * 15} 90,${-150 + i * 15} 160,${40 + i * 18} 330`}
                  />
                ))}
                {Array.from({ length: 7 }, (_, i) => (
                  <path
                    key={i}
                    d={`M${200 + i * 25} 0 C${100 + i * 20} 95,${360 + i * 14} 120,${240 + i * 28} 330`}
                  />
                ))}
              </g>
              <path d="M400 0H340Q250 85 300 163T340 330H400Z" fill="#abd7e7" />
              <path
                d="M100 350C30 250 260 265 182 179S280 80 227 -20"
                fill="none"
                stroke="white"
                strokeWidth="15"
              />
              <path
                d="M100 350C30 250 260 265 182 179S280 80 227 -20"
                fill="none"
                stroke="#517751"
                strokeWidth="4"
                strokeDasharray="5 4"
              />
              <g fill="#55764f" fontSize="12">
                <text x="29" y="87">
                  箱根スカイライン
                </text>
                <text x="319" y="188" fill="#467b90">
                  芦ノ湖
                </text>
              </g>
              <DemoPositionGroup baseX={218} baseY={95} centerY={182}>
                <circle cx="218" cy="95" r="17" fill="#253d2b" />
                <text
                  x="218"
                  y="100"
                  fill="white"
                  textAnchor="middle"
                  fontSize="12"
                >
                  <DemoText template="{initial}" />
                </text>
                <rect
                  x="244"
                  y="75"
                  width="130"
                  height="33"
                  rx="16"
                  fill="white"
                />
                <text x="251" y="97" fontSize="13" fill="#263e30">
                  <DemoText template="{name} · {distance}m" />
                </text>
              </DemoPositionGroup>
              <circle cx="187" cy="182" r="20" fill="#3479ed" opacity=".18" />
              <circle
                cx="187"
                cy="182"
                r="8"
                fill="#3479ed"
                stroke="white"
                strokeWidth="3"
              />
              <DemoPositionGroup
                baseX={161}
                baseY={231}
                centerY={182}
                peerSlot="secondary"
              >
                <circle
                  cx="161"
                  cy="231"
                  r="7"
                  fill="#9985be"
                  stroke="white"
                  strokeWidth="2"
                />
              </DemoPositionGroup>
              <DemoPositionGroup
                baseX={119}
                baseY={278}
                centerY={182}
                peerSlot="third"
              >
                <circle
                  cx="119"
                  cy="278"
                  r="6"
                  fill="#d69868"
                  stroke="white"
                  strokeWidth="2"
                />
              </DemoPositionGroup>
            </svg>
            <div className="rw-map-title">
              <MapPin size={15} />
              箱根<span>位置イメージ</span>
            </div>
            <div className="rw-map-overlay">
              <div>
                <h3>同じ道を、走る。</h3>
                <p>
                  <DemoText template="{count}人 · {connection}" />
                </p>
              </div>
              <span>
                <DemoVoiceIcon size={18} />
                <DemoText template="{name}" />
              </span>
            </div>
          </Sample>

          <Sample
            number="05"
            name="Frequency"
            idea="声そのものを、主役に。"
            tone="frequency"
          >
            <div className="rw-tile-heading">
              <span>
                <Headphones size={16} />
                Live voice
              </span>
              <span className="rw-pill">
                <DemoText template="{voice}" />
              </span>
            </div>
            <h3>
              <DemoText template="{name}" />
              <span>
                <DemoText template="{voice}" />
              </span>
            </h3>
            <Bars count={35} />
            <div className="rw-frequency-bottom">
              <span>箱根ツーリング</span>
              <span>
                <DemoText template="{count}人 · {connection}" />
              </span>
            </div>
          </Sample>

          <Sample
            number="06"
            name="Paper"
            idea="必要なことを、静かに。"
            tone="paper"
          >
            <Status label="ROADWEAVE / 06" />
            <h3>
              箱根、
              <br />
              みんなと。
            </h3>
            <div className="rw-paper-rule" />
            <div className="rw-paper-distance">
              <div>
                <small>
                  <DemoText template="{direction} · {name}" />
                </small>
                <strong>
                  <DemoText template="{distance}" />
                  <span>m</span>
                </strong>
              </div>
              <DemoDirectionIcon diagonal size={46} strokeWidth={1.1} />
            </div>
            <div className="rw-paper-rear">
              <span>
                <DemoText template="{rearName}" />
                <b>
                  <DemoText template="{rearDistance}m" />
                </b>
              </span>
              <span>
                <DemoText template="{thirdName}" />
                <b>
                  <DemoText template="{thirdDistance}m" />
                </b>
              </span>
            </div>
            <p className="rw-paper-voice">
              <DemoVoiceIcon size={16} />
              <DemoText template="{voiceSentence}" />
            </p>
          </Sample>

          <Sample
            number="07"
            name="Convoy"
            idea="前と後ろを、そのままに。"
            tone="convoy"
          >
            <div className="rw-tile-heading">
              <span>ライドの並び</span>
              <span className="rw-subtle">
                <DemoText template="{count}人" />
              </span>
            </div>
            <VoiceConvoy />
          </Sample>

          <Sample
            number="08"
            name="Signal"
            idea="迷わない、大きなサイン。"
            tone="signal"
          >
            <div className="rw-signal-top">
              <span>
                <Radio size={18} />
                <DemoText template="{voice}" />
              </span>
              <span>
                <DemoText template="{count}人 · {connection}" />
              </span>
            </div>
            <div className="rw-signal-main">
              <DemoDirectionIcon diagonal strokeWidth={2.5} />
              <div>
                <small>
                  <DemoText template="{name} / {direction}" />
                </small>
                <h3>
                  <DemoText template="{distance}" />
                  <span>m</span>
                </h3>
              </div>
            </div>
            <div className="rw-signal-bottom">
              <span>
                <DemoVoiceIcon size={24} />
                <DemoText template="{voiceSentence}" />
              </span>
              <span>
                <DemoText template="{rearName} {rearDistance}m {rearDirection}" />
              </span>
            </div>
          </Sample>

          <Sample number="09" name="Halo" idea="声の気配を、光で。" tone="halo">
            <Status label="箱根ツーリング" />
            <div className="rw-halo-orb">
              <div>
                <DemoVoiceIcon size={27} />
                <h3>
                  <DemoText template="{name}" />
                </h3>
                <span>
                  <DemoText template="{voice}" />
                </span>
              </div>
            </div>
            <div className="rw-halo-info">
              <span>
                <DemoDirectionIcon size={15} />
                <DemoText template="{direction} {distance}m" />
              </span>
              <span>
                <Users size={15} />
                <DemoText template="{count}人 · {connection}" />
              </span>
            </div>
          </Sample>

          <Sample
            number="10"
            name="Field"
            idea="小さな、頼れる計器。"
            tone="field"
          >
            <Status label="RW / GROUP 04" />
            <div className="rw-field-heading">
              <span>HEADING</span>
              <span>北東 / NE</span>
            </div>
            <div className="rw-field-dial">
              <div className="rw-field-ticks" />
              <span className="rw-field-n">N</span>
              <Navigation size={46} strokeWidth={1.5} />
              <span className="rw-field-e">E</span>
              <b>045°</b>
            </div>
            <div className="rw-field-data">
              <span>
                <DemoText template="{direction} / {name}" />
                <strong>
                  <DemoText template="{distance}" />
                  <small>m</small>
                </strong>
              </span>
              <span>
                <DemoText template="{rearDirection} / {rearName}" />
                <strong>
                  <DemoText template="{rearDistance}" />
                  <small>m</small>
                </strong>
              </span>
            </div>
            <div className="rw-field-voice">
              <DemoVoiceIcon size={16} />
              <DemoText template="{rx}: {name}" />
              <span>
                <DemoText template="{connection}" />
              </span>
            </div>
          </Sample>

          <Sample
            number="11"
            name="Tiles"
            idea="必要な情報を、小分けに。"
            tone="tiles"
          >
            <div className="rw-tile-heading">
              <span>箱根ツーリング</span>
              <span>
                <DemoSignalIcon size={15} />
              </span>
            </div>
            <div className="rw-mini-grid">
              <div className="rw-mini-voice">
                <div>
                  <span className="rw-avatar">
                    <DemoText template="{initial}" />
                  </span>
                  <DemoVoiceIcon size={25} />
                </div>
                <h3>
                  <DemoText template="{name}" />
                </h3>
                <p>
                  <DemoText template="{voice}" />
                </p>
              </div>
              <div className="rw-mini-distance">
                <DemoDirectionIcon diagonal size={25} />
                <small>
                  <DemoText template="{direction}" />
                </small>
                <strong>
                  <DemoText template="{distance}" />
                  <span>m</span>
                </strong>
              </div>
              <div className="rw-mini-people">
                <Users size={19} />
                <strong>
                  <DemoText template="{count}" />
                  <span>人</span>
                </strong>
                <small>
                  <DemoText template="{connection}" />
                </small>
              </div>
              <div className="rw-mini-rear">
                <small>
                  <ArrowDown size={14} />
                  ほかの仲間
                </small>
                <span>
                  <DemoText template="{rearName}" />
                  <b>
                    <DemoText template="{rearDistance}m" />
                  </b>
                </span>
                <span>
                  <DemoText template="{thirdName}" />
                  <b>
                    <DemoText template="{thirdDistance}m" />
                  </b>
                </span>
              </div>
            </div>
          </Sample>

          <Sample
            number="12"
            name="Channel"
            idea="無線機らしさを、楽しむ。"
            tone="channel"
          >
            <div className="rw-channel-brand">
              <span>
                <Radio size={20} />
                ROADWEAVE
              </span>
              <span>IP-PTT</span>
            </div>
            <div className="rw-lcd">
              <div className="rw-lcd-top">
                <span>
                  <DemoText template="{rx}" />
                </span>
                <span>GROUP / 04</span>
                <BatteryMedium size={19} />
              </div>
              <h3>
                <DemoText template="{name}" />
                <span>
                  <DemoText template="{distance} m {arrow}" />
                </span>
              </h3>
              <Bars count={29} />
              <div className="rw-lcd-bottom">
                <span>HAKONE</span>
                <span>
                  <DemoText template="{rearName} {rearDirection} {rearDistance}m" />
                </span>
              </div>
            </div>
            <div className="rw-radio-bottom">
              <div className="rw-speaker-grille">
                {Array.from({ length: 7 }, (_, i) => (
                  <i key={i} />
                ))}
              </div>
              <span>
                <Mic size={19} />
                <small>PUSH TO TALK</small>
              </span>
            </div>
          </Sample>
        </div>
        <section
          className="rw-dot-section"
          id="dot-studies"
          aria-labelledby="dot-heading"
        >
          <header className="rw-dot-section-heading">
            <div>
              <p>ANOTHER FREQUENCY / 13—16</p>
              <h2 id="dot-heading">小さな点で、伝わる。</h2>
              <span>
                黒、白、ときどき赤。ドットで描く4つのインターフェース。
              </span>
            </div>
            <div className="rw-dot-mark">
              <DotText text="RW" />
              <span>DOT STUDIES</span>
            </div>
          </header>
          <div className="rw-dot-grid">
            <Sample
              number="13"
              name="Dot / Distance"
              idea="大きなドット数字"
              tone="dot-distance"
            >
              <div className="rw-dot-top">
                <span>RW—13 / HAKONE</span>
                <span>
                  <i />
                  <DemoText template="{rx}" />
                </span>
              </div>
              <div className="rw-dot-distance-head">
                <span>
                  <DemoText template="{name} / {direction}" />
                </span>
                <DemoDirectionIcon diagonal size={26} strokeWidth={1} />
              </div>
              <DotText text="120" />
              <div className="rw-dot-unit">
                <span>
                  <DemoText template="{direction}" />
                </span>
                <span>m</span>
              </div>
              <div className="rw-dot-rule" />
              <div className="rw-dot-pair">
                <span>
                  <DemoText template="{rearName}" />
                  <DotText text="85" />
                  <small>
                    <DemoText template="m {rearDirection}" />
                  </small>
                </span>
                <span>
                  <DemoText template="{thirdName}" />
                  <DotText text="240" />
                  <small>
                    <DemoText template="m {rearDirection}" />
                  </small>
                </span>
              </div>
              <div className="rw-dot-caption">
                <DemoVoiceIcon size={15} />
                <DemoText template="{voiceSentence}" />
                <span>
                  <DemoText template="{count}人 · {connection}" />
                </span>
              </div>
            </Sample>
            <Sample
              number="14"
              name="Dot / Voice"
              idea="声を描くドット波形"
              tone="dot-voice"
            >
              <div className="rw-dot-top">
                <span>RW—14 / LIVE AUDIO</span>
                <span>
                  <i />
                  <DemoText template="{rx}" />
                </span>
              </div>
              <div className="rw-dot-voice-head">
                <DotText text="AKI" />
                <span>
                  <DemoText template="{voice}" />
                  <br />
                  <b>
                    <DemoText template="{direction} {distance}m" />
                  </b>
                </span>
              </div>
              <DotWave />
              <div className="rw-dot-voice-scale">
                <span>受信音声</span>
                <span>VOICE / MONO</span>
              </div>
              <div className="rw-dot-rule" />
              <div className="rw-dot-list">
                <span>
                  <i>
                    <DemoText template="{initial}" />
                  </i>
                  <DemoText template="{name}" />
                  <DemoVoiceIcon size={15} />
                </span>
                <span>
                  <i>
                    <DemoText template="{rearInitial}" />
                  </i>
                  <DemoText template="{rearName}" />
                </span>
                <span>
                  <i>
                    <DemoText template="{thirdInitial}" />
                  </i>
                  <DemoText template="{thirdName}" />
                </span>
                <span>
                  <i>自</i>あなた
                </span>
              </div>
              <p className="rw-dot-note">
                <DemoText template="{hint}" />
              </p>
            </Sample>
            <Sample
              number="15"
              name="Dot / Radar"
              idea="点でつながる位置関係"
              tone="dot-position"
            >
              <div className="rw-dot-top">
                <span>RW—15 / RELATIVE POSITION</span>
                <span>
                  <DemoText template="{range}m" />
                </span>
              </div>
              <DotRadar />
              <div className="rw-dot-position-footer">
                <span>
                  <i />
                  <DemoText template="{name}" />
                  <b>
                    <DemoText template="{voice}" />
                  </b>
                </span>
                <DotText text="120" />
                <small>
                  <DemoText template="m {direction}" />
                </small>
              </div>
            </Sample>
            <Sample
              number="16"
              name="Dot / Matrix"
              idea="小さな計器をひとつに"
              tone="dot-matrix"
            >
              <div className="rw-dot-top">
                <span>RW—16 / MICRO INTERFACE</span>
                <span>
                  <i />
                  <DemoText template="{rx}" />
                </span>
              </div>
              <div className="rw-matrix-grid">
                <div className="rw-matrix-voice">
                  <small>01 / VOICE</small>
                  <DotText text="AKI" />
                  <span>
                    <i />
                    <DemoText template="{voice}" />
                  </span>
                </div>
                <div>
                  <small>02 / DISTANCE</small>
                  <DotText text="120" />
                  <span>
                    <DemoText template="m {direction}" />
                    <DemoDirectionIcon diagonal size={14} />
                  </span>
                </div>
                <div>
                  <small>03 / GROUP</small>
                  <DotText text="04" />
                  <span>人が接続中</span>
                </div>
                <div>
                  <small>04 / MEMBERS</small>
                  <p>
                    <DemoText template="{rearName}" />
                    <b>
                      <DemoText template="{rearDistance}m" />
                    </b>
                  </p>
                  <p>
                    <DemoText template="{thirdName}" />
                    <b>
                      <DemoText template="{thirdDistance}m" />
                    </b>
                  </p>
                </div>
              </div>
              <div className="rw-matrix-meter">
                <span>LINK</span>
                <div>
                  {Array.from({ length: 32 }, (_, i) => (
                    <i key={i} className={i < 26 ? 'rw-dot-lit' : ''} />
                  ))}
                </div>
                <DemoSignalIcon size={13} />
              </div>
              <div className="rw-dot-caption">
                <Radio size={15} />
                箱根ツーリング
                <span>
                  <DemoText template="{connection}" />
                </span>
              </div>
            </Sample>
          </div>
        </section>
        <DrivingStudies />
        <AmoledStudies />
        <PopStudies />
        <CurveStudies />
        <ColorIdentityStudies />
        <footer className="rw-board-footer">
          <span>RoadWeave / Design studies · 2026</span>
          <p>
            60案すべて仮データで操作できます。音声メーターは送受信に連動する模擬表示です。地図・方位は構成イメージ。
          </p>
          <Link href="/">
            3つの操作デモへ
            <ChevronRight size={15} />
          </Link>
        </footer>
      </main>
    </StudyPageProvider>
  );
}

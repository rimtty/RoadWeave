'use client';
/* oxlint-disable jsx-a11y/prefer-tag-over-role -- SVG geometry represents accessible dot text. */
import type { ReactNode } from 'react';
import {
  ArrowUp,
  ArrowDown,
  ArrowUpRight,
  ArrowDownRight,
  AudioLines,
  Mic,
  Radio,
  WifiOff,
  MapPinOff,
  Signal,
  VolumeX,
  type LucideProps,
} from 'lucide-react';
import { useStudy, useView } from './bindings';
import type { DemoField } from './model';
export const glyphs: Record<string, string[]> = Object.fromEntries(
  Object.entries({
    '0': '01110/10001/10001/10001/10001/10001/01110',
    '1': '00100/01100/00100/00100/00100/00100/01110',
    '2': '01110/10001/00001/00010/00100/01000/11111',
    '3': '11110/00001/00001/01110/00001/00001/11110',
    '4': '00010/00110/01010/10010/11111/00010/00010',
    '5': '11111/10000/10000/11110/00001/00001/11110',
    '6': '01110/10000/10000/11110/10001/10001/01110',
    '7': '11111/00001/00010/00100/01000/01000/01000',
    '8': '01110/10001/10001/01110/10001/10001/01110',
    '9': '01110/10001/10001/01111/00001/00001/01110',
    A: '01110/10001/10001/11111/10001/10001/10001',
    K: '10001/10010/10100/11000/10100/10010/10001',
    I: '11111/00100/00100/00100/00100/00100/11111',
    M: '10001/11011/10101/10101/10001/10001/10001',
    E: '11111/10000/10000/11110/10000/10000/11111',
    N: '10001/11001/11001/10101/10011/10011/10001',
    R: '11110/10001/10001/11110/10100/10010/10001',
    W: '10001/10001/10001/10101/10101/10101/01010',
    X: '10001/10001/01010/00100/01010/10001/10001',
    Y: '10001/10001/01010/00100/00100/00100/00100',
    O: '01110/10001/10001/10001/10001/10001/01110',
    U: '10001/10001/10001/10001/10001/10001/01110',
    '—': '00000/00000/00000/11111/00000/00000/00000',
    ' ': '00000/00000/00000/00000/00000/00000/00000',
  }).map(([letter, rows]) => [letter, rows.split('/')]),
);
export function DemoDigits({
  field = 'distance',
  text,
  className = '',
  square = false,
  radius = 2.15,
}: {
  field?: DemoField;
  text?: string;
  className?: string;
  square?: boolean;
  radius?: number;
}) {
  const view = useView();
  const label = text ?? String(view[field]);
  const value = label === '自分' ? 'YOU' : label;
  const cells = /^[0-9—]+$/.test(value)
    ? Math.max(3, value.length)
    : value.length;
  const offset = (cells - value.length) * 36;
  return (
    <svg
      className={className}
      viewBox={`0 0 ${cells * 36 - 6} 42`}
      role="img"
      aria-label={label}
    >
      {value
        .split('')
        .flatMap((char, n) =>
          (glyphs[char] ?? glyphs['—']).flatMap((row, y) =>
            row
              .split('')
              .map((pixel, x) =>
                pixel === '1' ? (
                  square ? (
                    <rect
                      key={`${n}-${y}-${x}`}
                      x={offset + n * 36 + x * 6 + 0.5}
                      y={y * 6 + 0.5}
                      width="4.5"
                      height="4.5"
                      rx="1.2"
                      fill="currentColor"
                    />
                  ) : (
                    <circle
                      key={`${n}-${y}-${x}`}
                      cx={offset + n * 36 + x * 6 + 3}
                      cy={y * 6 + 3}
                      r={radius}
                      fill="currentColor"
                    />
                  )
                ) : null,
              ),
          ),
        )}
    </svg>
  );
}
export function DemoDirectionIcon({
  secondary = false,
  diagonal = false,
  ...props
}: LucideProps & { secondary?: boolean; diagonal?: boolean }) {
  const view = useView();
  const direction = secondary ? view.rearDirection : view.direction;
  const Icon =
    direction === '位置未更新'
      ? MapPinOff
      : direction === '後方'
        ? diagonal
          ? ArrowDownRight
          : ArrowDown
        : diagonal
          ? ArrowUpRight
          : ArrowUp;
  return <Icon {...props} />;
}
export function DemoVoiceIcon(props: LucideProps) {
  const { state } = useStudy();
  const view = useView();
  const Icon = view.offline
    ? WifiOff
    : state.mode === 'transmitting'
      ? Mic
      : state.muted || state.volume === 0
        ? VolumeX
        : state.remote
          ? AudioLines
          : Radio;
  return <Icon {...props} />;
}
export function DemoSignalIcon(props: LucideProps) {
  const view = useView();
  const Icon = view.offline ? WifiOff : Signal;
  return <Icon {...props} />;
}
export function DemoPositionGroup({
  children,
  baseX,
  baseY,
  centerY,
  peerSlot = 'primary',
}: {
  children: ReactNode;
  baseX: number;
  baseY: number;
  centerY: number;
  peerSlot?: 'primary' | 'secondary' | 'third';
}) {
  const { state } = useStudy();
  const view = useView();
  const distance = Number(
    peerSlot === 'primary'
      ? view.distance
      : peerSlot === 'secondary'
        ? view.rearDistance
        : view.thirdDistance,
  );
  const direction =
    peerSlot === 'primary'
      ? view.direction
      : peerSlot === 'secondary'
        ? view.rearDirection
        : view.thirdDirection;
  const y =
    centerY +
    (direction === '後方' ? 1 : -1) *
      ((Math.min(90, centerY - 26) * distance) / view.range);
  return (
    <g
      visibility={state.stale || view.offline ? 'hidden' : 'visible'}
      transform={`translate(0 ${y - baseY})`}
      data-peer-position={peerSlot}
      data-origin-x={baseX}
    >
      {children}
    </g>
  );
}

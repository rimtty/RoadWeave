import type { Metadata } from 'next';
import Collection from './collection';
export const metadata: Metadata = {
  title: 'RoadWeave — Design collection',
  description:
    '仲間の声と距離を、走行用UI・黒基調・名前入力なしでネオンカラーを使う6案を含む全60案で見比べるスナップショット集。',
};
export default function Page() {
  return <Collection />;
}

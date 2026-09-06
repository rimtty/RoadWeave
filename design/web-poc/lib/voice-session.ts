export const MAX_SPEAKERS = 3;

/** Local transmission occupies one slot; muting never releases a stream slot. */
export function toggleSpeaker<T extends string>(
  remotes: T[],
  id: T,
  transmitting: boolean,
): T[] {
  if (remotes.includes(id)) return remotes.filter((peer) => peer !== id);
  return remotes.length < MAX_SPEAKERS - Number(transmitting)
    ? [...remotes, id]
    : remotes;
}

export const FORMANT_SLOTS = [1, 2, 3, 4, 5];

export function roleOf(i, roles) {
  if (i === 0) return { label: "AIR SHELF", color: "--role-air", formant: null };
  if (i === 6) return { label: "RADIATION / PASS", color: "--role-pass", formant: null };
  const seated = roles && roles[i];
  if (seated) return { label: `${seated} FORMANT`, color: "--role-formant", formant: seated };
  if (i === 5) return { label: "LOWPASS / CHEST", color: "--role-chest", formant: null };
  return { label: `F${i} FORMANT`, color: "--role-empty", formant: null };
}

export function isFormant(i, roles) {
  return !!(roles && roles[i]);
}

const GUARD_ST = 1;

export function clampZeroHz(hz, lane, lanes, roles) {
  const guard = Math.pow(2, GUARD_ST / 12);
  let low = 40;
  let high = 16000;
  for (let i = 0; i < lanes.length; i++) {
    if (i === lane || !isFormant(i, roles) || lanes[i].pole_r <= 0) continue;
    const p = lanes[i].pole_hz;
    if (p <= hz && p * guard > low) low = p * guard;
    if (p > hz && p / guard < high) high = p / guard;
  }
  return Math.min(high, Math.max(low, hz));
}

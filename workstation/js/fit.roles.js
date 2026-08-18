export const FORMANT_SLOTS = [1, 2, 3, 4, 5];

export function roleOf(i, roles) {
  const seated = roles && roles[i];
  if (seated) return /^F\d/.test(seated)
    ? { label: `${seated} FORMANT`, color: "--role-formant", formant: seated }
    : { label: seated, color: "--role-formant", formant: null };
  return { label: "empty", color: "--role-empty", formant: null };
}

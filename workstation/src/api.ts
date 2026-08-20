import type { Curve, Geometry, Law } from "./doc.js";
import type { CornerWords, Lane, StageWords } from "./dsp.js";

export type StageCell = {
  corner: number;
  words: StageWords;
  lane: Lane;
  geometry: Geometry;
  citation: string;
};

export type StageTarget = {
  id: string;
  stage: number;
  corner_count: number;
  stage_count: number;
  source_sr_hz: number;
  cells: StageCell[];
};

export type ArSection = {
  pole_hz: number;
  pole_r: number;
  zero_hz?: number;
  zero_r?: number;
};

export type CurveTarget = {
  name: string;
  curve: number[];
  ar_sections?: ArSection[];
  source_seconds?: number;
  seconds?: number;
};

export type PoseTarget = { name: string; lanes: Lane[] };

export type BodyTarget = {
  name: string;
  corners: Lane[][];
  words?: CornerWords[];
  real_pair_sections_skipped?: number;
};

export type LibraryItem = { id: string; name: string; gloss?: string };

export type StageSource = {
  id: string;
  name: string;
  family: string;
  corners: number;
  stage_count: number;
  source_sr_hz: number;
  tracks: StageWords[][];
};

export type TemplateEntry = {
  type: string;
  generic: { id: string; name: string; order: number }[];
};

export type VocabularyState = {
  seat: { id: string; stage: number; corner: number };
  words: StageWords;
  count: number;
  presets: string[];
  pole: { hz: number; r: number };
};

export type Library = {
  root: string;
  authoring_sr: number;
  pole_ceiling_r?: number;
  stage_sources?: StageSource[];
  vocabulary?: { states?: VocabularyState[]; templates?: TemplateEntry[] };
  mouths?: LibraryItem[];
  bodies?: LibraryItem[];
  recordings?: LibraryItem[];
  poses?: LibraryItem[];
  architectures?: LibraryItem[];
};

export type ResponseResult = { lanes: Lane[]; words: CornerWords };

export type FitResult = {
  lanes: Lane[];
  words: CornerWords;
  target_rms_db: number;
};

export type FitCandidate = { words: CornerWords; rms: number };

export type AuditResult = {
  pass: boolean;
  crown_min_db: number;
  crown_max_db: number;
};

export type WriteResult = { bytes: number };
export type VowelRow = {
  ipa: string;
  f1: number;
  b1: number;
  f2: number;
  b2: number;
  f3: number;
  b3: number;
  f4?: number;
  b4?: number;
  f5?: number;
  b5?: number;
};

export type VowelTable = { description?: string; vowels: Record<string, VowelRow> };

export type Brief = { name: string; sentence?: string; intent: string };

async function get<T>(path: string): Promise<T> {
  const r = await fetch(path);
  const v = await r.json();
  if (!r.ok) throw new Error(v.error || r.statusText);
  return v as T;
}

async function post<T>(path: string, body: unknown): Promise<T> {
  const r = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  const v = await r.json();
  if (!r.ok) throw new Error(v.error || r.statusText);
  return v as T;
}

async function stream(path: string, body: unknown, onLine: (message: Record<string, unknown>) => void) {
  const r = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  if (!r.ok || !r.body) throw new Error(r.statusText);
  const reader = r.body.getReader();
  const decoder = new TextDecoder();
  let buffer = "";
  let done = false;
  while (!done) {
    const step = await reader.read();
    done = step.done;
    if (step.value) buffer += decoder.decode(step.value, { stream: true });
    let cut = buffer.indexOf("\n");
    while (cut >= 0) {
      const line = buffer.slice(0, cut);
      buffer = buffer.slice(cut + 1);
      if (line) onLine(JSON.parse(line));
      cut = buffer.indexOf("\n");
    }
  }
}

type TargetOptions = { stage?: number; slice_at_seconds?: number };

interface TargetApi {
  (source: "stage", id: string, options: { stage: number }): Promise<StageTarget>;
  (source: "mouth" | "recording", id: string, options?: TargetOptions): Promise<CurveTarget>;
  (source: "pose", id: string, options?: TargetOptions): Promise<PoseTarget>;
  (source: "body", id: string, options?: TargetOptions): Promise<BodyTarget>;
}

const target = ((source: string, id: string, options: TargetOptions = {}) =>
  post("/api/target", { source, id, ...options })) as TargetApi;

export const api = {
  library: () => get<Library>("/api/library"),
  target,
  skeleton: (curve: Curve, lanes: Lane[]) => post<{ lanes: Lane[] }>("/api/skeleton", { curve, lanes }),
  response: (lanes: Lane[]) => post<ResponseResult>("/api/response", { lanes }),
  fitStream: async (
    curve: Curve,
    lanes: Lane[],
    laws: Law[],
    positions: number[],
    onCandidate: (candidate: FitCandidate) => void,
  ): Promise<FitResult> => {
    const request = { curve, lanes, laws, cold: false, positions };
    let result: FitResult | null = null;
    let failure: string | null = null;
    await stream("/api/fit_stream", request, (m) => {
      if (m.candidate) onCandidate(m.candidate as FitCandidate);
      else if (m.error) failure = m.error as string;
      else result = m.result as FitResult;
    });
    if (failure) throw new Error(failure);
    if (!result) throw new Error("fit stream ended without a result");
    return result;
  },
  audit: (corners: Lane[][]) => post<AuditResult>("/api/audit", { corners }),
  auditWords: (words: CornerWords[]) => post<AuditResult>("/api/audit_words", { words }),
  writeWords: (words: CornerWords[], kind: string) => post<WriteResult>("/api/write_words", { words, kind }),
  vowelTable: () => get<VowelTable>("/ref/tables/phonetic_formants.json"),
  brief: (id: string) => get<Brief>(`/api/brief?id=${encodeURIComponent(id)}`),
};

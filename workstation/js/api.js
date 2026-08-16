async function get(path) {
  const r = await fetch(path);
  const v = await r.json();
  if (!r.ok) throw new Error(v.error || r.statusText);
  return v;
}

async function post(path, body) {
  const r = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  const v = await r.json();
  if (!r.ok) throw new Error(v.error || r.statusText);
  return v;
}

async function stream(path, body, onLine) {
  const r = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  if (r.status === 404) return null;
  if (!r.ok || !r.body) throw new Error(r.statusText);
  const reader = r.body.getReader();
  const decoder = new TextDecoder();
  let buffer = "";
  let done = false;
  while (!done) {
    const step = await reader.read();
    done = step.done;
    if (step.value) buffer += decoder.decode(step.value, { stream: true });
    let cut;
    while ((cut = buffer.indexOf("\n")) >= 0) {
      const line = buffer.slice(0, cut);
      buffer = buffer.slice(cut + 1);
      if (line) onLine(JSON.parse(line));
    }
  }
  return true;
}

export const api = {
  library: () => get("/api/library"),
  target: (source, id) => post("/api/target", { source, id }),
  skeleton: (curve, lanes) => post("/api/skeleton", { curve, lanes }),
  response: (lanes) => post("/api/response", { lanes }),
  fit: (curve, lanes, laws, cold) => post("/api/fit", { curve, lanes, laws, cold }),
  fitStream: async (curve, lanes, laws, cold, onCandidate) => {
    const request = { curve, lanes, laws, cold };
    let result = null;
    let failure = null;
    const served = await stream("/api/fit_stream", request, (m) => {
      if (m.candidate) onCandidate(m.candidate);
      else if (m.error) failure = m.error;
      else result = m.result;
    });
    if (served === null) return post("/api/fit", request);
    if (failure) throw new Error(failure);
    if (!result) throw new Error("fit stream ended without a result");
    return result;
  },
  corners: (corners) => post("/api/corners", { corners }),
  audit: (corners) => post("/api/audit", { corners }),
  writeBody: (corners, kind) => post("/api/write_body", { corners, kind }),
  writeFrame: (lanes, laws, provenance) => post("/api/write_frame", { lanes, laws, provenance }),
  frame: (id) => post("/api/frame", { id }),
  brief: (id) => get(`/api/brief?id=${encodeURIComponent(id)}`),
};

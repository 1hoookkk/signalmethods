let socket = null;
let ticket = 0;
const waiting = new Map();

export function openWire() {
  if (socket) return;
  socket = new WebSocket(`ws://${location.host}/ws`);
  socket.binaryType = "arraybuffer";
  socket.onmessage = (e) => {
    const view = new DataView(e.data);
    const id = view.getUint32(0, true);
    const settle = waiting.get(id);
    if (!settle) return;
    waiting.delete(id);
    const words = [];
    for (let s = 0; s < 7; s++) {
      const row = new Array(5);
      for (let w = 0; w < 5; w++) row[w] = view.getUint16(4 + s * 10 + w * 2, true);
      words.push(row);
    }
    settle(words);
  };
  socket.onclose = () => {
    socket = null;
    waiting.clear();
  };
  socket.onerror = () => {
    if (socket) socket.close();
  };
}

export function wireReady() {
  return !!socket && socket.readyState === 1;
}

export function wordsOf(lanes) {
  if (!wireReady()) return null;
  const id = ++ticket >>> 0;
  const buffer = new ArrayBuffer(4 + 7 * 20);
  const view = new DataView(buffer);
  view.setUint32(0, id, true);
  for (let s = 0; s < 7; s++) {
    const lane = lanes[s];
    const at = 4 + s * 20;
    view.setFloat32(at, lane.pole_hz, true);
    view.setFloat32(at + 4, lane.pole_r, true);
    view.setFloat32(at + 8, lane.zero_hz, true);
    view.setFloat32(at + 12, lane.zero_r, true);
    view.setFloat32(at + 16, lane.scale, true);
  }
  socket.send(buffer);
  return new Promise((resolve) => waiting.set(id, resolve));
}

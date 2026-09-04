import anywidget
import json
import math
from typing import Dict, List, Optional
import traitlets

class CascadePlotWidget(anywidget.AnyWidget):
    _esm = """
    export default {
      render({ model, el }) {
        const container = document.createElement("div");
        container.className = "trench-plot-container";

        const canvas = document.createElement("canvas");
        canvas.width = 720;
        canvas.height = 300;
        canvas.className = "trench-canvas";
        container.appendChild(canvas);
        el.appendChild(container);

        const ctx = canvas.getContext("2d");

        let dragging = null;
        let hoverHandle = null;

        function freqToX(hz, w) {
          const minL = Math.log10(20.0);
          const maxL = Math.log10(20000.0);
          const curL = Math.log10(Math.max(20.0, Math.min(20000.0, hz)));
          return ((curL - minL) / (maxL - minL)) * w;
        }

        function xToFreq(x, w) {
          const minL = Math.log10(20.0);
          const maxL = Math.log10(20000.0);
          const curL = minL + (x / w) * (maxL - minL);
          return Math.pow(10.0, curL);
        }

        function dbToY(db, h) {
          const clamped = Math.max(-30.0, Math.min(30.0, db));
          return ((30.0 - clamped) / 60.0) * h;
        }

        function yToDb(y, h) {
          return 30.0 - (y / h) * 60.0;
        }

        function redraw() {
          const w = canvas.width;
          const h = canvas.height;

          ctx.fillStyle = "#0c1412";
          ctx.fillRect(0, 0, w, h);

          ctx.lineWidth = 1;
          ctx.strokeStyle = "#1a2a26";
          const dbs = [-20, -10, 10, 20];
          for (const d of dbs) {
            const y = dbToY(d, h);
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(w, y);
            ctx.stroke();
          }

          const fGrid = [50, 100, 200, 500, 1000, 2000, 5000, 10000];
          for (const f of fGrid) {
            const x = freqToX(f, w);
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, h);
            ctx.stroke();
          }

          ctx.strokeStyle = "#38524a";
          ctx.lineWidth = 1.5;
          const y0 = dbToY(0.0, h);
          ctx.beginPath();
          ctx.moveTo(0, y0);
          ctx.lineTo(w, y0);
          ctx.stroke();

          ctx.fillStyle = "#4a6860";
          ctx.font = "10px sans-serif";
          ctx.fillText("0 dB", 6, y0 - 4);
          ctx.fillText("+20 dB", 6, dbToY(20.0, h) - 4);
          ctx.fillText("-20 dB", 6, dbToY(-20.0, h) - 4);

          for (const f of fGrid) {
            const label = f >= 1000 ? (f / 1000) + "k" : f + "";
            ctx.fillText(label, freqToX(f, w) + 3, h - 6);
          }

          const ghosts = model.get("ghost_curves") || [];
          for (const g of ghosts) {
            if (!g || g.length === 0) continue;
            ctx.strokeStyle = "rgba(72, 190, 148, 0.22)";
            ctx.lineWidth = 1.0;
            ctx.beginPath();
            const step = w / (g.length - 1);
            for (let i = 0; i < g.length; ++i) {
              const gx = i * step;
              const gy = dbToY(g[i], h);
              if (i === 0) ctx.moveTo(gx, gy);
              else ctx.lineTo(gx, gy);
            }
            ctx.stroke();
          }

          const curve = model.get("curve_points") || [];
          if (curve.length > 1) {
            ctx.strokeStyle = "#bef0d7";
            ctx.lineWidth = 2.0;
            ctx.shadowColor = "#3cc8be";
            ctx.shadowBlur = 4;
            ctx.beginPath();
            const step = w / (curve.length - 1);
            for (let i = 0; i < curve.length; ++i) {
              const cx = i * step;
              const cy = dbToY(curve[i], h);
              if (i === 0) ctx.moveTo(cx, cy);
              else ctx.lineTo(cx, cy);
            }
            ctx.stroke();
            ctx.shadowBlur = 0;
          }

          const handles = model.get("handles") || [];
          for (let i = 0; i < handles.length; ++i) {
            const hd = handles[i];
            const hx = freqToX(hd.freq, w);
            const hy = dbToY(hd.gain, h);
            const isHover = (hoverHandle === i) || (dragging && dragging.index === i);

            if (hd.type === "pole") {
              ctx.fillStyle = isHover ? "#ffffff" : "#3cc8be";
              ctx.strokeStyle = "#0c1412";
              ctx.lineWidth = 2;
              ctx.beginPath();
              ctx.arc(hx, hy, isHover ? 7 : 5.5, 0, 2 * Math.PI);
              ctx.fill();
              ctx.stroke();

              ctx.fillStyle = "#3cc8be";
              ctx.font = "bold 9px sans-serif";
              ctx.fillText((hd.section + 1) + "", hx - 3, hy - 8);
            } else {
              ctx.fillStyle = "#0c1412";
              ctx.strokeStyle = isHover ? "#ffffff" : "#8be0cf";
              ctx.lineWidth = 2;
              ctx.beginPath();
              ctx.arc(hx, hy, isHover ? 6 : 4.5, 0, 2 * Math.PI);
              ctx.fill();
              ctx.stroke();
            }
          }
        }

        canvas.addEventListener("pointerdown", (e) => {
          const rect = canvas.getBoundingClientRect();
          const px = e.clientX - rect.left;
          const py = e.clientY - rect.top;
          const handles = model.get("handles") || [];
          let hit = -1;
          for (let i = handles.length - 1; i >= 0; --i) {
            const hd = handles[i];
            const hx = freqToX(hd.freq, canvas.width);
            const hy = dbToY(hd.gain, canvas.height);
            const dist = Math.hypot(px - hx, py - hy);
            if (dist <= 14) {
              hit = i;
              break;
            }
          }
          if (hit >= 0) {
            dragging = {
              index: hit,
              startX: px,
              startY: py,
              initFreq: handles[hit].freq,
              initGain: handles[hit].gain,
              initBw: handles[hit].bw
            };
            canvas.setPointerCapture(e.pointerId);
          }
        });

        canvas.addEventListener("pointermove", (e) => {
          const rect = canvas.getBoundingClientRect();
          const px = Math.max(0, Math.min(canvas.width, e.clientX - rect.left));
          const py = Math.max(0, Math.min(canvas.height, e.clientY - rect.top));

          if (dragging) {
            const handles = JSON.parse(JSON.stringify(model.get("handles") || []));
            const hd = handles[dragging.index];
            if (e.shiftKey) {
              const dy = dragging.startY - py;
              hd.bw = Math.max(5.0, dragging.initBw * Math.pow(2.0, dy / 40.0));
            } else {
              hd.freq = Math.round(xToFreq(px, canvas.width) * 10) / 10;
              hd.gain = Math.round(yToDb(py, canvas.height) * 10) / 10;
            }
            model.set("handles", handles);
            model.save_changes();
            redraw();
            return;
          }

          const handles = model.get("handles") || [];
          let prevHover = hoverHandle;
          hoverHandle = null;
          for (let i = handles.length - 1; i >= 0; --i) {
            const hd = handles[i];
            const hx = freqToX(hd.freq, canvas.width);
            const hy = dbToY(hd.gain, canvas.height);
            if (Math.hypot(px - hx, py - hy) <= 14) {
              hoverHandle = i;
              break;
            }
          }
          if (prevHover !== hoverHandle) redraw();
        });

        canvas.addEventListener("pointerup", (e) => {
          if (dragging) {
            canvas.releasePointerCapture(e.pointerId);
            const handles = model.get("handles") || [];
            const hd = handles[dragging.index];
            model.set("drag_event", {
              handle_index: dragging.index,
              section: hd.section,
              type: hd.type,
              freq: hd.freq,
              gain: hd.gain,
              bw: hd.bw,
              timestamp: Date.now()
            });
            model.save_changes();
            dragging = null;
            redraw();
          }
        });

        model.on("change:curve_points", redraw);
        model.on("change:ghost_curves", redraw);
        model.on("change:handles", redraw);

        redraw();
      }
    };
    """

    _css = """
    .trench-plot-container {
      display: flex;
      flex-direction: column;
      align-items: center;
      background: #080d0c;
      border: 1px solid #1a2a26;
      border-radius: 4px;
      padding: 8px;
      user-select: none;
    }
    .trench-canvas {
      display: block;
      cursor: crosshair;
      touch-action: none;
    }
    """

    curve_points = traitlets.List(traitlets.Float()).tag(sync=True)
    ghost_curves = traitlets.List(traitlets.List(traitlets.Float())).tag(sync=True)
    handles = traitlets.List(traitlets.Dict()).tag(sync=True)
    drag_event = traitlets.Dict().tag(sync=True)

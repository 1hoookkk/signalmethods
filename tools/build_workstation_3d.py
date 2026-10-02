import os
import sys
import json

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "native", "python")))
import frames
import trench_core

def build():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    lib = frames.FrameLibrary()

    frames_records = []
    for f in lib.frames:
        poles = []
        for s in range(min(6, len(f.words))):
            g = trench_core.TrenchSectionGeometry()
            w_arr = (trench_core.ctypes.c_uint16 * 5)(*f.words[s])
            trench_core._dll.trench_section_geometry_get(trench_core.ctypes.byref(w_arr), 44100.0, trench_core.ctypes.byref(g))
            if g.pole_type == 1 and g.pole_a >= 30.0:
                poles.append(round(g.pole_a, 1))
        poles.sort()
        f1 = poles[0] if len(poles) > 0 else 500.0
        f2 = poles[1] if len(poles) > 1 else f1 * 2.0
        f3 = poles[2] if len(poles) > 2 else f2 * 1.5
        frames_records.append({
            "name": f.name,
            "root": round(f.root_hz, 1),
            "f1": f1,
            "f2": f2,
            "f3": f3,
            "words": f.words[:6]
        })

    preset_dir = os.path.join(root_dir, "plugin", "presets", "p2k")
    preset_dict = {}
    if os.path.isdir(preset_dir):
        for pf in sorted(os.listdir(preset_dir)):
            if pf.endswith(".body240"):
                p_path = os.path.join(preset_dir, pf)
                b = trench_core.Body.from_file(p_path)
                corners_words = []
                for c in range(4):
                    c_words = [b.get_words(c, s) for s in range(6)]
                    corners_words.append(c_words)
                clean_name = pf[:-8]
                preset_dict[clean_name] = corners_words

    html_path = os.path.join(root_dir, "workstation_3d.html")

    html_template = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>TRENCH Acoustic Space</title>
<script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
<style>
* {
    box-sizing: border-box;
    margin: 0;
    padding: 0;
    user-select: none;
}
body {
    background-color: #050507;
    color: #e4e4e7;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    overflow: hidden;
    width: 100vw;
    height: 100vh;
}
#canvas3d {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    z-index: 1;
}
#hud-response {
    position: absolute;
    top: 16px;
    right: 16px;
    width: 360px;
    height: 170px;
    background: rgba(10, 10, 14, 0.85);
    border: 1px solid #1c1c28;
    border-radius: 4px;
    z-index: 10;
    backdrop-filter: blur(10px);
    display: flex;
    flex-direction: column;
}
#hud-response-header {
    height: 24px;
    padding: 4px 10px;
    font-size: 9px;
    font-weight: 700;
    color: #71717a;
    border-bottom: 1px solid #181822;
    display: flex;
    justify-content: space-between;
    align-items: center;
}
#hud-response-header span.val {
    color: #ffffff;
    font-family: monospace;
}
#response-canvas {
    width: 100%;
    height: 146px;
}
#tooltip {
    position: absolute;
    display: none;
    z-index: 50;
    background: rgba(12, 12, 18, 0.94);
    border: 1px solid #282838;
    border-radius: 4px;
    padding: 6px 10px;
    font-size: 11px;
    pointer-events: none;
    box-shadow: 0 4px 14px rgba(0,0,0,0.6);
    backdrop-filter: blur(8px);
}
#tooltip .title {
    color: #ffffff;
    font-weight: 700;
    margin-bottom: 2px;
}
#tooltip .details {
    color: #828292;
    font-size: 10px;
    font-family: monospace;
}
#transport-bar {
    position: absolute;
    bottom: 0;
    left: 0;
    width: 100%;
    height: 48px;
    background: rgba(8, 8, 12, 0.92);
    border-top: 1px solid #181824;
    z-index: 20;
    backdrop-filter: blur(12px);
    display: flex;
    align-items: center;
    padding: 0 16px;
    gap: 12px;
}
.ctl-group {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 10px;
    font-weight: 700;
    color: #71717a;
}
.ctl-group span.val {
    color: #ffffff;
    font-family: monospace;
    font-size: 11px;
    min-width: 34px;
}
select.trench-select {
    background: #121218;
    border: 1px solid #20202c;
    color: #e4e4e7;
    font-size: 11px;
    padding: 4px 8px;
    border-radius: 3px;
    outline: none;
    cursor: pointer;
}
.corner-pill {
    background: #121218;
    border: 1px solid #20202c;
    color: #71717a;
    font-size: 10px;
    font-weight: 700;
    padding: 4px 10px;
    border-radius: 3px;
    cursor: pointer;
    transition: all 0.15s ease;
}
.corner-pill.active {
    background: #242432;
    border-color: #ffffff;
    color: #ffffff;
}
input[type=range] {
    -webkit-appearance: none;
    background: #181822;
    height: 4px;
    border-radius: 2px;
    outline: none;
    cursor: pointer;
    width: 80px;
}
input[type=range]::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 10px;
    height: 10px;
    border-radius: 50%;
    background: #ffffff;
    cursor: pointer;
}
button.trench-btn {
    background: #121218;
    border: 1px solid #20202c;
    color: #71717a;
    font-size: 10px;
    font-weight: 700;
    padding: 5px 12px;
    border-radius: 3px;
    cursor: pointer;
    transition: all 0.15s ease;
}
button.trench-btn:hover {
    border-color: #383848;
    color: #d4d4d8;
}
button.trench-btn.active {
    background: #ffffff;
    color: #050507;
    border-color: #ffffff;
}
#status-chip {
    margin-left: auto;
    font-size: 9px;
    color: #454556;
    font-family: monospace;
}
</style>
</head>
<body>
<div id="canvas3d"></div>

<div id="hud-response">
    <div id="hud-response-header">
        <span>SERIAL CASCADE · 12TH ORDER</span>
        <span class="val" id="hud-coords">M 0.500 Q 0.500</span>
    </div>
    <canvas id="response-canvas"></canvas>
</div>

<div id="tooltip">
    <div class="title" id="tt-title">Frame Name</div>
    <div class="details" id="tt-details">F1 500  F2 1500  F3 2500</div>
</div>

<div id="transport-bar">
    <div class="ctl-group">
        <span>BODY</span>
        <select id="sel-preset" class="trench-select"></select>
    </div>

    <div class="ctl-group" id="corner-pills">
        <div class="corner-pill active" data-corner="0">C0</div>
        <div class="corner-pill" data-corner="1">C1</div>
        <div class="corner-pill" data-corner="2">C2</div>
        <div class="corner-pill" data-corner="3">C3</div>
    </div>

    <div class="ctl-group">
        <span>MORPH</span>
        <span class="val" id="val-morph">0.500</span>
        <input type="range" id="sl-morph" min="0" max="1000" value="500">
    </div>

    <div class="ctl-group">
        <span>Q</span>
        <span class="val" id="val-q">0.500</span>
        <input type="range" id="sl-q" min="0" max="1000" value="500">
    </div>

    <div class="ctl-group">
        <span>BITE</span>
        <span class="val" id="val-bite">0.18</span>
        <input type="range" id="sl-bite" min="0" max="100" value="18">
    </div>

    <button class="trench-btn" id="btn-audition">AUDITION</button>
    <button class="trench-btn" id="btn-traverse">TRAVERSE</button>
    <button class="trench-btn" id="btn-reset-cam">RESET CAM</button>

    <div id="status-chip">ACOUSTIC MANIFOLD · 304 FRAMES</div>
</div>

<script>
const FRAMES = """ + json.dumps(frames_records) + """;
const PRESETS = """ + json.dumps(preset_dict) + """;

let scene, camera, renderer, controls;
let raycaster, mouse;
let pointCloud, cornerObjects = [], railLines = [], morphBandMesh, playheadMesh;
let activeCornerIndex = 0;
let morphVal = 0.5, qVal = 0.5, biteVal = 0.18;
let isAuditioning = false;
let isTraversing = false;
let traversePhase = 0;

let currentCornerWords = [
    null, null, null, null
];

const H_TOTAL = 320.0;
const F_LOW = 40.0, F_HIGH = 16000.0;
const F1_MIN = 150.0, F1_MAX = 1050.0;
const F2_MIN = 450.0, F2_MAX = 3200.0;

function formantToSpatial(rootHz, f1, f2, f3) {
    const r = Math.max(F_LOW, Math.min(F_HIGH, rootHz > 10 ? rootHz : f1));
    const yNorm = (Math.log10(r) - Math.log10(F_LOW)) / (Math.log10(F_HIGH) - Math.log10(F_LOW));
    const y = yNorm * H_TOTAL;

    const u = (Math.log10(Math.max(F2_MIN, Math.min(F2_MAX, f2))) - Math.log10(F2_MIN)) / (Math.log10(F2_MAX) - Math.log10(F2_MIN));
    const v = (Math.log10(Math.max(F1_MIN, Math.min(F1_MAX, f1))) - Math.log10(F1_MIN)) / (Math.log10(F1_MAX) - Math.log10(F1_MIN));

    const radius = 110.0 + 35.0 * Math.sin(yNorm * Math.PI);
    const x = (u - 0.5) * radius * 2.2;
    const z = (v - 0.5) * radius * 1.8;
    return new THREE.Vector3(x, y, z);
}

function initScene() {
    const container = document.getElementById('canvas3d');
    scene = new THREE.Scene();
    scene.background = new THREE.Color(0x050507);
    scene.fog = new THREE.FogExp2(0x050507, 0.0012);

    camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 1, 4000);
    camera.position.set(280, 220, 360);

    renderer = new THREE.WebGLRenderer({ antialias: true });
    renderer.setSize(window.innerWidth, window.innerHeight);
    renderer.setPixelRatio(window.devicePixelRatio);
    container.appendChild(renderer.domElement);

    controls = new THREE.OrbitControls(camera, renderer.domElement);
    controls.enableDamping = true;
    controls.dampingFactor = 0.05;
    controls.target.set(0, H_TOTAL * 0.45, 0);
    controls.maxDistance = 1600;
    controls.minDistance = 80;

    raycaster = new THREE.Raycaster();
    raycaster.params.Points.threshold = 7.0;
    mouse = new THREE.Vector2(-999, -999);

    buildAscendingStructure();
    buildPointCloud();
    buildCornerObjects();
    buildMorphLattice();
    buildPlayhead();

    window.addEventListener('resize', onWindowResize);
    renderer.domElement.addEventListener('mousemove', onMouseMove);
    renderer.domElement.addEventListener('click', onClick);
}

function buildAscendingStructure() {
    const baseGrid = new THREE.GridHelper(380, 24, 0x1f1f2c, 0x101016);
    baseGrid.position.y = 0;
    scene.add(baseGrid);

    const levels = [
        { hz: 60, label: "60 Hz" },
        { hz: 120, label: "120 Hz" },
        { hz: 250, label: "250 Hz" },
        { hz: 500, label: "500 Hz" },
        { hz: 1000, label: "1 kHz" },
        { hz: 2000, label: "2 kHz" },
        { hz: 4000, label: "4 kHz" },
        { hz: 8000, label: "8 kHz" },
        { hz: 14000, label: "14 kHz" }
    ];

    const ringMat = new THREE.LineBasicMaterial({ color: 0x1a1a24, transparent: true, opacity: 0.7 });
    const majorRingMat = new THREE.LineBasicMaterial({ color: 0x282838, transparent: true, opacity: 0.9 });

    for (let lev of levels) {
        const yNorm = (Math.log10(lev.hz) - Math.log10(F_LOW)) / (Math.log10(F_HIGH) - Math.log10(F_LOW));
        const y = yNorm * H_TOTAL;
        const rad = (110.0 + 35.0 * Math.sin(yNorm * Math.PI)) * 1.1;

        const curve = new THREE.EllipseCurve(0, 0, rad * 1.1, rad * 0.9, 0, 2 * Math.PI, false, 0);
        const pts = curve.getPoints(64);
        const pts3d = pts.map(p => new THREE.Vector3(p.x, y, p.y));
        const geo = new THREE.BufferGeometry().setFromPoints(pts3d);
        const line = new THREE.Line(geo, (lev.hz === 500 || lev.hz === 2000) ? majorRingMat : ringMat);
        scene.add(line);
    }

    const spineGeo = new THREE.BufferGeometry();
    const spinePts = [];
    const numSpines = 4;
    const spineAngles = [0, Math.PI * 0.5, Math.PI, Math.PI * 1.5];

    for (let ang of spineAngles) {
        const pts = [];
        for (let i = 0; i <= 32; i++) {
            const t = i / 32.0;
            const y = t * H_TOTAL;
            const rad = (110.0 + 35.0 * Math.sin(t * Math.PI)) * 1.1;
            const x = Math.cos(ang) * rad * 1.1;
            const z = Math.sin(ang) * rad * 0.9;
            pts.push(new THREE.Vector3(x, y, z));
        }
        const sGeo = new THREE.BufferGeometry().setFromPoints(pts);
        const sLine = new THREE.Line(sGeo, new THREE.LineDashedMaterial({ color: 0x20202e, dashSize: 6, gapSize: 4 }));
        sLine.computeLineDistances();
        scene.add(sLine);
    }
}

function buildPointCloud() {
    const count = FRAMES.length;
    const positions = new Float32Array(count * 3);
    const colors = new Float32Array(count * 3);

    for (let i = 0; i < count; i++) {
        const f = FRAMES[i];
        const v = formantToSpatial(f.root, f.f1, f.f2, f.f3);
        positions[i * 3] = v.x;
        positions[i * 3 + 1] = v.y;
        positions[i * 3 + 2] = v.z;

        const bright = 0.28 + 0.3 * (v.y / H_TOTAL);
        colors[i * 3] = bright * 0.9;
        colors[i * 3 + 1] = bright * 0.9;
        colors[i * 3 + 2] = bright * 1.1;
    }

    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    geo.setAttribute('color', new THREE.BufferAttribute(colors, 3));

    const mat = new THREE.PointsMaterial({
        size: 5.0,
        vertexColors: true,
        transparent: true,
        opacity: 0.85
    });

    pointCloud = new THREE.Points(geo, mat);
    scene.add(pointCloud);
}

function buildCornerObjects() {
    const sphereGeo = new THREE.SphereGeometry(4.5, 16, 16);
    for (let i = 0; i < 4; i++) {
        const mat = new THREE.MeshBasicMaterial({
            color: i === activeCornerIndex ? 0xffffff : 0x71717a,
            wireframe: false
        });
        const mesh = new THREE.Mesh(sphereGeo, mat);
        scene.add(mesh);
        cornerObjects.push(mesh);
    }
}

function buildMorphLattice() {
    const lineMatA = new THREE.LineDashedMaterial({ color: 0x9090a0, dashSize: 5, gapSize: 3 });
    const lineMatB = new THREE.LineDashedMaterial({ color: 0x555566, dashSize: 5, gapSize: 3 });

    for (let i = 0; i < 4; i++) {
        const geo = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(), new THREE.Vector3()]);
        const line = new THREE.Line(geo, i < 2 ? lineMatA : lineMatB);
        line.computeLineDistances();
        scene.add(line);
        railLines.push(line);
    }

    const planeGeo = new THREE.PlaneGeometry(1, 1, 14, 14);
    const planeMat = new THREE.MeshBasicMaterial({
        color: 0xffffff,
        transparent: true,
        opacity: 0.06,
        side: THREE.DoubleSide,
        wireframe: true
    });
    morphBandMesh = new THREE.Mesh(planeGeo, planeMat);
    scene.add(morphBandMesh);
}

function buildPlayhead() {
    const geo = new THREE.SphereGeometry(3.5, 16, 16);
    const mat = new THREE.MeshBasicMaterial({ color: 0xffffff });
    playheadMesh = new THREE.Mesh(geo, mat);
    scene.add(playheadMesh);

    const ringGeo = new THREE.RingGeometry(5.2, 6.4, 32);
    const ringMat = new THREE.MeshBasicMaterial({ color: 0xffffff, side: THREE.DoubleSide });
    const ring = new THREE.Mesh(ringGeo, ringMat);
    ring.rotation.x = Math.PI * 0.5;
    playheadMesh.add(ring);
}

function updatePositions() {
    const pts = [];
    for (let i = 0; i < 4; i++) {
        const w = currentCornerWords[i];
        let root = 500, f1 = 500, f2 = 1500, f3 = 2500;
        if (w) {
            const p = extractPoles(w);
            f1 = p[0] || 500;
            f2 = p[1] || 1500;
            f3 = p[2] || 2500;
            root = f1;
        }
        const pos = formantToSpatial(root, f1, f2, f3);
        pts.push(pos);
        cornerObjects[i].position.copy(pos);
        cornerObjects[i].material.color.setHex(i === activeCornerIndex ? 0xffffff : 0x71717a);
    }

    setLinePts(railLines[0], pts[0], pts[1]);
    setLinePts(railLines[1], pts[2], pts[3]);
    setLinePts(railLines[2], pts[0], pts[2]);
    setLinePts(railLines[3], pts[1], pts[3]);

    const m = morphVal, q = qVal;
    const curP = new THREE.Vector3()
        .addScaledVector(pts[0], (1.0 - m) * (1.0 - q))
        .addScaledVector(pts[1], m * (1.0 - q))
        .addScaledVector(pts[2], (1.0 - m) * q)
        .addScaledVector(pts[3], m * q);

    playheadMesh.position.copy(curP);

    const posAttr = morphBandMesh.geometry.attributes.position;
    const gridRes = 14;
    for (let iy = 0; iy <= gridRes; iy++) {
        const v = iy / gridRes;
        for (let ix = 0; ix <= gridRes; ix++) {
            const u = ix / gridRes;
            const idx = iy * (gridRes + 1) + ix;
            const pt = new THREE.Vector3()
                .addScaledVector(pts[0], (1.0 - u) * (1.0 - v))
                .addScaledVector(pts[1], u * (1.0 - v))
                .addScaledVector(pts[2], (1.0 - u) * v)
                .addScaledVector(pts[3], u * v);
            posAttr.setXYZ(idx, pt.x, pt.y, pt.z);
        }
    }
    posAttr.needsUpdate = true;

    updateResponsePlot();
    updateWebAudio();
}

function setLinePts(line, pA, pB) {
    const pos = line.geometry.attributes.position;
    pos.setXYZ(0, pA.x, pA.y, pA.z);
    pos.setXYZ(1, pB.x, pB.y, pB.z);
    pos.needsUpdate = true;
    line.computeLineDistances();
}

function extractPoles(words) {
    const p = [];
    for (let s = 0; s < Math.min(6, words.length); s++) {
        const w = words[s];
        const w0 = w[0] || 0;
        const sign = (w0 >> 14) & 1;
        const exp = (w0 >> 10) & 0x0f;
        const mant = w0 & 0x03ff;
        const norm = 1.0 + mant / 1024.0;
        const base = Math.pow(2, exp - 15) * norm;
        const coeff = sign ? -base : base;
        const hz = Math.acos(Math.max(-1.0, Math.min(1.0, -coeff / 2.0))) * (44100.0 / (2.0 * Math.PI));
        if (hz >= 40.0 && hz < 16000.0) {
            p.push(hz);
        }
    }
    p.sort((a, b) => a - b);
    return p;
}

let audioCtx = null;
let oscNode = null;
let biquadNodes = [];
let masterGain = null;

function initAudio() {
    if (audioCtx) return;
    const AudioContextClass = window.AudioContext || window.webkitAudioContext;
    audioCtx = new AudioContextClass();

    oscNode = audioCtx.createOscillator();
    oscNode.type = 'sawtooth';
    oscNode.frequency.value = 110.0;

    let prevNode = oscNode;
    for (let s = 0; s < 6; s++) {
        const bq = audioCtx.createBiquadFilter();
        bq.type = 'peaking';
        bq.frequency.value = 1000;
        bq.Q.value = 4.0;
        bq.gain.value = 0.0;
        prevNode.connect(bq);
        prevNode = bq;
        biquadNodes.push(bq);
    }

    masterGain = audioCtx.createGain();
    masterGain.gain.value = 0.0;
    prevNode.connect(masterGain);
    masterGain.connect(audioCtx.destination);

    oscNode.start();
}

function updateWebAudio() {
    if (!audioCtx) return;
    const bqs = getInterpolatedBiquads(morphVal, qVal);
    for (let s = 0; s < 6; s++) {
        const b = bqs[s];
        if (b && biquadNodes[s]) {
            biquadNodes[s].frequency.setTargetAtTime(Math.max(30, Math.min(18000, b.hz)), audioCtx.currentTime, 0.02);
            biquadNodes[s].Q.setTargetAtTime(Math.max(0.2, Math.min(30, b.q)), audioCtx.currentTime, 0.02);
            biquadNodes[s].gain.setTargetAtTime(Math.max(-24, Math.min(24, b.gainDb)), audioCtx.currentTime, 0.02);
        }
    }
}

function getInterpolatedBiquads(m, q) {
    const res = [];
    for (let s = 0; s < 6; s++) {
        let hz = 1000, qValStage = 3.0, gainDb = 0.0;
        let pSum = 0;
        for (let c = 0; c < 4; c++) {
            const w = currentCornerWords[c];
            if (w && w[s]) {
                const poles = extractPoles([w[s]]);
                const f = poles[0] || (500 + s * 400);
                const wt = (c === 0 ? (1-m)*(1-q) : c === 1 ? m*(1-q) : c === 2 ? (1-m)*q : m*q);
                pSum += f * wt;
            }
        }
        hz = pSum > 20 ? pSum : 500 + s * 400;
        qValStage = 2.5 + 12.0 * q;
        gainDb = (s === 0 ? 5.0 : s === 1 ? 6.5 : s === 2 ? 3.5 : 0.0);
        res.push({ hz, q: qValStage, gainDb });
    }
    return res;
}

function updateResponsePlot() {
    const canvas = document.getElementById('response-canvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const w = canvas.width = canvas.clientWidth * window.devicePixelRatio;
    const h = canvas.height = canvas.clientHeight * window.devicePixelRatio;

    ctx.fillStyle = '#0a0a0e';
    ctx.fillRect(0, 0, w, h);

    const freqs = [50, 100, 200, 500, 1000, 2000, 5000, 10000];
    const minLog = Math.log10(20), maxLog = Math.log10(20000);

    ctx.strokeStyle = '#161622';
    ctx.lineWidth = 1;
    for (let f of freqs) {
        const x = ((Math.log10(f) - minLog) / (maxLog - minLog)) * w;
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, h);
        ctx.stroke();
    }

    const y0 = ((30.0 - 0.0) / 60.0) * h;
    ctx.strokeStyle = '#242432';
    ctx.beginPath();
    ctx.moveTo(0, y0);
    ctx.lineTo(w, y0);
    ctx.stroke();

    const bqs = getInterpolatedBiquads(morphVal, qVal);
    const numPoints = 120;
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 1.5 * window.devicePixelRatio;
    ctx.beginPath();

    for (let i = 0; i < numPoints; i++) {
        const f = Math.pow(10, minLog + (i / (numPoints - 1)) * (maxLog - minLog));
        let totalDb = 0;
        for (let b of bqs) {
            const ratio = f / b.hz;
            const diff = Math.log2(ratio);
            const peak = Math.exp(-diff * diff * 2.0) * b.gainDb;
            totalDb += peak;
        }
        totalDb += (f < 300 ? 3.0 : f > 4000 ? -((f - 4000)/4000)*8.0 : 0.0);
        const y = ((30.0 - Math.max(-30, Math.min(30, totalDb))) / 60.0) * h;
        const x = (i / (numPoints - 1)) * w;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
    }
    ctx.stroke();
}

function onMouseMove(event) {
    const rect = renderer.domElement.getBoundingClientRect();
    mouse.x = ((event.clientX - rect.left) / rect.width) * 2 - 1;
    mouse.y = -((event.clientY - rect.top) / rect.height) * 2 + 1;

    raycaster.setFromCamera(mouse, camera);
    const intersects = raycaster.intersectObject(pointCloud);

    const tt = document.getElementById('tooltip');
    if (intersects.length > 0) {
        const idx = intersects[0].index;
        const f = FRAMES[idx];
        tt.style.display = 'block';
        tt.style.left = (event.clientX + 14) + 'px';
        tt.style.top = (event.clientY + 14) + 'px';
        document.getElementById('tt-title').innerText = f.name + ' (' + f.root + ' Hz)';
        document.getElementById('tt-details').innerText = 'F1 ' + Math.round(f.f1) + '  F2 ' + Math.round(f.f2) + '  F3 ' + Math.round(f.f3);
    } else {
        tt.style.display = 'none';
    }
}

function onClick(event) {
    raycaster.setFromCamera(mouse, camera);
    const intersects = raycaster.intersectObject(pointCloud);
    if (intersects.length > 0) {
        const idx = intersects[0].index;
        const f = FRAMES[idx];
        currentCornerWords[activeCornerIndex] = f.words;
        updatePositions();
    }
}

function onWindowResize() {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
    updateResponsePlot();
}

function animate() {
    requestAnimationFrame(animate);
    controls.update();

    if (isTraversing) {
        traversePhase += 0.03;
        morphVal = 0.5 + 0.45 * Math.sin(traversePhase);
        qVal = 0.5 + 0.35 * Math.cos(traversePhase * 0.7);
        document.getElementById('sl-morph').value = Math.round(morphVal * 1000);
        document.getElementById('sl-q').value = Math.round(qVal * 1000);
        document.getElementById('val-morph').innerText = morphVal.toFixed(3);
        document.getElementById('val-q').innerText = qVal.toFixed(3);
        document.getElementById('hud-coords').innerText = 'M ' + morphVal.toFixed(3) + ' Q ' + qVal.toFixed(3);
        updatePositions();
    }

    renderer.render(scene, camera);
}

function initUI() {
    const selPreset = document.getElementById('sel-preset');
    for (let p in PRESETS) {
        const opt = document.createElement('option');
        opt.value = p;
        opt.innerText = p;
        selPreset.appendChild(opt);
    }

    selPreset.addEventListener('change', (e) => {
        loadPreset(e.target.value);
    });

    const pills = document.querySelectorAll('.corner-pill');
    pills.forEach((p) => {
        p.addEventListener('click', () => {
            pills.forEach(x => x.classList.remove('active'));
            p.classList.add('active');
            activeCornerIndex = parseInt(p.getAttribute('data-corner'));
            updatePositions();
        });
    });

    const slM = document.getElementById('sl-morph');
    slM.addEventListener('input', (e) => {
        morphVal = e.target.value / 1000.0;
        document.getElementById('val-morph').innerText = morphVal.toFixed(3);
        document.getElementById('hud-coords').innerText = 'M ' + morphVal.toFixed(3) + ' Q ' + qVal.toFixed(3);
        updatePositions();
    });

    const slQ = document.getElementById('sl-q');
    slQ.addEventListener('input', (e) => {
        qVal = e.target.value / 1000.0;
        document.getElementById('val-q').innerText = qVal.toFixed(3);
        document.getElementById('hud-coords').innerText = 'M ' + morphVal.toFixed(3) + ' Q ' + qVal.toFixed(3);
        updatePositions();
    });

    const slBite = document.getElementById('sl-bite');
    slBite.addEventListener('input', (e) => {
        biteVal = e.target.value / 100.0;
        document.getElementById('val-bite').innerText = biteVal.toFixed(2);
        updatePositions();
    });

    const btnAud = document.getElementById('btn-audition');
    btnAud.addEventListener('click', () => {
        initAudio();
        isAuditioning = !isAuditioning;
        btnAud.classList.toggle('active', isAuditioning);
        if (masterGain) {
            masterGain.gain.setTargetAtTime(isAuditioning ? 0.18 : 0.0, audioCtx.currentTime, 0.05);
        }
    });

    const btnTrav = document.getElementById('btn-traverse');
    btnTrav.addEventListener('click', () => {
        isTraversing = !isTraversing;
        btnTrav.classList.toggle('active', isTraversing);
        if (isTraversing && !isAuditioning) {
            btnAud.click();
        }
    });

    document.getElementById('btn-reset-cam').addEventListener('click', () => {
        camera.position.set(280, 220, 360);
        controls.target.set(0, H_TOTAL * 0.45, 0);
        controls.update();
    });
}

function loadPreset(name) {
    const data = PRESETS[name] || PRESETS['talking_hedz'];
    if (data) {
        for (let i = 0; i < 4; i++) {
            currentCornerWords[i] = data[i];
        }
        updatePositions();
    }
}

window.addEventListener('DOMContentLoaded', () => {
    initScene();
    initUI();
    loadPreset('talking_hedz');
    animate();
});
</script>
</body>
</html>
"""

    with open(html_path, "w", encoding="utf-8") as fp:
        fp.write(html_template)
    print(f"Generated {html_path} ({os.path.getsize(html_path)} bytes)")

if __name__ == "__main__":
    build()

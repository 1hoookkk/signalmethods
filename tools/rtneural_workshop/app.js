const $ = id => document.getElementById(id);
const audio = new Audio();
audio.loop = true;
let state = null, selected = null, audioKind = 'reference', seenJob = null;
let pending = false, polling = false, historyKey = '';
let initialized = false;
const requestedRun = new URLSearchParams(location.search).get('run');
const labels = {baseline: 'Baseline', reference: 'Reference', model: 'RTNeural model'};
const controls = ['reference', 'drive', 'placement', 'filter', 'output', 'hidden', 'steps', 'delay', 'description', 'modelRate', 'modelUpload', 'pairInput', 'pairTarget', 'upload'];

async function api(path, body, headers = {}) {
    const response = await fetch(path, body === undefined ? {} : {method: 'POST', headers: body instanceof Blob ? headers : {'Content-Type': 'application/json', ...headers}, body: body instanceof Blob ? body : JSON.stringify(body)});
    const value = await response.json();
    if (!response.ok) throw new Error(value.error || response.statusText);
    return value;
}

function fail(error) { $('error').textContent = error.message || String(error); $('error').hidden = false; }
function config(action) {
    return {action, reference: $('reference').value, drive_db: Number($('drive').value), output_db: Number($('output').value), placement: $('placement').value, filter: $('filter').checked, hidden: Number($('hidden').value), steps: Number($('steps').value), delay_samples: Number($('delay').value), pair_description: $('description').value};
}

function changed() {
    const kind = $('reference').value;
    const busy = pending || state?.job?.status === 'running';
    for (const id of controls) $(id).disabled = busy;
    $('pairFields').hidden = kind !== 'pair';
    $('modelFields').hidden = kind !== 'model';
    $('nativeFields').hidden = kind !== 'native';
    $('pairPlacementNote').hidden = kind !== 'pair';
    $('auditionSource').hidden = kind === 'pair';
    if (kind === 'pair') $('placement').value = 'before';
    $('placement').disabled = busy || kind === 'pair' || !$('filter').checked;
    $('driveValue').textContent = $('drive').value;
    $('trainingDetail').textContent = kind === 'pair' ? 'Paired captures use 70% for training, 15% for validation and the final 15% for audition. Splits follow the recording in time.' : 'Learn the reference from separate training signals. Your audition source stays out of training.';
    const ready = state && (kind === 'native' || (kind === 'model' && state.model) || (kind === 'pair' && state.pair_input && state.pair_target));
    $('train').disabled = $('audition').disabled = busy || !ready;
    if (!state?.job) $('status').textContent = ready ? 'Ready to render or train.' : 'Import a reference to begin.';
    $('cancel').hidden = state?.job?.status !== 'running';
    if (selected) {
        const current = config(), old = selected.config;
        const keys = ['reference', 'output_db', 'filter', 'placement'];
        if (kind === 'native') keys.push('drive_db');
        if (kind === 'pair') keys.push('delay_samples', 'pair_description');
        let stale = keys.some(key => current[key] !== old[key]);
        if (kind !== 'pair') stale ||= state?.source?.path !== old.source;
        if (kind === 'model') stale ||= state?.model?.path !== old.capture_model?.path;
        if (kind === 'pair') stale ||= state?.pair_input?.path !== old.pair_input || state?.pair_target?.path !== old.pair_target;
        $('stale').hidden = !stale;
    }
}

function artifact(name) { return '/artifact/' + encodeURIComponent(selected.id) + '/' + name; }
function clock(seconds) { seconds = Number.isFinite(seconds) ? Math.max(0, seconds) : 0; return Math.floor(seconds / 60) + ':' + Math.floor(seconds % 60).toString().padStart(2, '0'); }
function updateTime() {
    $('time').textContent = clock(audio.currentTime) + ' / ' + clock(audio.duration);
    $('seek').value = audio.duration ? Math.round(audio.currentTime / audio.duration * 1000) : 0;
}

function setAudio(kind, preserve = true) {
    if (!selected?.audio[kind]) return;
    const position = preserve ? audio.currentTime : 0;
    const resume = preserve && !audio.paused;
    audio.pause();
    audioKind = kind;
    audio.src = artifact(kind + '.wav');
    audio.onloadedmetadata = async () => {
        audio.currentTime = Math.min(position, Math.max(0, audio.duration - 0.01));
        updateTime();
        if (resume) { try { await audio.play(); } catch (error) { fail(error); } }
    };
    for (const button of document.querySelectorAll('[data-audio]')) {
        button.classList.toggle('selected', button.dataset.audio === kind);
        button.setAttribute('aria-pressed', String(button.dataset.audio === kind));
    }
    $('playingLabel').textContent = labels[kind];
    const levels = selected.audio[kind];
    $('peak').textContent = levels.peak_dbfs.toFixed(1) + ' dBFS peak · ' + levels.rms_dbfs.toFixed(1) + ' dBFS RMS' + (levels.peak_dbfs > 0 ? ' · over 0 dBFS' : '');
    $('downloadAudio').hidden = false;
    $('downloadAudio').href = artifact(kind + '.wav');
    $('play').disabled = $('seek').disabled = false;
}

function showRun(run) {
    selected = run;
    const address = new URL(location.href);
    address.searchParams.set('run', run.id);
    history.replaceState(null, '', address);
    const c = run.config;
    const name = c.reference === 'native' ? 'Mackity 1202 · +' + c.drive_db + ' dB drive' : c.reference === 'model' ? c.capture_model.name : (c.pair_description || 'Recorded pair');
    $('runLabel').textContent = name + ' · ' + (c.filter ? (c.placement === 'before' ? 'stage → filter' : 'filter → stage') : 'stage alone') + ' · OUTPUT ' + c.output_db + ' dB';
    $('history').value = run.id;
    for (const button of document.querySelectorAll('[data-audio]')) button.disabled = !run.audio[button.dataset.audio];
    const r = run.report;
    $('valMetric').textContent = r ? r.validation_objective.toFixed(4) : '—';
    $('holdMetric').textContent = r ? r.holdout_lf_esr.toFixed(4) : '—';
    $('parityMetric').textContent = r ? r.export_parity_rmse.toExponential(1) : '—';
    $('fitNote').textContent = r ? 'Validation: MR spectral ' + r.validation_mr_spectral.toFixed(4) + ' + LF ESR ' + r.validation_lf_esr.toFixed(4) + '. Held-out full-band ESR ' + r.holdout_esr_db.toFixed(1) + ' dB is diagnostic only. Native export parity checks implementation, not sound quality.' : 'Reference audio is ready. Train a snapshot to compare the native RTNeural export.';
    $('export').classList.toggle('disabled', !r);
    $('export').setAttribute('aria-disabled', String(!r));
    if (r) $('export').href = artifact('model_rtneural.json'); else $('export').removeAttribute('href');
    $('report').hidden = false;
    $('report').href = artifact('result.json');
    setAudio('reference', false);
    changed();
}

async function refresh() {
    if (polling) return;
    polling = true;
    try {
        state = await api('/api/state');
        $('sourceName').textContent = state.source.name;
        $('sourceDetail').textContent = state.source.duration_seconds.toFixed(2) + ' s · 48 kHz · mono';
        $('pairInputName').textContent = state.pair_input?.name || 'No input imported';
        $('pairTargetName').textContent = state.pair_target?.name || 'No target imported';
        $('modelName').textContent = state.model ? state.model.name + ' · ' + state.model.format + ' · ' + state.model.sample_rate + ' Hz' : 'No model imported';
        const key = state.history.map(run => run.id).join('|');
        if (key !== historyKey) {
            historyKey = key;
            $('history').replaceChildren();
            for (const run of state.history) {
                const option = document.createElement('option');
                option.value = run.id;
                option.textContent = run.config.created_at + ' · ' + run.config.reference + (run.report ? ' · trained' : ' · reference');
                $('history').append(option);
            }
            if (selected) $('history').value = selected.id;
        }
        const j = state.job;
        if (!initialized) {
            initialized = true;
            if (j?.status === 'complete') seenJob = j.id;
            const requested = state.history.find(run => run.id === requestedRun);
            if (requested) showRun(requested);
        }
        if (j) {
            $('status').textContent = j.message;
            $('progress').value = j.progress || 0;
            $('percent').textContent = j.status === 'running' ? Math.round((j.progress || 0) * 100) + '%' : j.status;
            if (j.status === 'running' && j.validation_objective !== undefined) $('liveMetric').textContent = 'Validation objective ' + j.validation_objective.toFixed(4) + ' = MR spectral ' + j.validation_mr_spectral.toFixed(4) + ' + LF ESR ' + j.validation_lf_esr.toFixed(4);
            else $('liveMetric').textContent = '';
            if (j.status === 'complete' && seenJob !== j.id) {
                const run = state.history.find(item => item.id === j.id);
                if (run) { seenJob = j.id; showRun(run); }
            }
        }
        if (!selected && state.history.length) showRun(state.history[0]);
        changed();
    } catch (error) { fail(error); } finally { polling = false; }
}

async function mutate(action) {
    pending = true;
    $('error').hidden = true;
    changed();
    try { await action(); await refresh(); } catch (error) { fail(error); } finally { pending = false; changed(); }
}
for (const label of document.querySelectorAll('.file-button')) label.addEventListener('keydown', event => {
    if (event.key === 'Enter' || event.key === ' ') {
        event.preventDefault();
        $(label.htmlFor).click();
    }
});
for (const id of controls.filter(id => !['upload', 'modelUpload', 'pairInput', 'pairTarget'].includes(id))) $(id).addEventListener('input', changed);
for (const [id, path] of [['upload', '/api/source'], ['pairInput', '/api/pair/input'], ['pairTarget', '/api/pair/target']]) {
    $(id).addEventListener('change', () => {
        const file = $(id).files[0];
        if (file) mutate(() => api(path, file, {'Content-Type': 'audio/wav', 'X-Filename': encodeURIComponent(file.name)}));
        $(id).value = '';
    });
}
$('modelUpload').addEventListener('change', () => {
    const file = $('modelUpload').files[0];
    if (file) mutate(async () => api('/api/model', {name: file.name, sample_rate: Number($('modelRate').value), model: JSON.parse(await file.text())}));
    $('modelUpload').value = '';
});
$('audition').addEventListener('click', () => mutate(() => api('/api/run', config('audition'))));
$('train').addEventListener('click', () => mutate(() => api('/api/run', config('train'))));
$('cancel').addEventListener('click', () => mutate(() => api('/api/cancel', {})));
$('history').addEventListener('change', () => { const run = state.history.find(item => item.id === $('history').value); if (run) showRun(run); });
for (const button of document.querySelectorAll('[data-audio]')) button.addEventListener('click', () => setAudio(button.dataset.audio));
$('play').addEventListener('click', async () => { try { if (audio.paused) await audio.play(); else audio.pause(); } catch (error) { fail(error); } });
audio.addEventListener('play', () => { $('play').textContent = 'Ⅱ'; $('play').setAttribute('aria-label', 'Pause audition'); });
audio.addEventListener('pause', () => { $('play').textContent = '▶'; $('play').setAttribute('aria-label', 'Play audition'); });
audio.addEventListener('timeupdate', updateTime);
audio.addEventListener('error', () => fail(new Error('Audio could not be loaded. Download the WAV to inspect it.')));
$('loop').addEventListener('change', () => { audio.loop = $('loop').checked; });
$('seek').addEventListener('input', () => { if (Number.isFinite(audio.duration)) audio.currentTime = Number($('seek').value) / 1000 * audio.duration; });
refresh();
setInterval(refresh, 1000);

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
import mimetypes
from pathlib import Path
import re
import shutil
import subprocess
import sys
import threading
import time
from urllib.parse import unquote, urlsplit
import uuid
import numpy as np
import soundfile as sf
from references import convert_model

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
DATA = REPO / 'output/rtneural-workshop'
RUNS = DATA / 'runs'
SOURCES = DATA / 'sources'
LOCK = threading.RLock()
STATE = {'job': None, 'source': None, 'model': None, 'pair_input': None, 'pair_target': None}
PROCESS = None


def save_imports():
    path = DATA / 'imports.json'
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps({key: STATE[key] for key in ('source', 'model', 'pair_input', 'pair_target')}, indent=2))
    temporary.replace(path)


def configure(values):
    drive = float(values.get('drive_db', 32))
    output = float(values.get('output_db', -24))
    steps = int(values.get('steps', 1000))
    hidden = int(values.get('hidden', 16))
    action = values.get('action', 'audition')
    placement = values.get('placement', 'before')
    enabled = values.get('filter', True)
    reference = values.get('reference', 'native')
    delay = int(values.get('delay_samples', 0))
    if not np.isfinite([drive, output]).all() or not 0 <= drive <= 60 or not -60 <= output <= 12:
        raise ValueError('Drive must be 0..60 dB and OUTPUT -60..+12 dB')
    if steps not in (1000, 3000, 6000) or hidden not in (8, 16, 32):
        raise ValueError('Unsupported training size')
    if action not in ('audition', 'train') or placement not in ('before', 'after') or not isinstance(enabled, bool):
        raise ValueError('Invalid action or signal order')
    source = STATE['source']
    if reference not in ('native', 'model', 'pair') or abs(delay) > 48000:
        raise ValueError('Invalid reference or delay; delay range is +/-48000 samples')
    if reference == 'model' and not STATE['model']:
        raise ValueError('Import a capture model first')
    if reference == 'pair' and (not STATE['pair_input'] or not STATE['pair_target']):
        raise ValueError('Import both dry input and recorded target first')
    if reference == 'pair' and placement == 'after' and enabled:
        raise ValueError('A recorded pair supports filter-after-capture comparisons. Select stage before filter, or disable the filter.')
    result = {'drive_db': drive, 'output_db': output, 'steps': steps, 'hidden': hidden, 'action': action, 'placement': placement, 'filter': enabled, 'source': source['path'], 'source_name': source['name'], 'created_at': time.strftime('%Y-%m-%d %H:%M:%S'), 'reference': reference, 'delay_samples': delay}
    if reference == 'model':
        result['capture_model'] = STATE['model']
    if reference == 'pair':
        for key in ('pair_input', 'pair_target'):
            result[key] = STATE[key]['path']
        result['pair_description'] = str(values.get('pair_description', 'Unverified hardware capture'))[:500]
    return result


def history():
    result = []
    for path in sorted(RUNS.glob('*/result.json'), reverse=True)[:15]:
        result.append(json.loads(path.read_text()))
    return result


def watch(process, job):
    with (job / 'worker.log').open('w') as log:
        for line in process.stdout:
            log.write(line)
            log.flush()
            try:
                event = json.loads(line)
            except json.JSONDecodeError:
                continue
            with LOCK:
                if STATE['job']['id'] == job.name and STATE['job']['status'] == 'running':
                    STATE['job'].update(event)
        code = process.wait()
    with LOCK:
        if STATE['job']['id'] == job.name and STATE['job']['status'] == 'running':
            complete = code == 0 and (job / 'result.json').exists()
            STATE['job']['status'] = 'complete' if complete else 'failed'
            if not complete:
                STATE['job']['message'] = (job / 'worker.log').read_text()[-1800:]


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def local_request(self):
        port = self.server.server_port
        hosts = {f'127.0.0.1:{port}', f'localhost:{port}'}
        origin = self.headers.get('Origin')
        return self.headers.get('Host') in hosts and (not origin or origin in {f'http://{host}' for host in hosts})

    def send_json(self, value, status=200):
        data = json.dumps(value).encode()
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(data)

    def send_file(self, path, download=False):
        if not path.is_file():
            return self.send_json({'error': 'File not found'}, 404)
        data = path.read_bytes()
        start, end, status = 0, len(data)-1, 200
        byte_range = self.headers.get('Range')
        if byte_range:
            match = re.fullmatch(r'bytes=(\d+)-(\d*)', byte_range)
            if not match:
                return self.send_json({'error': 'Unsupported range'}, 416)
            start = int(match[1])
            end = min(int(match[2]) if match[2] else end, end)
            if start > end:
                return self.send_json({'error': 'Range outside file'}, 416)
            status = 206
        self.send_response(status)
        self.send_header('Content-Type', mimetypes.guess_type(path.name)[0] or 'application/octet-stream')
        self.send_header('Content-Length', str(end-start+1))
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('Cache-Control', 'no-store')
        self.send_header('X-Content-Type-Options', 'nosniff')
        if status == 206:
            self.send_header('Content-Range', f'bytes {start}-{end}/{len(data)}')
        if download:
            self.send_header('Content-Disposition', f'attachment; filename="{path.name}"')
        self.end_headers()
        try:
            self.wfile.write(data[start:end+1])
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def do_GET(self):
        if not self.local_request():
            return self.send_json({'error': 'Local requests only'}, 403)
        path = unquote(urlsplit(self.path).path)
        if path == '/':
            return self.send_file(HERE / 'index.html')
        if path in ('/app.js', '/style.css'):
            return self.send_file(HERE / path[1:])
        if path == '/api/state':
            with LOCK:
                return self.send_json({**STATE, 'history': history()})
        if path == '/capture-input.wav':
            return self.send_file(DATA / 'research/NAM-input.wav', True)
        match = re.fullmatch(r'/artifact/([a-zA-Z0-9_-]+)/(baseline.wav|reference.wav|model.wav|model_rtneural.json|training_report.json|result.json)', path)
        if match:
            return self.send_file(RUNS / match[1] / match[2], match[2].endswith('.json'))
        return self.send_json({'error': 'Not found'}, 404)

    def do_POST(self):
        global PROCESS
        if not self.local_request():
            return self.send_json({'error': 'Local requests only'}, 403)
        try:
            length = int(self.headers.get('Content-Length', 0))
            if not 0 <= length <= 50_000_000:
                raise ValueError('Upload limit is 50 MB')
            body = self.rfile.read(length)
            with LOCK:
                busy = STATE['job'] and STATE['job']['status'] == 'running'
                if self.path == '/api/cancel':
                    if busy and PROCESS and PROCESS.poll() is None:
                        PROCESS.terminate()
                        STATE['job'].update(status='cancelled', message='Run cancelled; existing completed runs are retained')
                    return self.send_json({'ok': True})
                if busy:
                    return self.send_json({'error': 'A run is already in progress'}, 409)
                if self.path == '/api/model':
                    value = json.loads(body)
                    original = value['model']
                    converted, info = convert_model(original, int(value['sample_rate']))
                    destination = DATA / 'models' / uuid.uuid4().hex
                    destination.mkdir(parents=True)
                    (destination / 'original.json').write_text(json.dumps(original))
                    (destination / 'runtime.json').write_text(json.dumps(converted))
                    info.update(path=str(destination / 'runtime.json'), name=str(value.get('name', 'Imported capture'))[:150])
                    (destination / 'metadata.json').write_text(json.dumps(info, indent=2))
                    STATE['model'] = info
                    save_imports()
                    return self.send_json({'ok': True, 'model': info})
                if self.path in ('/api/pair/input', '/api/pair/target'):
                    x, rate = sf.read(io.BytesIO(body))
                    if rate != 48000 or x.ndim != 1 or not 48000*12 <= len(x) <= 48000*600 or not np.isfinite(x).all():
                        raise ValueError('Use mono 48 kHz paired WAVs, 12 seconds to 10 minutes long')
                    destination = SOURCES / f'{uuid.uuid4().hex}.wav'
                    destination.write_bytes(body)
                    key = 'pair_input' if self.path.endswith('input') else 'pair_target'
                    STATE[key] = {'path': str(destination), 'name': Path(unquote(self.headers.get('X-Filename', 'Capture.wav'))).name[:120], 'duration_seconds': len(x)/rate}
                    save_imports()
                    return self.send_json({'ok': True})
                if self.path == '/api/source':
                    x, rate = sf.read(io.BytesIO(body), always_2d=True)
                    if rate != 48000 or len(x) == 0 or len(x) > 48000*60 or not np.isfinite(x).all():
                        raise ValueError('Choose a finite 48 kHz WAV, up to 60 seconds')
                    name = Path(unquote(self.headers.get('X-Filename', 'Audio.wav'))).name[:120]
                    destination = SOURCES / f'{uuid.uuid4().hex}.wav'
                    sf.write(str(destination), x.mean(axis=1), rate, subtype='FLOAT')
                    STATE['source'] = {'path': str(destination), 'name': name, 'duration_seconds': len(x)/rate}
                    save_imports()
                    return self.send_json({'ok': True})
                if self.path == '/api/run':
                    config = configure(json.loads(body))
                    job = RUNS / (time.strftime('%Y%m%d-%H%M%S') + '-' + uuid.uuid4().hex[:8])
                    job.mkdir()
                    (job / 'config.json').write_text(json.dumps(config, indent=2))
                    PROCESS = subprocess.Popen([sys.executable, '-u', str(HERE / 'worker.py'), str(job)], cwd=REPO, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
                    STATE['job'] = {'id': job.name, 'status': 'running', 'message': 'Starting', 'progress': 0, 'config': config}
                    threading.Thread(target=watch, args=(PROCESS, job), daemon=True).start()
                    return self.send_json({'id': job.name}, 202)
                return self.send_json({'error': 'Not found'}, 404)
        except (ValueError, TypeError, KeyError, IndexError, AttributeError, OSError, RuntimeError) as error:
            return self.send_json({'error': str(error)}, 400)


def main(port):
    RUNS.mkdir(parents=True, exist_ok=True)
    SOURCES.mkdir(parents=True, exist_ok=True)
    default = SOURCES / 'bass_phrase.wav'
    if not default.exists():
        shutil.copyfile(REPO / 'output/tube-rtneural-20260929/holdout_input.wav', default)
    STATE['source'] = {'path': str(default), 'name': 'Bass test phrase', 'duration_seconds': sf.info(str(default)).duration}
    if (DATA / 'imports.json').exists():
        saved = json.loads((DATA / 'imports.json').read_text())
        for key in ('source', 'model', 'pair_input', 'pair_target'):
            value = saved.get(key)
            if value and Path(value['path']).is_file():
                STATE[key] = value
    if not (DATA / 'bin/runner.exe').exists():
        raise RuntimeError('Build the native runner with tools/rtneural_workshop/build.cmd first')
    server = ThreadingHTTPServer(('127.0.0.1', port), Handler)
    print(f'RTNeural workshop: http://127.0.0.1:{port}', flush=True)
    try:
        server.serve_forever()
    finally:
        server.server_close()
        if PROCESS and PROCESS.poll() is None:
            PROCESS.terminate()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, default=8896)
    main(parser.parse_args().port)

import unittest
from unittest.mock import patch
import json
from pathlib import Path
import tempfile
import numpy as np
import soundfile as sf
from bus_output import FS, harmonics_at, simulate
from train_bus import render_record


class HarmonicMeasurementTests(unittest.TestCase):
    def test_fractional_cycle_window_separates_fundamental_harmonic_and_dc(self):
        t=np.arange(FS//4)/FS
        x=.37+1.2*np.sin(2*np.pi*50*t+.31)+.12*np.cos(2*np.pi*150*t-.9)
        fundamental,harmonics=harmonics_at(x,50)
        self.assertAlmostEqual(fundamental,1.2,places=10)
        self.assertAlmostEqual(harmonics[1],.12,places=10)
        self.assertLess(abs(harmonics[0]),1e-10)
        self.assertLess(np.linalg.norm(harmonics[2:]),1e-10)


class StorageTests(unittest.TestCase):
    def test_inherited_interpolation_cannot_masquerade_as_raw_samples(self):
        source='Storage isolation\nV1 in 0 dc 0 sin(0 1 1000)\nR1 in out 1k\nC1 out 0 1n\n.save v(out)\n'
        with tempfile.TemporaryDirectory() as directory:
            raw=simulate(Path(directory),source+'.tran 1u 100u 0 .1u\n.end\n','initial_adaptive')
            self.assertGreater(len(raw['time']),900)
            values=simulate(Path(directory),source+'.options interp\n.tran 1u 100u 0 .1u\n.end\n','uniform')
            self.assertLess(len(values['time']),200)
            with self.assertRaisesRegex(RuntimeError,'fresh process'):
                simulate(Path(directory),source+'.tran 1u 100u 0 .1u\n.end\n','adaptive')


class CaptureCacheTests(unittest.TestCase):
    def test_cache_reuses_exact_audio_and_rejects_changed_input_audio_or_source(self):
        x=np.linspace(-.3,.3,100,dtype=np.float32)
        def fixture(folder,part,volts,label,storage):
            sf.write(str(Path(folder)/f'{label}_volts.wav'),part.astype(np.float64)*3.1234567,FS,subtype='FLOAT')
        with tempfile.TemporaryDirectory() as directory,patch('train_bus.bus_output.render',side_effect=fixture) as renderer:
            root=Path(directory)
            _,first,_=render_record(root,x,0,{'fixture':'unchanged'})
            _,cached,_=render_record(root,x,0,{'fixture':'unchanged'})
            np.testing.assert_array_equal(first,cached)
            self.assertEqual(renderer.call_count,1)
            changed=x*.5
            render_record(root,changed,0,{'fixture':'unchanged'})
            self.assertEqual(renderer.call_count,2)
            sf.write(str(root/'record00_volts.wav'),np.zeros_like(x),FS,subtype='FLOAT')
            render_record(root,changed,0,{'fixture':'unchanged'})
            self.assertEqual(renderer.call_count,3)
            path=root/'record00_cache.json'
            metadata=json.loads(path.read_text())
            metadata['source_kind']='pwl'
            path.write_text(json.dumps(metadata))
            render_record(root,changed,0,{'fixture':'unchanged'})
            self.assertEqual(renderer.call_count,4)
            render_record(root,changed,0,{'fixture':'different'})
            self.assertEqual(renderer.call_count,5)


if __name__=='__main__':
    unittest.main()

import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import numpy as np
import soundfile as sf
import torch
from losses import AudioObjective
from model import Net, export, predict, widen,save_checkpoint
from references import align_pair, convert_model, split_pair
from records import run_records, score_records, training_layout, training_starts,prime_records
from worker import DATA, FS, native, sha, train, main


class ObjectiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        torch.set_num_threads(4)
        cls.objective = AudioObjective()
        t = torch.arange(4096) / FS
        cls.low = torch.sin(2 * torch.pi * 93.75 * t)
        cls.high = torch.sin(2 * torch.pi * 6000 * t)

    def test_identity_and_silence_have_finite_zero_gradients(self):
        for target in (self.low, torch.zeros(4096)):
            prediction = target.clone().requires_grad_()
            loss, _ = self.objective(prediction, target)
            self.assertEqual(float(loss.detach()), 0)
            loss.backward()
            self.assertTrue(torch.isfinite(prediction.grad).all())
            self.assertEqual(float(prediction.grad.abs().max()), 0)

    def test_bass_error_is_isolated_by_low_frequency_esr(self):
        target = self.low + self.high
        _, low = self.objective(target + 0.2*self.low, target)
        _, high = self.objective(target + 0.2*self.high, target)
        self.assertAlmostEqual(float(low['lf_esr']), 0.04, places=4)
        self.assertLess(float(high['lf_esr']), 1e-9)

    def test_spectral_term_detects_high_frequency_error(self):
        loss, parts = self.objective(self.low + 0.2*self.high, self.low)
        self.assertGreater(float(parts['mr_spectral']), 0.1)
        self.assertAlmostEqual(float(loss), float(parts['mr_spectral'] + parts['lf_esr']), places=5)

    def test_gain_error_is_not_normalized_away(self):
        _, parts = self.objective(self.low*2, self.low)
        self.assertAlmostEqual(float(parts['lf_esr']), 1.0, places=5)
        self.assertGreater(float(parts['mr_spectral']), 0.5)

    def test_nonzero_output_on_silence_has_finite_gradient(self):
        prediction = torch.full((2, 4096), 0.001, requires_grad=True)
        loss, _ = self.objective(prediction, torch.zeros_like(prediction))
        loss.backward()
        self.assertGreater(float(loss.detach()), 0)
        self.assertTrue(torch.isfinite(prediction.grad).all())

    def test_silence_denominator_floor_has_defined_scale(self):
        prediction = torch.full((4096,), 0.001)
        loss, parts = self.objective(prediction, torch.zeros_like(prediction))
        self.assertAlmostEqual(float(parts['lf_esr']), 1.0, places=4)
        self.assertLess(float(loss), 10)

    def test_evaluation_handles_partial_blocks(self):
        x = self.low.repeat(3)[:9000]
        loss, _ = self.objective.evaluate(x, x)
        self.assertEqual(float(loss), 0)


class ReferenceTests(unittest.TestCase):
    def test_random_capture_positions_preserve_boundaries_and_exact_context(self):
        starts=training_starts(np.random.default_rng(41),14*3*FS,3*FS,12000,4096,8,512,True)
        offsets=starts%(3*FS)
        self.assertTrue(np.all(offsets>=12000))
        self.assertTrue(np.all(offsets+8*4096<=3*FS))
        self.assertGreater(len(np.unique(offsets)),400)
        torch.manual_seed(119)
        for device in ['cpu']+(['cuda'] if torch.cuda.is_available() else []):
            net=Net(8,input_scale=16).to(device)
            data=torch.randn(800,device=device)*.02
            indices=np.array([0,313,590,660])
            actual=prime_records(net,data,indices,200,warm=16,chunk=37)
            self.assertTrue(net.lstm.training)
            with torch.no_grad():
                for i,index in enumerate(indices):
                    _,expected=net.lstm(torch.zeros(1,16,1,device=device))
                    base=index//200*200
                    if index>base:
                        _,expected=net.lstm(data[base:index].view(1,-1,1)*net.input_scale,expected)
                    for a,b in zip(actual,expected):
                        torch.testing.assert_close(a[:,i:i+1],b,atol=1e-5,rtol=0)

    @unittest.skipUnless(torch.cuda.is_available(),'CUDA unavailable in this Python environment')
    def test_cuda_objective_export_and_checkpoint_remain_portable(self):
        torch.manual_seed(442)
        torch.backends.cuda.matmul.allow_tf32=False
        torch.backends.cudnn.allow_tf32=False
        target=torch.randn(2,4096)*.1
        prediction=target*.85+torch.randn_like(target)*.005
        cpu,parts=AudioObjective()(prediction,target)
        gpu,gpu_parts=AudioObjective().cuda()(prediction.cuda(),target.cuda())
        self.assertAlmostEqual(float(cpu),float(gpu),places=4)
        for name in parts:
            self.assertAlmostEqual(float(parts[name]),float(gpu_parts[name]),places=4)
        net=Net(16,input_scale=16).cuda()
        x=np.random.default_rng(667).normal(0,.1,FS//2).astype(np.float32)
        expected=predict(net,x)
        with tempfile.TemporaryDirectory(dir=DATA) as directory:
            root=Path(directory)
            export(net,root/'cuda.json')
            save_checkpoint(net,root/'cuda.pt')
            self.assertTrue(all(value.device.type=='cpu' for value in torch.load(root/'cuda.pt',weights_only=True).values()))
            actual=native(root,x,'model',root/'cuda.json','cuda_export')
            np.testing.assert_allclose(actual,expected,atol=1e-5,rtol=0)

    def test_wider_model_preserves_response_and_native_export(self):
        torch.manual_seed(892)
        original=Net(8,input_scale=16)
        wider=widen(original,16)
        x=np.random.default_rng(803).normal(0,.15,FS//2).astype(np.float32)
        expected=predict(original,x)
        np.testing.assert_allclose(predict(wider,x),expected,atol=1e-6,rtol=0)
        with tempfile.TemporaryDirectory(dir=DATA) as directory:
            root=Path(directory)
            export(wider,root/'model.json')
            actual=native(root,x,'model',root/'model.json','wider')
            np.testing.assert_allclose(actual,expected,atol=1e-6,rtol=0)

    def test_manual_output_does_not_change_model_input_drive(self):
        torch.manual_seed(615)
        net=Net(8,input_scale=16)
        x=(.2*np.sin(2*np.pi*110*np.arange(FS*20)/FS)).astype(np.float32)
        *_,heldout,_=split_pair(x,x)
        with tempfile.TemporaryDirectory(dir=DATA) as directory:
            root=Path(directory)
            export(net,root/'model_rtneural.json')
            sf.write(str(root/'input.wav'),x,FS,subtype='FLOAT')
            sf.write(str(root/'target.wav'),x,FS,subtype='FLOAT')
            config={'reference':'pair','pair_input':str(root/'input.wav'),'pair_target':str(root/'target.wav'),'delay_samples':0,'source':str(root/'input.wav'),'source_name':'Gain routing fixture','filter':False,'placement':'after','action':'train','output_db':-12,'record_samples':FS}
            (root/'config.json').write_text(json.dumps(config))
            expected=run_records(lambda part,i:native(root,part,'model',root/'model_rtneural.json',f'expected{i}'),heldout,FS)*10**(-12/20)
            with patch('worker.train',return_value={'fixture':True}):
                main(root)
            actual,_=sf.read(str(root/'model.wav'),dtype='float32')
            np.testing.assert_array_equal(actual,expected)
            baseline,_=sf.read(str(root/'baseline.wav'),dtype='float32')
            np.testing.assert_array_equal(baseline,heldout*10**(-12/20))

    def test_sixty_second_split_has_exact_sample_boundaries(self):
        x=np.zeros(FS*60,np.float32)
        tx,_,vx,_,hx,_=split_pair(x,x)
        self.assertEqual([len(tx),len(vx),len(hx)],[FS*42,FS*9,FS*9])

    def test_training_sequences_never_cross_capture_resets(self):
        record=3*FS
        warm=12000
        block=4096
        cycle=training_layout(14*record,record,warm,block)
        starts=training_starts(np.random.default_rng(6),14*record,record,warm,block,cycle,512)
        self.assertTrue(np.all(starts%record==0))
        ends=starts+warm+cycle*block
        self.assertTrue(np.all(ends<=starts+record))
        self.assertTrue(np.all(ends<=14*record))

    def test_scoring_excludes_warmup_from_each_capture(self):
        record=FS
        target=np.full(record*2,.1,np.float32)
        prediction=target.copy()
        prediction[:12000]=3
        prediction[record:record+12000]=-3
        loss,_,error=score_records(AudioObjective(),prediction,target,record,12000)
        self.assertEqual(float(loss),0)
        self.assertEqual(error,0)

    def test_record_training_retains_checkpoint_and_exports_with_resets(self):
        net=Net(8,input_scale=16)
        with torch.no_grad():
            net.out.weight.zero_()
            net.out.bias.fill_(.125)
        with tempfile.TemporaryDirectory(dir=DATA) as directory:
            root=Path(directory)
            torch.save(net.state_dict(),root/'initial.pt')
            sf.write(str(root/'input.wav'),np.zeros(FS*20,np.float32),FS,subtype='FLOAT')
            sf.write(str(root/'target.wav'),np.full(FS*20,.125,np.float32),FS,subtype='FLOAT')
            config={'reference':'pair','pair_input':str(root/'input.wav'),'pair_target':str(root/'target.wav'),'delay_samples':0,'pair_description':'Independent constant-output fixtures','hidden':8,'input_scale':16,'initial_checkpoint':str(root/'initial.pt'),'steps':1,'source_name':'Fixture','drive_db':0,'record_samples':FS}
            report=train(root,config)
            self.assertEqual(report['validation_objective'],0)
            self.assertEqual(report['holdout_objective'],0)
            self.assertTrue(report['record_state_reset'])
            self.assertEqual(report['held_out_input_context_samples'],0)
            self.assertLess(report['export_parity_rmse'],1e-7)

    def test_signed_alignment_preserves_gain_and_splits(self):
        x = np.arange(FS*13, dtype=np.float32)
        y = np.pad(x*2, (73, 0))
        a, b = align_pair(x, y, 73)
        np.testing.assert_array_equal(b, a*2)
        b, a = align_pair(y, x, -73)
        np.testing.assert_array_equal(b, a*2)
        tx, ty, vx, vy, hx, hy = split_pair(a, b)
        np.testing.assert_array_equal(np.concatenate((tx, vx, hx)), a)
        np.testing.assert_array_equal(np.concatenate((ty, vy, hy)), b)
        self.assertEqual(len(tx), int(len(a)*0.7))

    def test_export_and_proteus_residual_match_native(self):
        torch.manual_seed(43)
        net = Net(8)
        x = np.random.default_rng(10).normal(0, 0.1, FS).astype(np.float32)
        DATA.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=DATA) as directory:
            root = Path(directory)
            path = root / 'export.json'
            export(net, path)
            raw = json.loads(path.read_text())
            converted, _ = convert_model(raw, 48000)
            path.write_text(json.dumps(converted))
            actual = native(root, x, 'model', path, 'direct')
            expected = predict(net, x)
            self.assertLess(float(np.max(abs(actual-expected))), 1e-5)
            state = net.state_dict()
            keys = {'rec.weight_ih_l0': 'lstm.weight_ih_l0', 'rec.weight_hh_l0': 'lstm.weight_hh_l0', 'rec.bias_ih_l0': 'lstm.bias_ih_l0', 'rec.bias_hh_l0': 'lstm.bias_hh_l0', 'lin.weight': 'out.weight', 'lin.bias': 'out.bias'}
            proteus = {'model_data': {'input_size': 1}, 'state_dict': {key: state[value].tolist() for key, value in keys.items()}}
            converted, info = convert_model(proteus, 48000)
            self.assertTrue(info['residual_input'])
            converted, info = convert_model(converted, 48000)
            self.assertTrue(info['residual_input'])
            path.write_text(json.dumps(converted))
            actual = native(root, x, 'model', path, 'residual')
            self.assertLess(float(np.max(abs(actual-expected-x))), 1e-5)
            converted['workshop_runtime']['sample_rate'] = 44100
            path.write_text(json.dumps(converted))
            actual = native(root, x, 'model', path, 'rate44100')
            self.assertEqual(actual.shape, x.shape)
            self.assertTrue(np.isfinite(actual).all())

    def test_rejects_conditioned_models_and_nonfinite_weights(self):
        with self.assertRaises(ValueError):
            convert_model({'model_data': {'input_size': 2}, 'state_dict': {}}, 44100)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'model.json'
            export(Net(8), path)
            value = json.loads(path.read_text())
            value['layers'][1]['weights'][1][0] = float('nan')
            with self.assertRaises(ValueError):
                convert_model(value, 48000)

    def test_input_conditioning_is_folded_into_export(self):
        net = Net(8, input_scale=16)
        x = np.random.default_rng(15).normal(0, 0.1, FS//4).astype(np.float32)
        with tempfile.TemporaryDirectory(dir=DATA) as directory:
            root = Path(directory)
            path = root/'conditioned.json'
            export(net, path)
            actual = native(root, x, 'model', path, 'conditioned')
            self.assertLess(float(np.max(abs(actual-predict(net,x)))), 1e-5)

    def test_starting_checkpoint_survives_without_optimizer_steps(self):
        net = Net(8, input_scale=16)
        with torch.no_grad():
            net.out.weight.zero_()
            net.out.bias.fill_(0.125)
        with tempfile.TemporaryDirectory(dir=DATA) as directory:
            root = Path(directory)
            initial = root / 'initial.pt'
            torch.save(net.state_dict(), initial)
            sf.write(str(root / 'input.wav'), np.zeros(FS*12, np.float32), FS, subtype='FLOAT')
            sf.write(str(root / 'target.wav'), np.full(FS*12, 0.125, np.float32), FS, subtype='FLOAT')
            config = {'reference': 'pair', 'pair_input': str(root/'input.wav'), 'pair_target': str(root/'target.wav'), 'delay_samples': 0, 'pair_description': 'Synthetic constant-output checkpoint fixture', 'hidden': 8, 'input_scale': 16, 'initial_checkpoint': str(initial), 'steps': 0, 'source_name': 'Unused paired-source label', 'drive_db': 0}
            report = train(root, config)
            self.assertEqual(report['validation_objective'], 0)
            self.assertEqual(report['initial_checkpoint_sha256'], sha(initial))
            self.assertEqual(report['history'][0]['step'], 0)
            actual = native(root, np.zeros(FS//4, np.float32), 'model', root/'model_rtneural.json', 'restored')
            np.testing.assert_allclose(actual, 0.125, rtol=0, atol=1e-7)


if __name__ == '__main__':
    unittest.main()

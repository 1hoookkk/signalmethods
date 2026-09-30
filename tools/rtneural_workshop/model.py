import json
from pathlib import Path
import numpy as np
import torch


class Net(torch.nn.Module):
    def __init__(self, hidden=16, input_scale=1.0):
        super().__init__()
        self.input_scale = float(input_scale)
        self.lstm = torch.nn.LSTM(1, hidden, batch_first=True)
        self.out = torch.nn.Linear(hidden, 1)
        with torch.no_grad():
            self.lstm.bias_ih_l0.zero_()
            self.lstm.bias_hh_l0.zero_()
            self.out.bias.zero_()

    def forward(self, x, state=None):
        y, state = self.lstm(x * self.input_scale, state)
        return self.out(y), state


def predict(net, x):
    chunks = []
    device=next(net.parameters()).device
    with torch.no_grad():
        _, state = net(torch.zeros(1, 12000, 1,device=device))
        for chunk in torch.tensor(np.asarray(x), dtype=torch.float32,device=device).split(8192):
            y, state = net(chunk.view(1, -1, 1), state)
            chunks.append(y.flatten().cpu().numpy())
    return np.concatenate(chunks)


def widen(net,hidden):
    original=net.lstm.hidden_size
    if hidden<=original:
        raise ValueError('The wider model must add hidden units')
    result=Net(hidden,input_scale=net.input_scale)
    with torch.no_grad():
        result.lstm.weight_hh_l0.zero_()
        result.lstm.weight_ih_l0.zero_()
        for gate in range(4):
            old=slice(gate*original,(gate+1)*original)
            new=slice(gate*hidden,gate*hidden+original)
            result.lstm.weight_ih_l0[new]=net.lstm.weight_ih_l0[old]
            result.lstm.weight_hh_l0[new,:original]=net.lstm.weight_hh_l0[old]
            result.lstm.bias_ih_l0[new]=net.lstm.bias_ih_l0[old]
            result.lstm.bias_hh_l0[new]=net.lstm.bias_hh_l0[old]
        result.lstm.bias_ih_l0[original:hidden]=2
        result.lstm.bias_ih_l0[hidden+original:2*hidden]=-4
        result.lstm.bias_ih_l0[3*hidden+original:4*hidden]=2
        slopes=torch.logspace(np.log10(.25),np.log10(8),hidden-original)
        slopes[1::2]*=-1
        result.lstm.weight_ih_l0[2*hidden+original:3*hidden,0]=slopes
        result.out.weight.zero_()
        result.out.weight[:,:original]=net.out.weight
        result.out.bias.copy_(net.out.bias)
    return result


def save_checkpoint(net,path):
    torch.save({key:value.detach().cpu() for key,value in net.state_dict().items()},path)


def export(net, path):
    state = {key:value.detach().cpu() for key,value in net.state_dict().items()}
    model = {'workshop_runtime': {'sample_rate': 48000, 'residual_input': False}, 'in_shape': [None, None, 1], 'layers': [
        {'type': 'lstm', 'activation': '', 'shape': [None, None, net.lstm.hidden_size], 'weights': [(state['lstm.weight_ih_l0'] * net.input_scale).numpy().T.tolist(), state['lstm.weight_hh_l0'].numpy().T.tolist(), (state['lstm.bias_ih_l0']+state['lstm.bias_hh_l0']).numpy().tolist()]},
        {'type': 'dense', 'activation': '', 'shape': [None, None, 1], 'weights': [state['out.weight'].numpy().T.tolist(), state['out.bias'].numpy().tolist()]}
    ]}
    Path(path).write_text(json.dumps(model))

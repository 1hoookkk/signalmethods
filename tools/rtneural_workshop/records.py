import numpy as np


def spans(length,record_samples):
    if record_samples and (record_samples<=0 or length%record_samples):
        raise ValueError('Capture records must divide the audio exactly')
    size=record_samples or length
    if size<=0:
        raise ValueError('Empty capture')
    return [(start,start+size) for start in range(0,length,size)]


def run_records(function,x,record_samples):
    parts=[]
    for i,(a,b) in enumerate(spans(len(x),record_samples)):
        value=np.asarray(function(x[a:b],i))
        if value.shape!=x[a:b].shape or not np.isfinite(value).all():
            raise ValueError('Incomplete or nonfinite capture prediction')
        parts.append(value)
    return np.concatenate(parts)


def training_layout(length,record_samples,warm,block):
    spans(length,record_samples)
    cycle=(record_samples-warm)//block if record_samples else 16
    if cycle<1 or (not record_samples and length<=warm+cycle*block):
        raise ValueError('Capture is too short for training context and blocks')
    return cycle


def training_starts(rng,length,record_samples,warm,block,cycle,batch,random_context=False):
    if record_samples:
        starts=rng.integers(0,length//record_samples,batch)*record_samples
        if random_context:
            starts+=rng.integers(warm,record_samples-cycle*block+1,batch)
        return starts
    return rng.integers(0,length-warm-cycle*block,batch)


def prime_records(net,data,indices,record_samples,warm=12000,chunk=8192):
    import torch
    bases=indices//record_samples*record_samples
    lengths=indices-bases
    unique_bases,record_indices=np.unique(bases,return_inverse=True)
    previous=net.lstm.training
    net.lstm.eval()
    try:
        with torch.no_grad():
            _,state=net.lstm(torch.zeros(len(unique_bases),warm,1,device=data.device))
            result=tuple(torch.empty(1,len(indices),net.lstm.hidden_size,device=data.device) for _ in state)
            offset=0
            for end in np.unique(lengths):
                while offset<end:
                    stop=min(offset+chunk,int(end))
                    values=torch.stack([data[base+offset:base+stop] for base in unique_bases]).unsqueeze(-1)*net.input_scale
                    _,state=net.lstm(values,state)
                    offset=stop
                rows=np.flatnonzero(lengths==end)
                destinations=torch.tensor(rows,device=data.device)
                sources=torch.tensor(record_indices[rows],device=data.device)
                for output,current in zip(result,state):
                    output.index_copy_(1,destinations,current.index_select(1,sources))
        return result
    finally:
        net.lstm.train(previous)


def score_records(objective,prediction,target,record_samples,warm):
    import torch
    if prediction.shape!=target.shape:
        raise ValueError('Prediction and target shapes disagree')
    totals=None
    device=next(objective.buffers()).device
    count=0
    error=power=0.
    with torch.no_grad():
        for a,b in spans(len(target),record_samples):
            p,t=prediction[a+warm:b],target[a+warm:b]
            if not len(t):
                raise ValueError('Capture has no samples after warmup')
            loss,parts=objective.evaluate(torch.from_numpy(p).to(device),torch.from_numpy(t).to(device))
            row=torch.stack((loss,parts['mr_spectral'],parts['lf_esr']))*len(t)
            totals=row if totals is None else totals+row
            count+=len(t)
            error+=float(np.sum((p.astype(np.float64)-t)**2))
            power+=float(np.sum(t.astype(np.float64)**2))
    totals/=count
    return totals[0],{'mr_spectral':totals[1],'lf_esr':totals[2]},error/max(power,1e-12)

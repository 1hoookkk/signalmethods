#include "Peevers.h"

extern "C" {

__declspec(dllexport) hs::Peevers* peevers_create()
{
    return new hs::Peevers();
}

__declspec(dllexport) void peevers_destroy(hs::Peevers* p)
{
    delete p;
}

__declspec(dllexport) void peevers_set_parms(hs::Peevers* p, int nfft, int winsize, int hop, int wintype, int lpcenv, float avgk)
{
    if (!p) return;
    p->lpcenv = lpcenv;
    p->avgk = avgk;
    p->setParms(nfft, winsize, hop, wintype);
}

__declspec(dllexport) void peevers_process_frame(hs::Peevers* p, const float* in, float* out_fx, float* out_avg, float* out_k)
{
    if (!p || !in) return;
    p->averagedFrame(in);
    const int n2 = p->nfft2;
    if (out_fx)
    {
        for (int i = 0; i <= n2; ++i)
            out_fx[i] = p->fx[(size_t) i];
    }
    if (out_avg)
    {
        for (int i = 0; i <= n2; ++i)
            out_avg[i] = p->avg[(size_t) i];
    }
    if (out_k)
    {
        for (int i = 0; i < 13; ++i)
            out_k[i] = p->lpc.k[(size_t) i];
    }
}

__declspec(dllexport) void peevers_reset_avg(hs::Peevers* p)
{
    if (!p) return;
    p->xavg(p->fx.data(), p->nfft2, 1);
}

}

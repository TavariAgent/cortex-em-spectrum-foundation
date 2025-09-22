#include "spectrum_cuda.hpp"

#ifdef CUDA_AVAILABLE
#include <cuda_runtime.h>
#include <cmath>
#include <stdint.h>

namespace cortex {

static __device__ __forceinline__ float clamp01f(float v){ return v<0.f?0.f:(v>1.f?1.f:v); }

static __device__ __forceinline__ void base_rgb(float wl, float& r, float& g, float& b) {
    r=g=b=0.f;
    if (wl>=380.f && wl<440.f){ r=-(wl-440.f)/60.f; b=1.f; }
    else if (wl>=440.f && wl<490.f){ g=(wl-440.f)/50.f; b=1.f; }
    else if (wl>=490.f && wl<510.f){ g=1.f; b=-(wl-510.f)/20.f; }
    else if (wl>=510.f && wl<580.f){ r=(wl-510.f)/70.f; g=1.f; }
    else if (wl>=580.f && wl<645.f){ r=1.f; g=-(wl-645.f)/65.f; }
    else if (wl>=645.f && wl<=750.f){ r=1.f; }
    r=clamp01f(r); g=clamp01f(g); b=clamp01f(b);
}

static __device__ __forceinline__ float intensity_term(float wl) {
    float I=1.f;
    if (wl>=380.f && wl<420.f) I = 0.3f + 0.7f*(wl-380.f)/40.f;
    else if (wl>=701.f && wl<=750.f) I = 0.3f + 0.7f*(750.f-wl)/49.f;
    return I;
}

static __device__ __forceinline__ uint32_t xorshift32(uint32_t& s){ s^=s<<13; s^=s>>17; s^=s<<5; return s; }
static __device__ __forceinline__ float rand01(uint32_t& s){ return (xorshift32(s)&0x00FFFFFF)*(1.0f/16777216.0f); }

__global__ void spectrum_kernel(SpectrumCudaParams p, float* out_rgb) {
    int x = blockIdx.x*blockDim.x + threadIdx.x;
    int y = blockIdx.y*blockDim.y + threadIdx.y;
    if (x>=p.width || y>=p.height) return;

    const int sppX = max(1, p.spp_x);
    const int sppY = max(1, p.spp_y);
    const int spp  = sppX*sppY;

    float acc_r=0.f, acc_g=0.f, acc_b=0.f;
    uint32_t seed = (uint32_t)((y*p.width+x)*9781u + 0x9E3779B9u);

    for (int sy=0; sy<sppY; ++sy){
        for (int sx=0; sx<sppX; ++sx){
            float jx = p.jitter ? rand01(seed) : 0.5f;
            float fx = (sx + jx) / (float)sppX;
            float xN = (x + fx) / (float)p.width;
            float wl = (float)p.wl_min + ((float)p.wl_max - (float)p.wl_min) * xN;

            float r,g,b;
            base_rgb(wl, r,g,b);
            float I = intensity_term(wl);
            float inv_gamma = (p.gamma>0.0) ? (1.0f/(float)p.gamma) : (1.0f/2.2f);
            r = powf(r*I, inv_gamma);
            g = powf(g*I, inv_gamma);
            b = powf(b*I, inv_gamma);

            acc_r += r; acc_g += g; acc_b += b;
        }
    }

    float scale = 1.f/(float)spp;
    int idx = (y*p.width + x)*3;
    out_rgb[idx+0] = acc_r*scale;
    out_rgb[idx+1] = acc_g*scale;
    out_rgb[idx+2] = acc_b*scale;
}

bool spectrum_shade_cuda(const SpectrumCudaParams& p, float* out_rgb){
    if (!out_rgb || p.width<=0 || p.height<=0) return false;
    float* d_out=nullptr;
    size_t bytes = (size_t)p.width*(size_t)p.height*3*sizeof(float);
    if (cudaMalloc(&d_out, bytes)!=cudaSuccess) return false;

    dim3 block(16,16);
    dim3 grid((p.width+block.x-1)/block.x, (p.height+block.y-1)/block.y);
    spectrum_kernel<<<grid,block>>>(p, d_out);
    cudaError_t e = cudaDeviceSynchronize();

    bool ok = (e==cudaSuccess);
    if (ok) ok = (cudaMemcpy(out_rgb, d_out, bytes, cudaMemcpyDeviceToHost)==cudaSuccess);
    cudaFree(d_out);
    return ok;
}

} // namespace cortex
#else
namespace cortex {
bool spectrum_shade_cuda(const SpectrumCudaParams&, float*) { return false; }
}
#endif
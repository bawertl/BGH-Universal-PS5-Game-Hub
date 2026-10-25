// Standalone deterministic BC7 mode-6 encoder for PS5 sce_sys/pic*.dds.
// Copyright (c) BGH project. No external library; input: raw RGBA 8-bit, output: 3840x2160 BC7 DX10 DDS.
// Build on a development host: c++ -std=c++17 -O3 tooling/encode_ps5_bc7_mode6.cpp -o /tmp/bgh-bc7
// Invoke: /tmp/bgh-bc7 input_3840x2160.rgba output.dds
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

constexpr int W=3840, H=2160;
constexpr std::array<int,16> WEIGHTS = {0,4,9,13,17,21,26,30,34,38,43,47,51,55,60,64};
using Pixel=std::array<uint8_t,4>;

struct BitWriter {
    uint8_t bits[16]{};
    int pos=0;
    void write(unsigned value, unsigned width) {
        for(unsigned n=0;n<width;n++,pos++) if((value>>n)&1u) bits[pos>>3] |= (1u<<(pos&7));
    }
};

static std::array<uint8_t,16> block_encode(const std::array<Pixel,16>& px) {
    double mean[3]={}, minc[3]={255,255,255}, maxc[3]={};
    for(const auto& p:px) for(int c=0;c<3;c++) {
        mean[c]+=p[c]/16.0;
        minc[c]=std::min<double>(minc[c],p[c]);
        maxc[c]=std::max<double>(maxc[c],p[c]);
    }
    double cov[3][3]{};
    for(const auto& p:px) for(int i=0;i<3;i++) for(int j=0;j<3;j++) cov[i][j]+=(p[i]-mean[i])*(p[j]-mean[j]);
    double vec[3]={maxc[0]-minc[0],maxc[1]-minc[1],maxc[2]-minc[2]};
    auto normalize=[](double* v){
        double norm=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
        if(norm<1e-9){v[0]=1;v[1]=0;v[2]=0;return;}
        for(int i=0;i<3;i++) v[i]/=norm;
    };
    normalize(vec);
    for(int iteration=0;iteration<5;iteration++){
        double next[3]{};
        for(int i=0;i<3;i++) for(int j=0;j<3;j++) next[i]+=cov[i][j]*vec[j];
        normalize(next);
        for(int i=0;i<3;i++) vec[i]=next[i];
    }
    double low=1e20, high=-1e20;
    for(const auto& p:px){
        double projection=0;
        for(int c=0;c<3;c++) projection+=(p[c]-mean[c])*vec[c];
        low=std::min(low,projection);
        high=std::max(high,projection);
    }
    double ep[2][3]{};
    for(int c=0;c<3;c++){
        ep[0][c]=std::clamp(mean[c]+low*vec[c],0.0,255.0);
        ep[1][c]=std::clamp(mean[c]+high*vec[c],0.0,255.0);
    }
    uint8_t index[16]{};
    uint8_t q[2][3]{};
    auto assign_indices=[&](){
        for(int e=0;e<2;e++) for(int c=0;c<3;c++) {
            // P-bit 1 makes each decompressed endpoint channel an odd 8-bit value.
            const int approx=std::clamp(int(std::round(ep[e][c])),1,255);
            q[e][c]=std::clamp((approx-1+1)/2,0,127);
        }
        for(int p=0;p<16;p++){
            int best=0, least=INT32_MAX;
            for(int k=0;k<16;k++){
                const int weight=WEIGHTS[k];
                int err=0;
                for(int c=0;c<3;c++) {
                    const int a=q[0][c]*2+1, b=q[1][c]*2+1;
                    const int y=((64-weight)*a+weight*b+32)>>6;
                    const int d=y-px[p][c];
                    err+=d*d;
                }
                if(err<least){least=err;best=k;}
            }
            index[p]=best;
        }
    };
    assign_indices();
    // Least-squares endpoint refinement using palette interpolation weights.
    double aa=0,ab=0,bb=0;
    double ax[3]{},bx[3]{};
    for(int p=0;p<16;p++){
        const double t=WEIGHTS[index[p]]/64.0, s=1.0-t;
        aa+=s*s; ab+=s*t; bb+=t*t;
        for(int c=0;c<3;c++){ax[c]+=s*px[p][c];bx[c]+=t*px[p][c];}
    }
    const double det=aa*bb-ab*ab;
    if(det>1e-6){
        for(int c=0;c<3;c++){
            ep[0][c]=std::clamp((ax[c]*bb-bx[c]*ab)/det,0.0,255.0);
            ep[1][c]=std::clamp((bx[c]*aa-ax[c]*ab)/det,0.0,255.0);
        }
        assign_indices();
    }
    // BC7 has an implicit high bit 0 for pixel 0, thus its index must be below 8.
    if(index[0]>=8){
        for(int c=0;c<3;c++) std::swap(q[0][c],q[1][c]);
        for(uint8_t& idx:index) idx=15-idx;
    }
    BitWriter writer;
    writer.write(1u<<6,7);               // Mode 6: unary mode selector (bit 6 = 1)
    for(int c=0;c<3;c++){writer.write(q[0][c],7);writer.write(q[1][c],7);}
    writer.write(127,7);writer.write(127,7);  // opaque A = 255 using P-bit 1
    writer.write(1,1);writer.write(1,1);     // two per-endpoint P bits
    for(int i=0;i<16;i++) writer.write(index[i],i==0?3:4);
    if(writer.pos!=128) throw std::runtime_error("Invalid BC7 block length");
    std::array<uint8_t,16> out{};
    std::copy(writer.bits,writer.bits+16,out.begin());
    return out;
}

void le32(std::ofstream& out,uint32_t value){
    char b[4]={(char)value,(char)(value>>8),(char)(value>>16),(char)(value>>24)};
    out.write(b,4);
}
void dds_header(std::ofstream& out){
    out.write("DDS ",4);
    le32(out,124);
    le32(out,0x81007); // caps | height | width | pixelformat | linearsize
    le32(out,H);le32(out,W);le32(out,W*H);le32(out,0);le32(out,1); // single base mip level; no additional mipmaps
    for(int i=0;i<11;i++)le32(out,0);
    le32(out,32);le32(out,4);out.write("DX10",4);
    for(int i=0;i<5;i++)le32(out,0);
    le32(out,0x1000); // DDSCAPS_TEXTURE
    for(int i=0;i<4;i++)le32(out,0);
    // DDS_HEADER_DXT10: DXGI_FORMAT_BC7_UNORM, D3D10_RESOURCE_DIMENSION_TEXTURE2D, array size 1.
    le32(out,98);le32(out,3);le32(out,0);le32(out,1);le32(out,0);
}
int main(int argc,char** argv){
    if(argc!=3){std::cerr<<"Usage: bgh-bc7 INPUT_RGBA_3840x2160 OUTPUT_DDS\n";return 2;}
    std::ifstream in(argv[1],std::ios::binary);
    if(!in){std::cerr<<"Could not read input\n";return 2;}
    std::vector<uint8_t> raw(size_t(W)*H*4);
    in.read((char*)raw.data(),std::streamsize(raw.size()));
    if(in.gcount()!=std::streamsize(raw.size()) || in.peek()!=EOF){
        std::cerr<<"Incorrect input dimensions or RGBA buffer size\n";return 2;
    }
    std::ofstream out(argv[2],std::ios::binary);
    if(!out){std::cerr<<"Could not write output\n";return 2;}
    dds_header(out);
    for(int by=0;by<H;by+=4){
        for(int bx=0;bx<W;bx+=4){
            std::array<Pixel,16> px{};
            for(int y=0;y<4;y++)for(int x=0;x<4;x++){
                const size_t offset=(size_t(by+y)*W+(bx+x))*4;
                for(int c=0;c<4;c++) px[y*4+x][c]=raw[offset+c];
            }
            auto encoded=block_encode(px);
            out.write((const char*)encoded.data(),16);
        }
    }
    if(!out){std::cerr<<"Output truncated\n";return 2;}
    return 0;
}

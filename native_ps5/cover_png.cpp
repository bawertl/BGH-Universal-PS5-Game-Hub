// SPDX-License-Identifier: MIT
#include "cover_png.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <zlib.h>
namespace ulg {namespace {
std::uint32_t be32(const std::uint8_t *p){return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3];}
int paeth(int a,int b,int c){const int p=a+b-c,pa=std::abs(p-a),pb=std::abs(p-b),pc=std::abs(p-c);return pa<=pb&&pa<=pc?a:(pb<=pc?b:c);}
} // namespace
bool decode_png(const std::string &filename,PngImage &result,int max_edge){
    result={};
    FILE *in=std::fopen(filename.c_str(),"rb");if(!in)return false;
    if(std::fseek(in,0,SEEK_END)!=0){std::fclose(in);return false;}
    const long length=std::ftell(in);
    if(length<33 || length>12*1024*1024 || std::fseek(in,0,SEEK_SET)!=0){std::fclose(in);return false;}
    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    const bool ok=std::fread(data.data(),1,data.size(),in)==data.size();
    std::fclose(in);if(!ok)return false;
    static const unsigned char signature[]={137,80,78,71,13,10,26,10};
    if(std::memcmp(data.data(),signature,8)!=0)return false;
    int width=0,height=0,channels=0,color=0;
    bool ihdr=false,end=false;
    std::vector<std::uint8_t> compressed,palette,transparency;
    std::size_t pos=8;
    while(pos+12<=data.size()){
        std::uint32_t size=be32(data.data()+pos);pos+=4;
        if(size>data.size()-pos-8)return false;
        const auto *type=data.data()+pos;pos+=4;
        const auto *payload=data.data()+pos;pos+=size;
        const auto received_crc=be32(data.data()+pos);pos+=4;
        uLong crc=crc32(0L,Z_NULL,0);
        crc=crc32(crc,type,4);crc=crc32(crc,payload,size);
        if(received_crc!=static_cast<std::uint32_t>(crc))return false;
        if(std::memcmp(type,"IHDR",4)==0){
            if(ihdr||size!=13)return false;
            ihdr=true;
            width=static_cast<int>(be32(payload));height=static_cast<int>(be32(payload+4));
            if(width<1||height<1||width>2048||height>2048 || payload[8]!=8 ||
               payload[10]!=0 || payload[11]!=0 || payload[12]!=0)return false;
            color=payload[9];
            channels=color==0?1:color==2?3:color==3?1:color==4?2:color==6?4:0;
            if(!channels)return false;
        }else if(std::memcmp(type,"PLTE",4)==0){
            if(size%3!=0 || size>768)return false;
            palette.assign(payload,payload+size);
        }else if(std::memcmp(type,"tRNS",4)==0){
            if(size>256)return false;
            transparency.assign(payload,payload+size);
        }else if(std::memcmp(type,"IDAT",4)==0){
            if(!ihdr || compressed.size()+size>12*1024*1024)return false;
            compressed.insert(compressed.end(),payload,payload+size);
        }else if(std::memcmp(type,"IEND",4)==0){end=true;break;}
        else if((type[0]&32)==0)return false; // Unknown critical chunk.
    }
    if(!end||!ihdr||compressed.empty()||(color==3&&palette.empty()))return false;
    const std::size_t stride=static_cast<std::size_t>(width)*channels;
    const std::size_t unpacked_size=(stride+1)*static_cast<std::size_t>(height);
    if(unpacked_size>32*1024*1024)return false;
    std::vector<std::uint8_t> packed(unpacked_size);
    uLongf count=static_cast<uLongf>(packed.size());
    if(uncompress(packed.data(),&count,compressed.data(),static_cast<uLong>(compressed.size()))!=Z_OK||count!=packed.size())return false;
    std::vector<std::uint8_t> filtered(stride*static_cast<std::size_t>(height));
    for(int row=0;row<height;++row){
        const auto src=packed.data()+static_cast<std::size_t>(row)*(stride+1);
        auto dst=filtered.data()+static_cast<std::size_t>(row)*stride;
        const auto previous=row?filtered.data()+static_cast<std::size_t>(row-1)*stride:nullptr;
        if(src[0]>4)return false;
        for(std::size_t x=0;x<stride;++x){
            const int left=x>=static_cast<std::size_t>(channels)?dst[x-channels]:0;
            const int up=previous?previous[x]:0;
            const int upper_left=previous&&x>=static_cast<std::size_t>(channels)?previous[x-channels]:0;
            int predictor=0;
            switch(src[0]){case 1:predictor=left;break;case 2:predictor=up;break;
                case 3:predictor=(left+up)/2;break;case 4:predictor=paeth(left,up,upper_left);break;}
            dst[x]=static_cast<std::uint8_t>(src[x+1]+predictor);
        }
    }
    const int limit=max_edge>0?max_edge:std::max(width,height);
    const int out_width=std::min(width,limit),out_height=std::min(height,limit);
    PngImage decoded;decoded.width=out_width;decoded.height=out_height;
    decoded.rgba.resize(static_cast<std::size_t>(out_width)*out_height*4);
    for(int y=0;y<out_height;++y){
        const int yy=static_cast<int>((static_cast<std::int64_t>(y)*height)/out_height);
        for(int x=0;x<out_width;++x){
            const int xx=static_cast<int>((static_cast<std::int64_t>(x)*width)/out_width);
            const std::uint8_t *source=filtered.data()+(static_cast<std::size_t>(yy)*width+xx)*channels;
            std::uint8_t *dest=decoded.rgba.data()+(static_cast<std::size_t>(y)*out_width+x)*4;
            dest[3]=255;
            if(color==0){dest[0]=dest[1]=dest[2]=source[0];}
            else if(color==2){dest[0]=source[0];dest[1]=source[1];dest[2]=source[2];}
            else if(color==3){const std::size_t index=source[0];if(index*3+2>=palette.size())return false;
                dest[0]=palette[index*3];dest[1]=palette[index*3+1];dest[2]=palette[index*3+2];
                if(index<transparency.size())dest[3]=transparency[index];}
            else if(color==4){dest[0]=dest[1]=dest[2]=source[0];dest[3]=source[1];}
            else {std::memcpy(dest,source,4);}
        }
    }
    result=std::move(decoded);return true;
}
bool decode_png(const std::string &filename,PngImage &result){
    return decode_png(filename,result,512);
}
} // namespace ulg

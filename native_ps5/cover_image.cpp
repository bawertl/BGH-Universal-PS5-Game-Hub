// SPDX-License-Identifier: MIT
#include "cover_image.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <vector>
#include <utility>
// stb_image is fetched from a pinned public source commit in the native build,
// not used for the already functioning legacy PNG renderer.
#define STBI_ONLY_JPEG
#define STBI_ONLY_TGA
#define STBI_NO_SIMD
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
namespace ulg {
namespace {
std::string file_extension(const std::string &file){
    const auto dot=file.find_last_of('.');if(dot==std::string::npos)return {};
    std::string extension=file.substr(dot);
    for(char &c:extension)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return extension;
}
}
namespace {
bool decode_image_impl(const std::string &file,PngImage &result,std::string *failure,int max_edge){
    result={};
    if(failure)failure->clear();
    const auto fail=[&](const std::string &reason){if(failure)*failure=reason;return false;};
    const auto extension=file_extension(file);
    if(extension==".png"){
        if(decode_png(file,result,max_edge))return true;
        return fail("PNG-DEKODIERUNG FEHLGESCHLAGEN");
    }
    if(extension!=".jpg" && extension!=".jpeg" && extension!=".tga")return fail("DATEIENDUNG NICHT UNTERSTUETZT");
    // BGH V0.12.5: PS5 can allow stdio while rejecting path metadata calls.
    // Read the actual stream, bounded to 8 MiB; no stat/lstat prerequisite.
    constexpr std::size_t max_bytes=8u*1024u*1024u;
    FILE *in=std::fopen(file.c_str(),"rb");
    if(!in){const int error=errno;return fail("OEFFNEN: "+std::string(std::strerror(error)));}
    std::vector<unsigned char> bytes;
    unsigned char block[8192];
    for(;;){
        const std::size_t count=std::fread(block,1,sizeof(block),in);
        const int read_error=errno;
        if(std::ferror(in)){
            std::fclose(in);
            return fail("LESEN: "+std::string(std::strerror(read_error)));
        }
        if(count>max_bytes-bytes.size()){
            std::fclose(in);return fail("BILDDATEI GROESSER ALS 8 MIB");
        }
        bytes.insert(bytes.end(),block,block+count);
        if(std::feof(in))break;
        if(count==0){std::fclose(in);return fail("LESEN: KEIN FORTSCHRITT");}
    }
    if(std::fclose(in)!=0){const int error=errno;return fail("SCHLIESSEN: "+std::string(std::strerror(error)));}
    if(bytes.size()<24)return fail("BILDDATEI ZU KLEIN");
    int width=0,height=0,channels=0;
    if(!stbi_info_from_memory(bytes.data(),static_cast<int>(bytes.size()),&width,&height,&channels)){
        const char *why=stbi_failure_reason();
        return fail(std::string("STB BILDINFO: ")+(why?why:"UNBEKANNTER FEHLER"));
    }
    if(width<1 || height<1 || width>4096 || height>4096 ||
       static_cast<std::uint64_t>(width)*height>8u*1024u*1024u)return fail("BILDDIMENSION ZU GROSS/UNGUELTIG");
    int decoded_width=0,decoded_height=0,n=0;
    unsigned char *rgba=stbi_load_from_memory(bytes.data(),static_cast<int>(bytes.size()),
                                               &decoded_width,&decoded_height,&n,4);
    if(!rgba){
        const char *why=stbi_failure_reason();
        return fail(std::string("STB DEKODIERUNG: ")+(why?why:"UNBEKANNTER FEHLER"));
    }
    if(decoded_width!=width || decoded_height!=height){stbi_image_free(rgba);return fail("BILDDIMENSION NACH DEKODIERUNG ABWEICHEND");}
    const int limit=max_edge>0?max_edge:std::max(width,height);
    PngImage decoded;
    decoded.width=std::min(width,limit);decoded.height=std::min(height,limit);
    decoded.rgba.resize(static_cast<std::size_t>(decoded.width)*decoded.height*4);
    for(int y=0;y<decoded.height;++y){
        const int from_y=static_cast<int>((static_cast<std::int64_t>(y)*height)/decoded.height);
        for(int x=0;x<decoded.width;++x){
            const int from_x=static_cast<int>((static_cast<std::int64_t>(x)*width)/decoded.width);
            std::memcpy(decoded.rgba.data()+(static_cast<std::size_t>(y)*decoded.width+x)*4,
                        rgba+(static_cast<std::size_t>(from_y)*width+from_x)*4,4);
        }
    }
    stbi_image_free(rgba);result=std::move(decoded);return true;
}
}
bool decode_cover_image(const std::string &file,PngImage &result,std::string *failure){
    return decode_image_impl(file,result,failure,512);
}
bool decode_ui_image(const std::string &file,PngImage &result,std::string *failure){
    return decode_image_impl(file,result,failure,0);
}
}

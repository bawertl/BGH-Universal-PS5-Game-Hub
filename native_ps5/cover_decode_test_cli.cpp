// Host-only diagnostics for the exact JPEG/TGA decoder used in native PS5.
#include "cover_image.hpp"
#include <iostream>
#include <string>
int main(int argc,char **argv){
    if(argc!=3)return 2;
    ulg::PngImage image;
    std::string failure;
    const bool success=ulg::decode_cover_image(argv[2],image,&failure);
    if(std::string(argv[1])=="valid"){
        if(!success){std::cerr<<failure<<'\n';return 1;}
        if(image.width<1 || image.height<1 || image.rgba.size()!=static_cast<std::size_t>(image.width*image.height*4))return 1;
        std::cout<<"DECODE_OK "<<image.width<<"x"<<image.height<<'\n';return 0;
    }
    if(std::string(argv[1])=="invalid")return success?1:0;
    return 2;
}

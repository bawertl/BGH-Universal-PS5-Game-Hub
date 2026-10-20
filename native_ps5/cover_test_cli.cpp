#include "cover_png.hpp"
#include <iostream>
int main(int argc,char **argv){
 if(argc!=2)return 2;
 ulg::PngImage image;
 if(!ulg::decode_png(argv[1],image)){std::cerr<<"invalid png\n";return 1;}
 std::cout<<image.width<<"x"<<image.height<<" pixels="<<image.rgba.size()<<"\n";
 return image.rgba.size()==static_cast<std::size_t>(image.width)*image.height*4?0:3;
}

// BGH V0.11: PS5SX2/ProsperoEden artwork lookup with native and read-only FTP fallback.
// SPDX-License-Identifier: MIT
#include "emulator_covers.hpp"
#include "ftp_readonly.hpp"
#ifdef BGH_TARGET_PS5
#include "cover_image.hpp"
#endif
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <map>
#include <set>
#include <string>
#include <sys/stat.h>
#include <vector>
namespace ulg { namespace {
constexpr std::size_t kMaxImage = 8u*1024u*1024u;
std::string basename(const std::string &path) {
    auto at=path.find_last_of('/'); return at==std::string::npos?path:path.substr(at+1);
}
std::string stem(std::string name) {
    const auto at=name.find_last_of('.');if(at!=std::string::npos)name.resize(at);return name;
}
bool safe_component(const std::string &name) {
    if(name.empty() || name.size()>250 || name=="." || name=="..")return false;
    for(unsigned char c:name)if(c<32||c==127||c=='/'||c=='\\')return false;
    return true;
}
std::string hex32(std::uint32_t value) {
    char digits[9]{};
    std::snprintf(digits, sizeof(digits), "%08x", static_cast<unsigned>(value));
    return digits;
}
std::uint32_t be32(const unsigned char *p) {
    return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3];
}
// Private BGH cache reads must not depend on directory listing or lstat():
// PS5 virtual mounts may allow opening a known file without exposing metadata.
bool read_private_cache(const std::string &path,std::string &out){
    out.clear();
    FILE *file=std::fopen(path.c_str(),"rb");
    if(!file)return false;
    char block[8192];
    bool ok=true;
    while(true){
        const std::size_t n=std::fread(block,1,sizeof(block),file);
        if(n){
            if(out.size()>kMaxImage-n){ok=false;break;}
            out.append(block,n);
        }
        if(n<sizeof(block)){
            if(std::ferror(file))ok=false;
            break;
        }
    }
    if(std::fclose(file)!=0)ok=false;
    if(!ok || out.empty()){out.clear();return false;}
    return true;
}
bool read_native(const std::string &path,std::string &out,std::size_t cap) {
    struct stat st{};if(lstat(path.c_str(),&st)!=0||!S_ISREG(st.st_mode)||st.st_size<=0||st.st_size>static_cast<off_t>(cap))return false;
    FILE *file=fopen(path.c_str(),"rb");if(!file)return false;
    out.resize(static_cast<std::size_t>(st.st_size));
    const bool ok=fread(out.data(),1,out.size(),file)==out.size();fclose(file);
    if(!ok)out.clear();
    return ok;
}
bool read_prefix_native(const std::string &path,std::string &out,std::size_t cap){
    struct stat st{};if(lstat(path.c_str(),&st)!=0||!S_ISREG(st.st_mode)||st.st_size<24)return false;
    FILE *file=fopen(path.c_str(),"rb");if(!file)return false;
    out.resize(std::min(cap,static_cast<std::size_t>(st.st_size)));
    const bool ok=fread(out.data(),1,out.size(),file)==out.size();fclose(file);
    if(!ok)out.clear();
    return ok;
}
std::string ext(const std::string &path){
    auto dot=path.find_last_of('.');if(dot==std::string::npos)return {};
    std::string e=path.substr(dot);
    for(char &c:e)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e;
}
std::string serial_ps2(std::string serial){
    // PS5SX2 normalizes SLUS_213.51 => SLUS-21351. Only strict serials are trusted.
    for(char &c:serial)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if(serial.size()==11 && serial[4]=='_' && serial[8]=='.') {
        serial=serial.substr(0,4)+"-"+serial.substr(5,3)+serial.substr(9,2);
    }
    if(serial.size()!=10 || serial[4]!='-')return {};
    for(int i=0;i<10;++i)if(i!=4 && (i<4 ? serial[i]<'A'||serial[i]>'Z' : serial[i]<'0'||serial[i]>'9'))return {};
    return serial;
}
// Conservative ISO9660 SYSTEM.CNF reader: PS5SX2 itself extracts BOOT2 serials this way.
// Works only with normal 2048-byte-sector .iso, not compressed .chd/.cso/.zso.
bool read_at_native(FILE *file,std::uint64_t offset,void *buffer,std::size_t length){
    if(offset>static_cast<std::uint64_t>(INT64_MAX))return false;
    return fseeko(file,static_cast<off_t>(offset),SEEK_SET)==0 && fread(buffer,1,length,file)==length;
}
bool read_at_game(FILE *file,const std::string &path,int port,
                  std::uint64_t offset,void *buffer,std::size_t length){
    if(file && read_at_native(file,offset,buffer,length))return true;
    FtpReadonly ftp(port);
    std::string bytes;
    if(ftp.retrieve_range(path,offset,bytes,length)){
        std::memcpy(buffer,bytes.data(),length);return true;
    }
    // Some PS5 FTP services allow RETR but reject REST. ISO9660's descriptor,
    // SYSTEM.CNF and the usual root directory fit in a bounded 256 KiB prefix.
    // Never download the entire game image as a fallback.
    constexpr std::size_t kPrefixCap=256u*1024u;
    if(offset>kPrefixCap || length>kPrefixCap-offset)return false;
    const auto total=static_cast<std::size_t>(offset)+length;
    FtpReadonly fallback(port);
    if(!fallback.retrieve_range(path,0,bytes,total))return false;
    std::memcpy(buffer,bytes.data()+static_cast<std::size_t>(offset),length);
    return true;
}
bool read_iso_sector(FILE *file,const std::string &path,int ftp_port,std::uint32_t sector,unsigned char *dst){
    return read_at_game(file,path,ftp_port,static_cast<std::uint64_t>(sector)*2048u,dst,2048);
}
std::uint32_t le32(const unsigned char *p){
    return std::uint32_t(p[0]) | (std::uint32_t(p[1])<<8) | (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24);
}
std::string serial_from_cnf(std::string content){
    const auto pos=content.find("BOOT2");
    if(pos==std::string::npos)return {};
    const auto line_end=content.find_first_of("\r\n",pos);
    std::string line=content.substr(pos,line_end==std::string::npos?std::string::npos:line_end-pos);
    const auto found=line.find("cdrom0:");
    if(found==std::string::npos)return {};
    const auto start=line.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",found+7);
    if(start==std::string::npos)return {};
    const std::string id=line.substr(start,11);
    return serial_ps2(id);
}
std::string iso_serial(const std::string &path,int ftp_port){
    if(ext(path)!=".iso")return {};
    FILE *file=fopen(path.c_str(),"rb");
    unsigned char block[2048]{};
    bool valid=read_iso_sector(file,path,ftp_port,16,block) && block[0]==1 && std::memcmp(block+1,"CD001",5)==0;
    if(!valid){
        if(file)fclose(file);
        return {};
    }
    const unsigned char *root=block+156;
    if(root[0]<34){
        if(file)fclose(file);
        return {};
    }
    const auto dir_sector=le32(root+2),dir_size=le32(root+10);
    if(dir_size==0 || dir_size>128u*1024u){
        if(file)fclose(file);
        return {};
    }
    const auto num_sectors=(dir_size+2047u)/2048u;
    for(std::uint32_t n=0;n<num_sectors;++n){
        if(dir_sector>UINT32_MAX-n||!read_iso_sector(file,path,ftp_port,dir_sector+n,block))break;
        for(std::size_t at=0;at+34<=sizeof(block);){
            const std::size_t record=block[at];
            if(!record)break;
            if(record<34 || at+record>sizeof(block))break;
            const auto *entry=block+at;
            const std::size_t name_size=entry[32];
            if(33+name_size<=record && name_size>=10 &&
               std::memcmp(entry+33,"SYSTEM.CNF",10)==0){
                const std::uint32_t data_sector=le32(entry+2),size=le32(entry+10);
                if(size>0 && size<=2048 && read_iso_sector(file,path,ftp_port,data_sector,block)){
                    if(file)fclose(file);
                    return serial_from_cnf(std::string(reinterpret_cast<const char*>(block),size));
                }
            }
            at+=record;
        }
    }
    if(file)fclose(file);
    return {};
}
std::uint16_t le16(const unsigned char *p){return std::uint16_t(p[0]) | std::uint16_t(p[1]<<8);}
// PS5X360 tools/console.py: read XDVDFS volume descriptors and default.xex execution header.
// No writes to the disc image, and no guessed mappings from folders.
std::string xbox_iso_title(const std::string &path,int ftp_port){
    if(ext(path)!=".iso")return {};
    FILE *file=fopen(path.c_str(),"rb");
    for(const std::uint64_t base:{0ull,0x18300000ull,0xFD90000ull,0x2080000ull}){
        unsigned char volume[2048]{};
        if(!read_at_game(file,path,ftp_port,base+32*2048,volume,sizeof(volume)) ||
           std::memcmp(volume,"MICROSOFT*XBOX*MEDIA",20)!=0)continue;
        const auto root=le32(volume+20),size=le32(volume+24);
        if(size<14 || size>1024*1024)continue;
        std::vector<unsigned char> table(size);
        if(!read_at_game(file,path,ftp_port,base+std::uint64_t(root)*2048,table.data(),table.size()))continue;
        std::vector<std::size_t> pending{0};
        std::size_t budget=0;
        while(!pending.empty() && budget++<8192){
            const auto at=pending.back();pending.pop_back();
            if(at>table.size() || table.size()-at<14)continue;
            const unsigned char *p=table.data()+at;
            const auto left=le16(p),right=le16(p+2);
            if(left==0xFFFF)continue;
            const unsigned name_length=p[13];
            if(name_length>0 && name_length<=255 && at+14+name_length<=table.size()){
                std::string name(reinterpret_cast<const char*>(p+14),name_length);
                for(char &ch:name)ch=char(std::tolower(static_cast<unsigned char>(ch)));
                if(name=="default.xex"){
                    std::vector<unsigned char> header(512*1024);
                    const std::uint64_t file_at=base+std::uint64_t(le32(p+4))*2048;
                    if(read_at_game(file,path,ftp_port,file_at,header.data(),header.size())){
                        if(file)fclose(file);
                        return xex_title_id(std::string(reinterpret_cast<const char*>(header.data()),header.size()));
                    }
                }
            }
            for(unsigned offset:{left,right})if(offset && offset!=0xFFFF && static_cast<std::size_t>(offset)*4<table.size())
                pending.push_back(static_cast<std::size_t>(offset)*4);
        }
    }
    if(file)fclose(file);
    return {};
}
std::string read_maybe_ftp(const std::string &path,int port,std::size_t cap);
// PS5SX2 caches CHD serials as <fullpath> TAB <size> TAB <mtime> TAB <serial>.
// The BGH sandbox may not stat a USB game, so exact path + unique valid serial
// is a safe fallback when the local file metadata cannot be read.
std::string chd_cached_serial(const std::string &cache,const std::string &path,int port){
    std::string text=read_maybe_ftp(cache,port,2u*1024u*1024u);
    if(text.empty())return {};
    struct stat st{};
    const bool measured=(lstat(path.c_str(),&st)==0 && S_ISREG(st.st_mode));
    const std::string prefix=path+"\t";
    const std::string exact=measured ? prefix+std::to_string(static_cast<long long>(st.st_size))+"\t"+
        std::to_string(static_cast<long long>(st.st_mtime))+"\t" : std::string{};
    std::string unique,strict;
    bool conflicting=false;
    for(std::size_t begin=0;begin<text.size();){
        const auto end=text.find('\n',begin);
        const auto line=text.substr(begin,end==std::string::npos?std::string::npos:end-begin);
        if(line.compare(0,prefix.size(),prefix)==0){
            const auto last_tab=line.rfind('\t');
            if(last_tab!=std::string::npos && last_tab>prefix.size()){
                const std::string serial=serial_ps2(line.substr(last_tab+1));
                if(!serial.empty()){
                    if(!unique.empty() && unique!=serial)conflicting=true;
                    unique=serial;
                    if(measured && line.compare(0,exact.size(),exact)==0)strict=serial;
                }
            }
        }
        if(end==std::string::npos)break;
        begin=end+1;
    }
    if(!strict.empty())return strict;
    return conflicting ? std::string{} : unique;
}
// ASCII word comparison against the real names used by Stefan's Xbox-360 ROM folders.
// No external lookup, no renaming, and no guessed Xbox title IDs for Viva Pinata.
std::string normalized_words(const std::string &input){
    std::string result;
    bool spacer=false;
    for(unsigned char c:input){
        if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')){
            if(spacer && !result.empty())result.push_back(' ');
            result.push_back(static_cast<char>(std::tolower(c)));
            spacer=false;
        }else spacer=true;
    }
    return result;
}
bool has_words(const std::string &haystack,const std::string &needle){
    const auto at=haystack.find(needle);
    if(at==std::string::npos)return false;
    const auto before=(at==0||haystack[at-1]==' ');
    const auto after=(at+needle.size()==haystack.size()||haystack[at+needle.size()]==' ');
    return before&&after;
}
// Exact JPG filenames already present under /mnt/usb1/xbox360/Cover/.
// Two IDs are confirmed from the user's BGH/PS5X360 screenshots; Viva uses
// only its explicit game name, never an invented title-ID mapping.
std::string user_xbox_cover_filename(const Game &game,const std::string &id){
    const std::string name=normalized_words(game.title+" "+game.path+" "+stem(basename(game.path)));
    if(id=="4D53082D" || (id.empty() && has_words(name,"gears of war 2")))
        return "Gears of War 2.jpg";
    if(id=="575207EF" || (id.empty() && has_words(name,"lord of the rings") &&
        has_words(name,"war in the north")))
        return "Lord of the Rings War in the North.jpg";
    if(has_words(name,"viva pinata") && has_words(name,"party animals"))
        return "Viva Party Animals.jpg";
    return {};
}
bool valid_xbox_title(const std::string &id){
    if(id.size()!=8)return false;
    return std::all_of(id.begin(),id.end(),[](unsigned char c){return std::isxdigit(c)!=0;});
}
std::string read_maybe_ftp(const std::string &path,int port,std::size_t cap){
    std::string bytes;
    if(read_native(path,bytes,cap))return bytes;
    FtpReadonly ftp(port);
    if(ftp.retrieve(path,bytes,cap))return bytes;
    return {};
}
std::string parent_dir(const std::string &path){
    const auto slash=path.rfind('/');
    return slash==std::string::npos ? std::string{} : (slash==0 ? "/" : path.substr(0,slash));
}
// Additional PS5SX2 lookup locations are bound to the selected game's drive,
// never scanned indiscriminately across unrelated libraries.
std::string usb_drive_root(const std::string &path){
    constexpr const char *prefix="/mnt/usb";
    if(path.compare(0,8,prefix)!=0)return {};
    std::size_t i=8;
    while(i<path.size() && path[i]>='0' && path[i]<='9')++i;
    return i>8 && i<path.size() && path[i]=='/' ? path.substr(0,i) : std::string{};
}
std::string ps2_opl_id(const std::string &serial){
    if(serial_ps2(serial).empty())return {};
    return serial.substr(0,4)+"_"+serial.substr(5,3)+"."+serial.substr(8,2);
}
// Validate BGH's private cached files before trusting them. The PS5 path
// uses the SAME bounded decoder as the OpenGL texture loader. Standalone host
// importer tests use the conservative container checks below because stb_image
// is supplied by the pinned PS5 build kit (not an uploaded host binary).
bool payload_valid(const std::string &path, const std::string &bytes,const std::string &extension,std::string *reason=nullptr){
    if(!cover_signature(bytes,extension))return false;
#ifdef BGH_TARGET_PS5
    PngImage image;
    return decode_cover_image(path,image,reason) && image.width>0 && image.height>0 && !image.rgba.empty();
#else
    (void)path;
    (void)reason;
    const auto *p=reinterpret_cast<const unsigned char *>(bytes.data());
    if(extension==".jpg"||extension==".jpeg")return bytes.size()>=32 && p[bytes.size()-2]==0xff && p[bytes.size()-1]==0xd9;
    if(extension==".png")return bytes.size()>=32 && bytes.compare(bytes.size()-8,8,"IEND\xae\x42\x60\x82",8)==0; // verified by pinned PNG decoder on PS5
    if(extension==".tga"){
        const std::size_t start=18u+p[0]+static_cast<std::size_t>(p[1])*18u;
        const auto w=static_cast<std::size_t>(p[12])+256u*p[13];
        const auto h=static_cast<std::size_t>(p[14])+256u*p[15];
        const auto pixels=w*h;
        const auto pixel_size=static_cast<std::size_t>(p[16])/8u;
        return start<bytes.size() && (p[2]==10 || pixels<=((bytes.size()-start)/pixel_size));
    }
    return false;
#endif
}
std::string cached(const std::string &dir,const Game &game,const std::string &extension){
    if(!safe_component(game.id))return {};
    const std::string path=dir+"/"+game.id+extension;
    std::string bytes;
    if(read_private_cache(path,bytes) && payload_valid(path,bytes,extension))return path;
    return {};
}
std::string store(const std::string &dir,const Game &game,const std::string &origin,int port,std::string &failure){
    const std::string suffix=ext(origin);
    if(suffix!=".png"&&suffix!=".jpg"&&suffix!=".jpeg"&&suffix!=".tga"){
        failure="IMPORT: NICHT UNTERSTUETZTES BILDFORMAT";return {};
    }
    const std::string target=dir+"/"+game.id+suffix;
    if(const auto found=cached(dir,game,suffix);!found.empty())return found;
    // Never pre-block on lstat/opendir of a virtual PS5 cache directory.
    // A direct private-cache fopen/write/rename yields the real usability error.
    std::string bytes;
    if(!read_native(origin,bytes,kMaxImage)){
        FtpReadonly ftp(port);
        const auto result=ftp.retrieve_detailed(origin,bytes,kMaxImage);
        if(!result.ok){failure="FTP-ABRUF: "+result.message+" (Antwort "+std::to_string(result.reply)+
            ", Bytes "+std::to_string(result.received)+")";return {};}
    }
    if(!cover_signature(bytes,suffix)){
        failure="BILDSIGNATUR UNGUELTIG ODER UNVOLLSTAENDIG: "+basename(origin);return {};
    }
    // Preserve the file extension for the decoder (foo.tmp.jpg, not foo.jpg.tmp).
    const std::string tmp=dir+"/"+game.id+".tmp"+suffix;
    FILE *file=fopen(tmp.c_str(),"wb");
    if(!file){
        const int error=errno;
        failure="BGH-CACHE-ORDNER NICHT VERFUEGBAR / CACHE-DATEI OEFFNEN: "+dir+
            " (errno "+std::to_string(error)+": "+std::strerror(error)+")";
        return {};
    }
    const bool wrote=fwrite(bytes.data(),1,bytes.size(),file)==bytes.size();
    const int write_error=wrote?0:errno;
    const bool closed=fclose(file)==0;
    const int close_error=closed?0:errno;
    if(!wrote||!closed){
        const int error=wrote?close_error:write_error;
        failure="CACHE SCHREIBEN/SCHLIESSEN (errno "+std::to_string(error)+"): "+std::strerror(error);
        std::remove(tmp.c_str());return {};
    }
    std::string decode_error;
    if(!payload_valid(tmp,bytes,suffix,&decode_error)){
        failure="BILD-DEKODIERUNG: "+basename(origin)+" "+decode_error;
        std::remove(tmp.c_str());return {};
    }
    if(std::rename(tmp.c_str(),target.c_str())!=0){
        failure="CACHE-UMBENENNEN: "+std::string(std::strerror(errno));
        std::remove(tmp.c_str());return {};
    }
    return target;
}
void candidate(std::vector<std::string> &out,const std::string &dir,const std::string &name){
    if(safe_component(name))out.push_back(dir+"/"+name);
}
void candidates_for_key(std::vector<std::string> &out,const std::string &dir,const std::string &key) {
    for(const char *ending:{".jpg",".png",".jpeg"})candidate(out,dir,key+ending);
}
} // namespace
std::uint32_t eden_cover_hash(const std::string &filename){
    std::uint32_t hash=2166136261u;
    for(unsigned char ch:filename)hash=(hash^ch)*16777619u;
    return hash;
}
std::string eden_cover_filename(const std::string &rom_path){
    const auto name=basename(rom_path);
    return hex32(eden_cover_hash(name))+".tga";
}
std::string xex_title_id(const std::string &header){
    if(header.size()<24 || header.compare(0,4,"XEX2")!=0)return {};
    const auto *b=reinterpret_cast<const unsigned char*>(header.data());
    const std::uint32_t n=be32(b+0x14);
    for(std::uint32_t i=0;i<n && i<64 && 0x18ull+i*8+8<=header.size();++i){
        const std::size_t at=0x18ull+i*8;
        if(be32(b+at)!=0x00040006u)continue;
        const std::uint32_t start=be32(b+at+4);
        if(start>header.size() || header.size()-start<16)return {};
        const auto id=be32(b+start+12);
        return id?hex32(id):std::string{};
    }
    return {};
}
bool cover_signature(const std::string &bytes,const std::string &extension){
    if(bytes.size()<24 || bytes.size()>kMaxImage)return false;
    const auto *p=reinterpret_cast<const unsigned char *>(bytes.data());
    if(extension==".png")return bytes.size()>=24 && std::memcmp(p,"\x89PNG\r\n\x1a\n",8)==0;
    if(extension==".jpg"||extension==".jpeg")return p[0]==0xff && p[1]==0xd8 && p[2]==0xff;
    if(extension==".tga") {
        // ProsperoEden TGA output: uncompressed (2) or RLE (10) true-colour TGA.
        const unsigned type=p[2],depth=p[16];
        const unsigned width=p[12]+256u*p[13],height=p[14]+256u*p[15];
        return p[1]==0 && (type==2 || type==10) && (depth==24||depth==32) && width>0 && height>0 && width<=4096 && height<=4096;
    }
    return false;
}
EmulatorCoverImporter::EmulatorCoverImporter(std::string cache_dir,int ftp_port,EmulatorCoverSources sources)
    :cache_dir_(std::move(cache_dir)),ftp_port_(ftp_port),sources_(std::move(sources)){}
bool EmulatorCoverImporter::hydrate(Game &game,std::string *diagnostic){
    auto report=[&](const std::string &msg){if(diagnostic)*diagnostic=msg;};
    if(game.platform!="PS2" && game.platform!="Switch" && game.platform!="Xbox 360"){report("NICHT EMULATOR");return false;}
    if(!game.cover_path.empty()){report("BEREITS ZUGEORDNET");return false;} // never overwrite existing covers.
    if(!safe_component(game.id)){report("UNGUELTIGE SPIEL-ID");return false;}
    for(const char *type:{".jpg",".png",".jpeg",".tga"}){
        if(const auto hit=cached(cache_dir_,game,type);!hit.empty()){game.cover_path=hit;report("BGH-CACHE: COVER GEFUNDEN");return true;}
    }
    std::vector<std::string> origins;
    if(game.platform=="Switch"){
        candidate(origins,sources_.switch_covers,eden_cover_filename(game.path));
        report("SWITCH: HASH GEFUNDEN " + eden_cover_filename(game.path));
    }else if(game.platform=="PS2"){
        std::string serial=serial_ps2(game.title_id);
        if(serial.empty())serial=iso_serial(game.path,ftp_port_);
        if(serial.empty() && ext(game.path)==".chd")serial=chd_cached_serial(sources_.ps2_serial_cache,game.path,ftp_port_);
        const std::string filename=stem(basename(game.path));
        // PS5SX2: manual covers by serial or exact image filename; cache by serial.
        if(!serial.empty()){
            candidates_for_key(origins,sources_.ps2_manual,serial);
            report("PS2: SERIENNUMMER "+serial);
            candidate(origins,sources_.ps2_cache,serial+".jpg");
        }
        candidates_for_key(origins,sources_.ps2_manual,filename);
        // Same-disc cover and USB-wide cover/ART paths follow PS5SX2's
        // search convention. Do not modify those emulator or USB files.
        const auto beside=parent_dir(game.path);
        if(!beside.empty())candidates_for_key(origins,beside,filename);
        const auto drive=usb_drive_root(game.path);
        if(!drive.empty()){
            if(!serial.empty())candidates_for_key(origins,drive+"/covers",serial);
            candidates_for_key(origins,drive+"/covers",filename);
            if(!serial.empty())candidates_for_key(origins,drive+"/ART",ps2_opl_id(serial)+"_COV");
        }
        if(serial.empty())report("PS2: SERIENNUMMER FEHLT; ISO/CHD-LESEZUGRIFF PRUEFEN");
    }else if(game.platform=="Xbox 360"){
        // PS5X360 CacheKey uses this exact 64-bit seed (not standard FNV's
        // longer offset basis), over the complete, case-sensitive ROM path.
        std::uint64_t library_hash=1469598103934665603ull;
        for(unsigned char c:game.path){library_hash^=c;library_hash*=1099511628211ull;}
        char library_name[24];
        std::snprintf(library_name,sizeof(library_name),"%016llx.png",
            static_cast<unsigned long long>(library_hash));
        candidate(origins,"/download0/xbox360ps5/library",library_name);
        // PS5X360 launcher.cpp: ISO-basename images and XEX folder cover.
        const auto beside=parent_dir(game.path);
        if(!beside.empty()){
            candidates_for_key(origins,beside,stem(basename(game.path)));
            candidates_for_key(origins,beside,"cover");
        }
        std::string id=game.title_id;
        if(!valid_xbox_title(id)){
            if(ext(game.path)==".iso")id=xbox_iso_title(game.path,ftp_port_);
            if(ext(game.path)==".xex"){
                std::string prefix;
                // XEX2 files are usually much larger than the 4 MiB FTP limit.
                // Read local prefix only; never guess a title ID from folder names.
                if(read_prefix_native(game.path,prefix,512*1024))id=xex_title_id(prefix);
                else {
                    FtpReadonly ftp(ftp_port_);
                    if(ftp.retrieve_range(game.path,0,prefix,512*1024))id=xex_title_id(prefix);
                }
            }
        }
        if(valid_xbox_title(id))
            std::transform(id.begin(),id.end(),id.begin(),[](unsigned char c){return char(std::toupper(c));});
        else id.clear();
        // Prefer the user's explicitly named JPGs, but retain all existing
        // PS5X360 cache and same-disc cover routes as fallback.
        const std::string named_art=user_xbox_cover_filename(game,id);
        if(!named_art.empty())candidate(origins,sources_.xbox_user_covers,named_art);
        if(valid_xbox_title(id)){
            report("XBOX 360: TITEL-ID "+id);
            candidates_for_key(origins,sources_.xbox_user_covers,id);
            candidates_for_key(origins,sources_.xbox_covers,id);
            candidates_for_key(origins,sources_.xbox_installed_covers,id);
            // PS5X360 tools/console.py game_logs(): discover actual mounted
            // sandbox names, never guess mount suffixes or mount download0.dat.
            FtpReadonly ftp(ftp_port_);
            const auto sandboxes=ftp.list(sources_.xbox_sandbox_root,128);
            unsigned added=0;
            for(const auto &entry:sandboxes.entries){
                if(!entry.folder || entry.name.compare(0,10,"PPSA50011_")!=0 ||
                   entry.name.size()<=10 || entry.name.size()>80)continue;
                if(!std::all_of(entry.name.begin()+10,entry.name.end(),
                    [](unsigned char c){return std::isalnum(c)!=0;}))continue;
                if(added++>=8)break;
                candidates_for_key(origins,sources_.xbox_sandbox_root+"/"+entry.name+
                    "/download0/xbox360ps5/covers",id);
                candidate(origins,sources_.xbox_sandbox_root+"/"+entry.name+
                    "/download0/xbox360ps5/library",library_name);
            }
        }
    }
    if(game.platform=="Xbox 360"){
        // The user explicitly provided these images. Prefer them over any
        // older per-ISO or emulator-cache art without changing other platforms.
        const std::string usb_prefix=sources_.xbox_user_covers+"/";
        std::stable_partition(origins.begin(),origins.end(),[&](const std::string &path){
            return path.compare(0,usb_prefix.size(),usb_prefix)==0;
        });
    }
    if(game.platform=="Xbox 360" && origins.empty())report("XBOX 360: TITEL-ID FEHLT");
    if(game.platform=="PS2" || game.platform=="Xbox 360"){
        // Some older FTP servers have no MLSD. Try the most common .jpg
        // names at all verified PS5SX2 locations before less common types.
        std::stable_partition(origins.begin(),origins.end(),[](const std::string &path){
            return ext(path)==".jpg";
        });
    }
    const std::string lookup_status=diagnostic ? *diagnostic : std::string{};
    bool candidate_exists=false;
    std::string first_failure;
    bool folder_readable=false;
    // MLSD is only a read-only candidate filter: one listing per directory,
    // rather than dozens of RETR requests for nonexistent suffixes.
    // An older FTP service without MLSD falls back to a bounded number of
    // exact filename attempts, prioritizing confirmed serial/hash candidates.
    struct DirectoryState {bool listed=false;std::set<std::string> files;};
    std::map<std::string,DirectoryState> directories;
    unsigned uncertain_ftp_attempts=0;
    constexpr unsigned kMaxUnlistedRetr=8;
    for(const auto &path:origins){
        struct stat st{};
        const bool direct_file=(lstat(path.c_str(),&st)==0 && S_ISREG(st.st_mode));
        if(direct_file)candidate_exists=true;
        if(!direct_file){
            const auto directory=parent_dir(path);
            const auto it=directories.find(directory);
            if(it==directories.end()){
                DirectoryState info;
                FtpReadonly ftp(ftp_port_);
                const auto listing=ftp.list(directory,2000);
                info.listed=listing.ok;
                if(listing.ok){
                    folder_readable=true;
                    for(const auto &entry:listing.entries)if(!entry.folder)info.files.insert(entry.name);
                }
                directories.emplace(directory,std::move(info));
            }
            const auto &info=directories.at(directory);
            if(info.listed){
                if(!info.files.count(basename(path)))continue;
                candidate_exists=true;
            }else if(++uncertain_ftp_attempts>kMaxUnlistedRetr)continue;
        }
        std::string failure;
        const std::string stored=store(cache_dir_,game,path,ftp_port_,failure);
        if(!stored.empty()){
            game.cover_path=stored;
            report("COVER IMPORTIERT: "+basename(path));
            return true;
        }
        if(!failure.empty() && first_failure.empty())first_failure=failure;
    }
    if(candidate_exists && !first_failure.empty())report(first_failure);
    else if(candidate_exists)report("COVER-KANDIDAT GEFUNDEN; IMPORT FEHLGESCHLAGEN");
    else if(game.platform=="PS2" && lookup_status.find("SERIENNUMMER FEHLT")!=std::string::npos)
        report(lookup_status);
    else if(origins.empty())report("SPIELKENNUNG FEHLT");
    else if(folder_readable)report(lookup_status+"; KEIN PASSENDES COVER");
    else report(lookup_status+"; COVERORDNER NICHT LESBAR ODER COVER FEHLT");
    return false;
}
} // namespace ulg

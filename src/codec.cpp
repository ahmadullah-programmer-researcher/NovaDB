#include "codec.hpp"
#include <array>
#include <limits>

namespace novadb::codec {
void write_u32(std::ostream& o, std::uint32_t v) { for (int i=0;i<4;++i) o.put(static_cast<char>((v>>(i*8))&0xff)); }
void write_u64(std::ostream& o, std::uint64_t v) { for (int i=0;i<8;++i) o.put(static_cast<char>((v>>(i*8))&0xff)); }
bool read_u32(std::istream& in, std::uint32_t& v) { v=0; for(int i=0;i<4;++i){ int c=in.get(); if(c==EOF)return false; v|=static_cast<std::uint32_t>(static_cast<unsigned char>(c))<<(i*8);} return true; }
bool read_u64(std::istream& in, std::uint64_t& v) { v=0; for(int i=0;i<8;++i){ int c=in.get(); if(c==EOF)return false; v|=static_cast<std::uint64_t>(static_cast<unsigned char>(c))<<(i*8);} return true; }
void write_string(std::ostream& out,const std::string& s){ write_u32(out,static_cast<std::uint32_t>(s.size())); out.write(s.data(),static_cast<std::streamsize>(s.size())); }
bool read_string(std::istream& in,std::string& s,std::uint32_t max){ std::uint32_t n; if(!read_u32(in,n)||n>max)return false; s.assign(n,'\0'); in.read(s.data(),static_cast<std::streamsize>(n)); return static_cast<bool>(in); }
std::uint32_t checksum(const std::string& s){ std::uint32_t h=2166136261u; for(unsigned char c:s){h^=c;h*=16777619u;} return h; }
}

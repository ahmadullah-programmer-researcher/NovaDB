#include "novadb/db.hpp"
#include "codec.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

namespace novadb {
namespace {
constexpr char kWalMagic[] = "NWL1";
constexpr char kTableMagic[] = "NST1";
constexpr std::uint32_t kMaxRecord = 64u * 1024u * 1024u;
std::filesystem::path table_path(const std::filesystem::path& p, std::uint64_t n) { return p / ("table-" + std::to_string(n) + ".sst"); }
std::uint64_t parse_generation(const std::string& name) {
    const std::string prefix="table-", suffix=".sst";
    if(name.size()<=prefix.size()+suffix.size() || name.compare(0,prefix.size(),prefix)!=0 || name.compare(name.size()-suffix.size(),suffix.size(),suffix)!=0) return 0;
    try { return std::stoull(name.substr(prefix.size(),name.size()-prefix.size()-suffix.size())); } catch(...) { return 0; }
}
}

WriteBatch& WriteBatch::put(std::string k,std::string v){ operations_.push_back({WriteOperation::Type::Put,std::move(k),std::move(v)}); return *this; }
WriteBatch& WriteBatch::erase(std::string k){ operations_.push_back({WriteOperation::Type::Delete,std::move(k),{}}); return *this; }
DB DB::open(const std::filesystem::path& p, Options o){ if(!std::filesystem::exists(p)){ if(!o.create_if_missing) throw std::runtime_error("database directory does not exist"); std::filesystem::create_directories(p); } if(!std::filesystem::is_directory(p)) throw std::runtime_error("database path is not a directory"); return DB(p,o); }
DB::DB(std::filesystem::path p,Options o):path_(std::move(p)),options_(o){ recover(); }
DB::~DB(){ try { std::lock_guard<std::mutex> lock(mutex_); if(!closed_) flush_locked(); closed_=true; } catch(...) {} }
void DB::apply(const WriteOperation& op,std::uint64_t seq){ if(op.type==WriteOperation::Type::Put) memtable_[op.key]={seq,op.value}; else memtable_[op.key]={seq,std::nullopt}; approximate_bytes_ += op.key.size()+op.value.size()+sizeof(seq)+16; sequence_=std::max(sequence_,seq); }
void DB::append_wal(const WriteOperation& op,std::uint64_t seq){
    std::string payload; payload.push_back(static_cast<char>(op.type));
    for(int i=0;i<8;++i) payload.push_back(static_cast<char>((seq>>(i*8))&0xff));
    auto append32=[&](std::uint32_t n){for(int i=0;i<4;++i)payload.push_back(static_cast<char>((n>>(i*8))&0xff));};
    append32(static_cast<std::uint32_t>(op.key.size())); payload+=op.key; append32(static_cast<std::uint32_t>(op.value.size())); payload+=op.value;
    if(payload.size()>kMaxRecord) throw std::runtime_error("WAL record exceeds maximum size");
    std::ofstream out(path_/"active.wal",std::ios::binary|std::ios::app); if(!out) throw std::runtime_error("cannot open WAL");
    out.write(kWalMagic,4); codec::write_u32(out,static_cast<std::uint32_t>(payload.size())); out.write(payload.data(),static_cast<std::streamsize>(payload.size())); codec::write_u32(out,codec::checksum(payload)); out.flush(); if(!out) throw std::runtime_error("failed to append WAL record");
}
void DB::put(const std::string& k,const std::string& v){ WriteBatch b; b.put(k,v); write(b); }
bool DB::erase(const std::string& k){ const bool existed=get(k).has_value(); WriteBatch b; b.erase(k); write(b); return existed; }
void DB::write(const WriteBatch& b){ if(b.empty())return; std::lock_guard<std::mutex> lock(mutex_); if(closed_)throw std::runtime_error("database is closed"); for(const auto& op:b.operations()){ if(op.key.size()>kMaxRecord||op.value.size()>kMaxRecord)throw std::runtime_error("key or value too large"); const auto next=sequence_+1; append_wal(op,next); apply(op,next); } if(approximate_bytes_>=options_.write_buffer_size)flush_locked(); }
std::optional<std::string> DB::get(const std::string& k) const { std::lock_guard<std::mutex> lock(mutex_); auto it=memtable_.find(k); if(it==memtable_.end()||!it->second.second)return std::nullopt; return it->second.second; }
std::vector<std::pair<std::string,std::string>> DB::scan() const { std::lock_guard<std::mutex> lock(mutex_); std::vector<std::pair<std::string,std::string>> out; for(const auto& e:memtable_)if(e.second.second)out.emplace_back(e.first,*e.second.second); return out; }
std::size_t DB::size() const { return scan().size(); }
std::uint64_t DB::last_sequence() const { std::lock_guard<std::mutex> lock(mutex_); return sequence_; }
void DB::flush(){ std::lock_guard<std::mutex> lock(mutex_); if(closed_)throw std::runtime_error("database is closed"); flush_locked(); }
void DB::flush_locked(){
    if(memtable_.empty()) { std::ofstream clear(path_/"active.wal",std::ios::binary|std::ios::trunc); return; }
    const auto next_generation=generation_+1; const auto final=table_path(path_,next_generation); const auto temp=final.string()+".tmp";
    std::ofstream out(temp,std::ios::binary|std::ios::trunc); if(!out)throw std::runtime_error("cannot create SSTable"); out.write(kTableMagic,4); codec::write_u64(out,sequence_); codec::write_u64(out,static_cast<std::uint64_t>(memtable_.size()));
    for(const auto& e:memtable_){ codec::write_string(out,e.first); codec::write_u64(out,e.second.first); out.put(e.second.second?1:0); if(e.second.second)codec::write_string(out,*e.second.second); }
    out.flush(); if(!out)throw std::runtime_error("failed writing SSTable"); out.close();
    std::error_code ec; std::filesystem::rename(temp,final,ec); if(ec){std::filesystem::remove(temp);throw std::runtime_error("cannot publish SSTable: "+ec.message());}
    // The new complete snapshot is published before old snapshots and WAL are removed.
    for(const auto& entry:std::filesystem::directory_iterator(path_)){ auto n=parse_generation(entry.path().filename().string()); if(n&&n!=next_generation)std::filesystem::remove(entry.path(),ec); }
    generation_=next_generation; memtable_.clear(); approximate_bytes_=0;
    std::ofstream wal(path_/"active.wal",std::ios::binary|std::ios::trunc); wal.flush(); if(!wal)throw std::runtime_error("failed truncating WAL after flush");
}
void DB::recover(){
    std::uint64_t best=0; for(const auto& e:std::filesystem::directory_iterator(path_)){auto n=parse_generation(e.path().filename().string());if(n>best)best=n;}
    generation_=best;
    if(best){ std::ifstream in(table_path(path_,best),std::ios::binary); char magic[4]; in.read(magic,4); if(!in||std::string(magic,4)!=kTableMagic)throw std::runtime_error("invalid SSTable header"); std::uint64_t seq,count; if(!codec::read_u64(in,seq)||!codec::read_u64(in,count)||count>100000000)throw std::runtime_error("invalid SSTable metadata"); sequence_=seq; for(std::uint64_t i=0;i<count;++i){std::string k,v;std::uint64_t s; if(!codec::read_string(in,k)||!codec::read_u64(in,s))throw std::runtime_error("corrupt SSTable entry"); int live=in.get(); if(live==EOF)throw std::runtime_error("corrupt SSTable entry"); std::optional<std::string> value; if(live){if(!codec::read_string(in,v))throw std::runtime_error("corrupt SSTable value");value=std::move(v);} memtable_[std::move(k)]={s,std::move(value)};}}
    const auto walpath=path_/"active.wal"; if(!std::filesystem::exists(walpath))return; std::ifstream in(walpath,std::ios::binary); if(!in)throw std::runtime_error("cannot read WAL");
    while(in.peek()!=EOF){char magic[4];in.read(magic,4);if(!in)break;if(std::string(magic,4)!=kWalMagic)break;std::uint32_t len;if(!codec::read_u32(in,len)||len>kMaxRecord)break;std::string payload(len,'\0');in.read(payload.data(),len);std::uint32_t crc;if(!in||!codec::read_u32(in,crc)||crc!=codec::checksum(payload))break;
        std::size_t p=0; auto take32=[&](std::uint32_t& n){if(p+4>payload.size())return false;n=0;for(int i=0;i<4;++i)n|=static_cast<std::uint32_t>(static_cast<unsigned char>(payload[p++]))<<(i*8);return true;};
        if (payload.size() < 9) break;
        auto type = static_cast<WriteOperation::Type>(static_cast<unsigned char>(payload[p++]));
        std::uint64_t seq = 0;
        for (int i = 0; i < 8; ++i)
            seq |= static_cast<std::uint64_t>(static_cast<unsigned char>(payload[p++])) << (i * 8);
        std::uint32_t kl, vl;
        if (!take32(kl) || kl > payload.size() - p) break;
        std::string key = payload.substr(p, kl);
        p += kl;
        if (!take32(vl) || vl > payload.size() - p || p + vl != payload.size()) break;
        std::string value = payload.substr(p, vl);
        if (seq > sequence_ && (type == WriteOperation::Type::Put || type == WriteOperation::Type::Delete))
            apply({type, std::move(key), std::move(value)}, seq);
    }
}
} // namespace novadb

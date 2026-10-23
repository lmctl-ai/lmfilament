#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace lmfilament {
// Deterministic RFC 1951 fixed-Huffman LZ77 encoder, 32 KiB window.
inline std::vector<unsigned char> deflate_zlib(const std::vector<unsigned char>& raw) {
  std::vector<unsigned char> out{0x78,0x01};
  unsigned bits=0,count=0;
  auto emit=[&](unsigned value,unsigned n){bits|=value<<count;count+=n;while(count>=8){out.push_back(bits&255);bits>>=8;count-=8;}};
  auto reverse=[](unsigned value,unsigned n){unsigned r=0;while(n--){r=(r<<1)|(value&1);value>>=1;}return r;};
  auto literal=[&](unsigned symbol){unsigned code,n;
    if(symbol<144){code=0x30+symbol;n=8;}
    else if(symbol<256){code=0x190+symbol-144;n=9;}
    else if(symbol<280){code=symbol-256;n=7;}
    else{code=0xc0+symbol-280;n=8;}
    emit(reverse(code,n),n);
  };
  constexpr unsigned lengths[]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
  constexpr unsigned lextra[]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
  constexpr unsigned distances[]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
  constexpr unsigned dextra[]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
  std::array<int,65536> heads;heads.fill(-1);
  std::array<int,32768> previous;previous.fill(-1);
  auto hash=[&](size_t p){return (unsigned(raw[p])*251u*251u+unsigned(raw[p+1])*251u+raw[p+2])&65535u;};
  auto insert=[&](size_t p){if(p+2<raw.size()){auto h=hash(p);previous[p&32767]=heads[h];heads[h]=static_cast<int>(p);}};
  emit(3,3); // BFINAL=1, BTYPE=01 (fixed Huffman).
  for(size_t p=0;p<raw.size();){
    size_t length=0,distance=0;
    if(p+2<raw.size()){
      int candidate=heads[hash(p)];unsigned probes=0;
      while(candidate>=0&&p-size_t(candidate)<=32768&&probes++<64){
        size_t n=0,limit=std::min<size_t>(258,raw.size()-p);
        while(n<limit&&raw[size_t(candidate)+n]==raw[p+n])++n;
        if(n>length){length=n;distance=p-size_t(candidate);if(n==limit)break;}
        int next=previous[size_t(candidate)&32767];if(next>=candidate)break;candidate=next;
      }
    }
    if(length>=3){
      unsigned l=0,d=0;while(l<28&&lengths[l+1]<=length)++l;while(d<29&&distances[d+1]<=distance)++d;
      literal(257+l);emit(unsigned(length)-lengths[l],lextra[l]);
      emit(reverse(d,5),5);emit(unsigned(distance)-distances[d],dextra[d]);
      for(size_t n=0;n<length;++n)insert(p+n);
      p+=length;
    }else{literal(raw[p]);insert(p);++p;}
  }
  literal(256);if(count)out.push_back(bits&255);
  // Incompressible input gets stored blocks instead of expanding with Huffman codes.
  if(out.size()>2+raw.size()+5*std::max<size_t>(1,(raw.size()+65534)/65535)){
    out={0x78,0x01};size_t p=0;
    do{size_t n=std::min<size_t>(65535,raw.size()-p);out.push_back(p+n==raw.size()?1:0);
      out.push_back(n&255);out.push_back(n>>8);unsigned inv=(~unsigned(n))&65535;
      out.push_back(inv&255);out.push_back(inv>>8);out.insert(out.end(),raw.begin()+p,raw.begin()+p+n);p+=n;
    }while(p<raw.size());
  }
  uint32_t a=1,b=0;for(auto v:raw){a=(a+v)%65521;b=(b+a)%65521;}uint32_t sum=(b<<16)|a;
  for(int shift:{24,16,8,0})out.push_back(sum>>shift);
  return out;
}
}

// Test-only transparent IO recorder. No production IO or proof code is changed.
#define main retained_fixture_main
#ifdef PERF6_TRACE_NATIVE
#include "perf6_native_probe.cpp"
#else
#include "perf5_product_fs_probe.cpp"
#endif
#undef main
#include <fstream>
class ExactTrace final:public IO {
 IO&base_;std::ofstream out_;
 void number(std::uint64_t n){char bytes[8];for(unsigned j=0;j<8;++j)bytes[j]=char(n>>(8*j));out_.write(bytes,8);}
 void event(char kind,const std::string&label,const Bytes&value){out_.write(&kind,1);number(label.size());out_.write(label.data(),label.size());number(value.size());out_.write(reinterpret_cast<const char*>(value.data()),value.size());need(bool(out_),"PERF6 exact trace write");}
 public:ExactTrace(IO&base,const std::string&path):base_(base),out_(path,std::ios::binary){need(bool(out_),"PERF6 exact trace open");auto s=base_.session();event('I',"session",Bytes(s.begin(),s.end()));auto p=base_.prefix();event('I',"initial-prefix",Bytes(p.begin(),p.end()));}
 bool prover()const override{return base_.prover();}
 std::string session()const override{return base_.session();}
 Digest prefix()const override{return base_.prefix();}
 Bytes message(const std::string&label,const Bytes&value={})override{auto bytes=base_.message(label,value);event('M',label,bytes);return bytes;}
 Fr coin(const std::string&label)override{auto value=base_.coin(label);event('C',label,r1::encode(value));return value;}
 void finishTrace(){auto p=base_.prefix();event('F',"final-prefix",Bytes(p.begin(),p.end()));out_.flush();need(bool(out_),"PERF6 exact trace flush");}
};
int main(int argc,char**argv){try{
 need(argc>=5,"mode proof trace n [native synthetic seed]");r1::init();auto n=std::stoull(argv[4]);bool p=std::string(argv[1])=="prove";need(p || std::string(argv[1])=="verify","PERF6 exact mode");
#ifdef PERF6_TRACE_NATIVE
 auto bind=nativeBinding(n,false);
#else
 auto bind=binding(n,false);
#endif
 if(p){fixedNonce=true;fs::FsProverIO base(argv[2],bind);ExactTrace io(base,argv[3]);
#ifdef PERF6_TRACE_NATIVE
  need(argc==6,"PERF6 exact native seed");NativeRandom random;executeNative(io,n,&random,argv[5],{});
#else
  FixedRandom random;execute(io,n,&random,{});
#endif
  base.finish();io.finishTrace();std::cout<<base.metadataJson()<<'\n';
 }else{fs::FsVerifierIO base(argv[2],bind);ExactTrace io(base,argv[3]);
#ifdef PERF6_TRACE_NATIVE
  executeNative(io,n,nullptr,"",{});
#else
  execute(io,n,nullptr,{});
#endif
  base.finish();io.finishTrace();std::cout<<base.metadataJson()<<'\n';
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

#define main retained_perf4_lookup_main
#include "oneblock_lookup_tests.cpp"
#undef main
#include <fstream>
struct NativeFixedRandom:Random {
 std::uint64_t counter=0;
 void fill(std::uint8_t*p,std::size_t n)override{while(n){Writer w;w.text("perf4-native-test-only");w.number(counter++,8);auto d=r1::hash(w.b);auto count=std::min(n,d.size());std::copy_n(d.data(),count,p);p+=count;n-=count;}}
};
int main(int argc,char**argv){try{
 r1::init();NativeFixedRandom random;Tape p(true);auto make=factory(random);std::vector<Fr>table{Fr(3),Fr(5),Fr(7),Fr(11)};
 auto address=make(p,"perf4/native/address",5,8,[](std::size_t j){return Fr(j<5?j%4:0);});
 auto value=make(p,"perf4/native/value",5,8,[&](std::size_t j){return table[j<5?j%4:0];});
 hiddenMemory(p,address,value,table,make,&random,"perf4/native/memory");
 Tape v(false);v.messages=p.messages;auto verify=factory(random);
 auto va=verify(v,"perf4/native/address",5,8,{}),vv=verify(v,"perf4/native/value",5,8,{});
 hiddenMemory(v,va,vv,table,verify,nullptr,"perf4/native/memory");need(v.pos==v.messages.size(),"PERF4 native verifier left proof messages");
 Writer bytes;for(const auto&message:p.messages){bytes.text(message.first);bytes.number(message.second.size(),8);bytes.raw(message.second);}
 if(argc==2){std::ofstream out(argv[1],std::ios::binary);need(bool(out),"PERF4 native message artifact open");out.write(reinterpret_cast<const char*>(bytes.b.data()),bytes.b.size());need(bool(out),"PERF4 native message artifact write");}
 std::cout<<"PERF4_NATIVE_MESSAGES {\"sha256\":\""<<hex(r1::hash(bytes.b))<<"\",\"messages\":"<<p.messages.size()<<",\"independent_accept\":true}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

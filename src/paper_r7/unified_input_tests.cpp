// Reuse only the existing small real commitment fixture and tape transport.
#define main classified_fixture_main
#include "classified_crypto_tests.cpp"
#undef main
#include "unified_input.hpp"
#include "interactive.hpp"
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <map>
namespace ui=zkgpt::r7::oneblock::unified;
void largeVirtualDomainTests(){
 // A virtual domain is metadata, not a materialized vector. Keep only three
 // scalar blocks while testing every round through the 29..32-bit domains.
 for(unsigned bits=29;bits<=32;++bits){
  auto n=std::size_t(1)<<bits;std::vector<ui::Segment>segments{{0,1},{(n>>1)+17,1},{n-1,1}};
  std::array<Fr,3>a{Fr(2),Fr(-3),Fr(5)},b{Fr(7),Fr(11),Fr(-13)};
  ui::Factors factors=[&](std::size_t k,const ui::Consume&consume){consume([&,k](std::size_t j){need(j==0,"virtual sparse private index");return a[k];},[&,k](std::size_t j){need(j==0,"virtual sparse kernel index");return b[k];});};
  std::vector<Fr>prefix;
  for(unsigned t=0;t<bits;++t){
   std::array<Fr,3>values{Fr(0),Fr(0),Fr(0)};
   for(unsigned q=0;q<3;++q){std::map<std::size_t,std::pair<Fr,Fr>>suffix;
    for(std::size_t k=0;k<segments.size();++k){auto coordinate=segments[k].offset;Fr weight=1;for(unsigned j=0;j<t;++j)weight*=((coordinate>>j)&1)?prefix[j]:Fr(1)-prefix[j];weight*=((coordinate>>t)&1)?Fr(q):Fr(1)-Fr(q);auto&entry=suffix.try_emplace(coordinate>>(t+1),Fr(0),Fr(0)).first->second;entry.first+=weight*a[k];entry.second+=weight*b[k];}
    for(auto&entry:suffix)values[q]+=entry.second.first*entry.second.second;
   }
   std::array<Fr,3>want;want[0]=values[0];want[2]=(values[2]-Fr(2)*values[1]+values[0])/Fr(2);want[1]=values[1]-want[0]-want[2];auto round=ui::quadraticRound(n,segments,prefix,factors);need(round.coefficients==want,"large virtual domain sparse reference mismatch");prefix.push_back(Fr(-2-int(t)));
  }
  auto endpoint=[&](const std::vector<Fr>&point){Fr result=0;for(std::size_t k=0;k<segments.size();++k){Fr weight=1;for(unsigned j=0;j<bits;++j)weight*=((segments[k].offset>>j)&1)?point[j]:Fr(1)-point[j];result+=weight*b[k];}return result;};
  SessionRandom rng;Tape p(true);auto initial=publicHidden(p,"large-virtual/initial",a[0]*b[0]+a[1]*b[1]+a[2]*b[2]);auto reduced=ui::reduce(p,n,segments,initial,factors,endpoint,&rng,"large-virtual/sumcheck");need(reduced.point.size()==bits,"large virtual domain round count");Tape v(false);v.data=p.data;auto vi=publicHidden(v,"large-virtual/initial",a[0]*b[0]+a[1]*b[1]+a[2]*b[2]);ui::reduce(v,n,segments,vi,{},endpoint,nullptr,"large-virtual/sumcheck");need(v.at==v.data.size(),"large virtual domain independent EOF");
 }
 rejects([]{ui::quadraticRound(std::size_t(1)<<33,{{0,1}},{},[](std::size_t,const ui::Consume&){});},"unsupported virtual domain accepted");
 need(r1::kMaxVector==(std::size_t(1)<<28),"materialized vector limit changed");std::cout<<"UNIFIED_LARGE_VIRTUAL_DOMAIN_TESTED bits=29..32 private_scalars=3 unchanged_materialized_bound="<<r1::kMaxVector<<'\n';
}
void denseRoundTests(){
 for(unsigned variant=0;variant<9;++variant){
  std::vector<ui::Block>blocks{{1,8},{2,2},{3,16},{2,8},{3,1}};
  if(variant==8)blocks.clear();
  auto plan=ui::pack(blocks,variant==8?1:4);
  std::vector<std::vector<Fr>>a(plan.segments.size()),b(a.size());
  std::vector<Fr>denseA(plan.domain,Fr(0)),denseB(plan.domain,Fr(0));
  for(std::size_t k=0;k<a.size();++k){auto seg=plan.segments[k];a[k].resize(seg.padded);b[k].resize(seg.padded);
   for(std::size_t j=0;j<seg.padded;++j){a[k][j]=Fr(int((k+2)*(j+3))-11);b[k][j]=k==0?Fr(0):Fr(int(k+3*j)-7);denseA[seg.offset+j]=a[k][j];denseB[seg.offset+j]=b[k][j];}}
  ui::Factors factors=[&](std::size_t k,const ui::Consume&consume){Value av=[&,k](std::size_t j){return a[k].at(j);};Value bv=[&,k](std::size_t j){return b[k].at(j);};consume(av,bv);};
  std::vector<Fr>prefix;std::size_t current=plan.domain;
  while(current>1){
   // Independent oracle: dense low-bit folding and evaluations at0,1,2.
   std::array<Fr,3>evaluation{Fr(0),Fr(0),Fr(0)};
   for(unsigned t=0;t<3;++t)for(std::size_t j=0;j<current/2;++j){auto x=denseA[2*j]*(Fr(1)-Fr(t))+denseA[2*j+1]*Fr(t);auto y=denseB[2*j]*(Fr(1)-Fr(t))+denseB[2*j+1]*Fr(t);evaluation[t]+=x*y;}
   std::array<Fr,3>want;want[0]=evaluation[0];want[2]=(evaluation[2]-Fr(2)*evaluation[1]+evaluation[0])/Fr(2);want[1]=evaluation[1]-want[0]-want[2];
   auto got=ui::quadraticRound(plan.domain,plan.segments,prefix,factors);
   need(got.coefficients==want,"streaming global polynomial differs from independent dense oracle");
   if(current==2)need(got.input_pair[0]==denseA[0] && got.input_pair[1]==denseA[1] && got.kernel_pair[0]==denseB[0] && got.kernel_pair[1]==denseB[1],"global terminal pairs lost crossroot contributions");
   Fr r=variant%3==0?Fr(0):(variant%3==1?Fr(1):Fr(-3-int(prefix.size())));prefix.push_back(r);
   for(std::size_t j=0;j<current/2;++j){denseA[j]=denseA[2*j]*(Fr(1)-r)+denseA[2*j+1]*r;denseB[j]=denseB[2*j]*(Fr(1)-r)+denseB[2*j+1]*r;}current/=2;
  }
  Fr stitchedA=0,stitchedB=0;
  for(std::size_t k=0;k<a.size();++k){auto d=r1::log2exact(a[k].size());std::vector<Fr>q(prefix.begin(),prefix.begin()+d);auto weight=ui::selector(plan.segments[k],prefix);stitchedA+=weight*r1::mle(a[k],q);stitchedB+=weight*r1::mle(b[k],q);}
  need(stitchedA==denseA[0] && stitchedB==denseB[0],"common point classified stitching differs from full input MLE");
 }
 rejects([]{ui::pack({{0,4}},4);},"private block placed into public partition");
 rejects([]{ui::pack({{4,4}},4);},"unknown category accepted");
 rejects([]{ui::pack({{1,3}},4);},"non power-of-two block accepted");
 rejects([]{ui::validate(16,{{0,8},{4,4}});},"overlapping blocks accepted");
 rejects([]{ui::validate(16,{{2,4}});},"unaligned block accepted");
 rejects([]{ui::validate(16,{{16,1}});},"out-of-domain block accepted");
 std::cout<<"UNIFIED_DENSE_ROUNDS_TESTED\n";
}
void contract(){
 r1::init();auto layout=small();Prepared data;
 data["unit/native-E"]=std::make_shared<std::vector<Fr>>(8,Fr(7));
 SessionRandom rng;Tape tx(true);Commitments pcs(tx,layout,[](std::size_t j){return Fr(j+1);},&data,&rng);
 pcs.constrainBaseTails();std::vector<Fr>point{Fr(3),Fr(5),Fr(7)};
 auto oracle=pcs.view("unit/native-E");auto value=exchangeHidden(tx,"unified-test/value",Fr(7),&rng);
 oracle.open(tx,{point,value.handle},&value,&rng,"unified-test/source");
 pcs.authenticateUnifiedInput({Fr(11),Fr(13),Fr(17)});
 Tape rx(false);rx.data=tx.data;Commitments verifier(rx,layout,{},nullptr,nullptr);verifier.constrainBaseTails();auto source=verifier.view("unit/native-E");auto hidden=exchangeHidden(rx,"unified-test/value",Fr(0),nullptr);source.open(rx,{point,hidden.handle},nullptr,nullptr,"unified-test/source");verifier.authenticateUnifiedInput({Fr(11),Fr(13),Fr(17)});need(verifier.authenticated() && rx.at==rx.data.size(),"unified classified witness-free verification/EOF failed");
 std::size_t unified=0,oldRoots=0;
 for(auto&m:tx.data){unified+=m.first=="UnifiedInput/sumcheck/statement";oldRoots+=m.first.find("Classified/root-proof/")==0;}
 need(unified==1 && oldRoots==0,"one global Unified Input Sumcheck required; old root barrier is not equivalent");
}
void hiddenProtocolTests(){
 for(unsigned mode=0;mode<5;++mode){
  auto plan=ui::pack({{1,4},{2,2},{3,1}},2);std::vector<Fr>a(plan.domain,Fr(0)),b(a.size(),Fr(0));
  for(std::size_t k=0;k<plan.segments.size();++k){auto s=plan.segments[k];for(std::size_t j=0;j<s.padded;++j){a[s.offset+j]=Fr(k+2*j+1);b[s.offset+j]=k?Fr(3*k+j):Fr(0);}}
  Fr sum=0;for(std::size_t j=0;j<a.size();++j)sum+=a[j]*b[j];
  ui::Factors factors=[&](std::size_t k,const ui::Consume&f){auto offset=plan.segments[k].offset;f([&,offset](std::size_t j){return a.at(offset+j);},[&,offset](std::size_t j){return b.at(offset+j);});};
  auto kernel=[&](const std::vector<Fr>&r){return r1::mle(b,r);};SessionRandom rng;Tape p(true);auto h=exchangeHidden(p,"hidden-input-test/initial",sum,&rng);
  ui::reduce(p,plan.domain,plan.segments,h,factors,kernel,&rng,"UnifiedInput/sumcheck");Tape v(false);v.data=p.data;
  if(mode==1){for(auto&m:v.data)if(m.first=="hidden-input-test/initial/commitment"){auto x=decodeHandle(m.second);x.point+=parameters(1).scalar;m.second=encode(x);break;}}
  if(mode==2){for(auto&m:v.data)if(m.first=="UnifiedInput/sumcheck/round/0/c/0/commitment"){auto x=decodeHandle(m.second);x.point+=parameters(1).scalar;m.second=encode(x);break;}}
  if(mode==3){for(auto&m:v.data)if(m.first=="UnifiedInput/sumcheck/input-evaluation/commitment"){auto x=decodeHandle(m.second);x.point+=parameters(1).scalar;m.second=encode(x);break;}}
  auto replay=[&]{auto initial=exchangeHidden(v,"hidden-input-test/initial",Fr(0),nullptr);auto vk=[&](const std::vector<Fr>&r){return kernel(r)+Fr(mode==4?1:0);};ui::reduce(v,plan.domain,plan.segments,initial,{},vk,nullptr,"UnifiedInput/sumcheck");need(v.at==v.data.size(),"hidden input proof has unconsumed messages");};
  if(mode)rejects(replay,"malicious hidden unified input proof accepted");else replay();
 }
 // Zero-dimensional physical blocks are exercised above; also test n=1 driver.
 Tape p(true);SessionRandom rng;auto initial=publicHidden(p,"unit-domain/initial",Fr(21));ui::Factors f=[](std::size_t,const ui::Consume&c){c([](std::size_t){return Fr(3);},[](std::size_t){return Fr(7);});};auto k=[](const std::vector<Fr>&){return Fr(7);};ui::reduce(p,1,{{0,1}},initial,f,k,&rng,"unit-domain");Tape v(false);v.data=p.data;auto vi=publicHidden(v,"unit-domain/initial",Fr(21));ui::reduce(v,1,{{0,1}},vi,{},k,nullptr,"unit-domain");need(v.at==v.data.size(),"unit-domain EOF");
 std::cout<<"UNIFIED_HIDDEN_PROTOCOL_TESTED\n";
}
void mappedProtocolTests(){
 for(unsigned pattern=0;pattern<4;++pattern){
  auto l=small();Prepared data;data["unit/native-E"]=std::make_shared<std::vector<Fr>>();for(unsigned j=0;j<8;++j)data["unit/native-E"]->push_back(Fr(j+2));
  auto map=[pattern](std::size_t j)->Located{if(pattern==3 && j>=5)return {{},Fr(-2),true};auto root=unsigned(j&1);auto index=pattern==2?0:(pattern==1?4-unsigned(j/2):unsigned(j/2));return {{root,index},pattern==3?Fr(3):Fr(0),false};};
  std::vector<Fr>logical;for(unsigned j=0;j<8;++j){auto v=map(j);logical.push_back(v.constant?v.bias:Fr(v.coordinate.index+(v.coordinate.root?2:1))+v.bias);}std::vector<Fr>point{Fr(3),Fr(5),Fr(7)};
  SessionRandom rng;Tape p(true);Commitments pcs(p,l,[](std::size_t j){return Fr(j+1);},&data,&rng);pcs.constrainBaseTails();auto h=exchangeHidden(p,"mapped-protocol/value",r1::mle(logical,point),&rng);std::size_t calls=0;pcs.defer({"mapped-protocol/claim","trusted-test-map",h,point,8,[&](std::size_t j){++calls;return map(j);}});pcs.authenticateUnifiedInput({Fr(11),Fr(13)});need(calls==16,"frozen unified map invoked mutable callback again");
  for(unsigned attack=0;attack<9;++attack){Tape v(false);v.data=p.data;
   if(attack==1)for(auto&m:v.data)if(m.first=="Classified/root/1/PC"){auto c=decodeVector(m.second);c.points[0]+=parameters(1).scalar;m.second=encodeVector(c);break;}
   if(attack==2)for(auto&m:v.data)if(m.first=="mapped-protocol/value/commitment"){auto c=decodeHandle(m.second);c.point+=parameters(1).scalar;m.second=encode(c);break;}
   if(attack==3)for(auto&m:v.data)if(m.first.find("UnifiedInput/opening/root/0/hyrax/")==0 && m.second.size()>=32){m.second.back()^=1;break;}
   if(attack==4)for(auto&m:v.data)if(m.first=="UnifiedInput/all-claims-authenticated"){m.second.back()^=1;break;}
   if(attack==8)for(auto&m:v.data)if(m.first.find("Classified/zero-tail/")==0 && m.first.find("/responses")!=std::string::npos){auto end=Bytes(m.second.end()-32,m.second.end());auto changed=r1::encode(r1::decodeFr(end)+Fr(1));std::copy(changed.begin(),changed.end(),m.second.end()-32);break;}
   auto replay=[&]{Commitments judge(v,l,{},nullptr,nullptr);judge.constrainBaseTails();auto value=exchangeHidden(v,"mapped-protocol/value",Fr(0),nullptr);judge.defer({"mapped-protocol/claim","trusted-test-map",value,point,8,[&](std::size_t j){auto at=map(j);if(attack==5 && !at.constant)at.coordinate.index=(at.coordinate.index+1)%5;if(attack==7)at.bias+=Fr(1);return at;}});judge.authenticateUnifiedInput(attack==6?std::vector<Fr>{Fr(12),Fr(13)}:std::vector<Fr>{Fr(11),Fr(13)});need(judge.authenticated() && v.at==v.data.size(),"mapped unified verifier EOF");};
   if(attack)rejects(replay,"independent verifier accepted malicious mapping/root/claim/opening/X/completion");else replay();
  }
 }
 std::cout<<"UNIFIED_MAPPED_PROTOCOL_TESTED\n";
}
void exhaustedProtocolTest(){
 constexpr std::size_t n=std::size_t(1)<<22;auto l=small();Prepared data;data["unit/native-E"]=std::make_shared<std::vector<Fr>>(8,Fr(7));std::vector<Fr>point(22,Fr(3));
 auto location=[](std::size_t j)->Located{std::uint64_t h=j+0x9e3779b97f4a7c15ull;h=(h^(h>>30))*0xbf58476d1ce4e5b9ull;h=(h^(h>>27))*0x94d049bb133111ebull;h^=h>>31;return {{std::uint32_t(h&1),std::uint32_t((h>>1)%5)},Fr(0),false};};
 Tape p(true);SessionRandom rng;Commitments pcs(p,l,[](std::size_t){return Fr(7);},&data,&rng);pcs.constrainBaseTails();auto h=exchangeHidden(p,"exhausted/value",Fr(7),&rng);std::size_t calls=0;pcs.defer({"exhausted/query","trusted-exhausted-map",h,point,n,[&](std::size_t j){++calls;return location(j);}});pcs.authenticateUnifiedInput({Fr(11),Fr(13)});need(calls>2*n,"cache exhaustion fixture did not exercise checked fallback");
 for(bool changed:{false,true}){Tape v(false);v.data=p.data;std::size_t vcalls=0;auto replay=[&]{Commitments judge(v,l,{},nullptr,nullptr);judge.constrainBaseTails();auto value=exchangeHidden(v,"exhausted/value",Fr(0),nullptr);judge.defer({"exhausted/query","trusted-exhausted-map",value,point,n,[&](std::size_t j){auto at=location(j);++vcalls;if(changed && vcalls>2*n)at.coordinate.index=(at.coordinate.index+1)%5;return at;}});judge.authenticateUnifiedInput({Fr(11),Fr(13)});need(judge.authenticated() && v.at==v.data.size(),"checked fallback verifier EOF");};if(changed)rejects(replay,"mutable exhausted fallback mapping accepted");else replay();}
 std::cout<<"UNIFIED_EXHAUSTED_PROTOCOL_TESTED callbacks="<<calls<<" initial_twice="<<2*n<<'\n';
}
void interactiveReady(){
 int sockets[2];need(socketpair(AF_UNIX,SOCK_STREAM,0,sockets)==0,"unified READY socketpair");char name[]="build/protocol1-ready-XXXXXX";int audit=mkstemp(name);need(audit>2,"unified READY audit file");auto child=fork();need(child>=0,"unified READY fork");
 if(!child){close(sockets[0]);dup2(sockets[1],0);dup2(sockets[1],1);close(sockets[1]);try{PipeIO io(false,"unified-ready","","R7-CLASSIFIED-PROTOCOL1-UNIFIED-INTERACTIVE-v1",PipeIO::Profile::ClassifiedV2,audit);_exit(0);}catch(const std::exception&e){std::cerr<<e.what()<<'\n';_exit(2);}}
 close(sockets[1]);Bytes frame;std::array<unsigned char,4096>buffer;for(;;){auto n=read(sockets[0],buffer.data(),buffer.size());if(n<0 && errno==EINTR)continue;need(n>=0,"unified READY read");if(!n)break;frame.insert(frame.end(),buffer.begin(),buffer.begin()+n);}close(sockets[0]);int status=0;need(waitpid(child,&status,0)==child,"unified READY wait");close(audit);need(WIFEXITED(status) && WEXITSTATUS(status)==0 && frame.size()>4,"new unified interactive READY rejected");
 Bytes header(frame.begin(),frame.begin()+4);Reader length{header};need(length.number()==frame.size()-4,"unified READY frame length");auto record=R7Wire::decodeLocalRecord(Bytes(frame.begin()+4,frame.end()));std::string expected="R7-CLASSIFIED-PROTOCOL1-UNIFIED-INTERACTIVE-v1";need(record.label=="READY/unified-ready" && record.payload==Bytes(expected.begin(),expected.end()),"unified READY suite was downgraded");std::cout<<"UNIFIED_INTERACTIVE_READY_TESTED audit="<<name<<'\n';
}
int main(int argc,char**argv){try{r1::init();if(argc>1 && std::string(argv[1])=="large-domain"){largeVirtualDomainTests();return 0;}if(argc>1 && std::string(argv[1])=="interactive"){interactiveReady();return 0;}if(argc>1 && std::string(argv[1])=="exhaustion"){exhaustedProtocolTest();return 0;}denseRoundTests();largeVirtualDomainTests();hiddenProtocolTests();mappedProtocolTests();if(argc==1 || std::string(argv[1])!="algebra")contract();std::cout<<"UNIFIED_INPUT_TESTED\n";return 0;}catch(const std::exception&e){std::cerr<<"UNIFIED_INPUT_FAILED "<<e.what()<<'\n';return 1;}}

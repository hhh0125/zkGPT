#define main retained_perf4_opt3a_main
#include "opt3a_kernel_tests.cpp"
#undef main

// Queued built-in view maps must own immutable public mapping metadata.
// Mutating a live Layout must still be rejected at the authentication barrier;
// it must not mutate the already queued map through a retained View reference.
int main(){try{
 r1::init();
 for(unsigned mutation=0;mutation<10;++mutation){
  Layout p;p.native=std::string(64,'a');p.master_actual=12;
  p.roots={{"C_aux",8,8,true,false},{"C_lookup/queries",8,8,true,false},{"C_lookup/helpers",8,8,false,false}};
  p.base_runs={{0,6,{0,0}},{6,6,{1,1}}};
  View a;a.id="perf4/mapped";a.actual=5;a.padded=8;a.mapped=true;a.kind=ObjectKind::MappedChild;a.spans={{4,5,Fr(-2)}};p.views.emplace(a.id,a);
  View b;b.id="perf4/public";b.actual=8;b.padded=8;b.mapped=true;b.kind=ObjectKind::MappedChild;b.spans={{0,3,Fr(3)},{6,5,Fr(-1)}};p.views.emplace(b.id,b);
  View h;h.id="perf4/helper";h.actual=5;h.padded=8;h.kind=ObjectKind::ExpHelper;h.start={2,0};h.pieces={{0,3,{2,0}},{3,5,{2,3}}};p.views.emplace(h.id,h);
  Prepared data;data[h.id]=std::make_shared<std::vector<Fr>>(8,Fr(7));FixedRandom random(41);Tape tape;
  Commitments c(tape,p,[](std::size_t j){return Fr(j+11);},&data,&random,std::vector<Fr>{Fr(11),Fr(13)});c.constrainBaseTails();
  bool native=mutation>=4 && mutation<=6;std::size_t indexBump=0;
  auto id=(mutation==2 || mutation==9)?h.id:mutation>=7?b.id:a.id;auto oracle=c.view(id);std::vector<Fr>point{Fr(mutation>=7?2:0),Fr(mutation>=7?3:1),Fr(-3)};auto weights=r1::weights(point);Fr y=0;
  const std::vector<std::vector<Fr>>physical{{Fr(11),Fr(13),Fr(13),Fr(14),Fr(15),Fr(16),Fr(0),Fr(0)},{Fr(0),Fr(17),Fr(18),Fr(19),Fr(20),Fr(21),Fr(22),Fr(0)},std::vector<Fr>(8,Fr(7))};
  if(!native)for(std::size_t j=0;j<8;++j){auto ref=locate(p,p.views.at(id),j);auto want=ref.constant?ref.bias:physical.at(ref.coordinate.root).at(ref.coordinate.index)+ref.bias;need(oracle.value(j)==want,"PERF4 source cache changed independent oracle/padding/public X value");}
  for(std::size_t j=0;j<8;++j)y+=weights[j]*(native?(j<5?Fr(j+15):Fr(0)):oracle.value(j));
  auto hidden=exchangeHidden(tape,"perf4/snapshot/value",y,&random);
  if(native)c.deferBase("perf4/snapshot/native",5,8,[&](std::size_t j){return 4+j+(j==0?indexBump:0);},hidden,point);
  else oracle.open(tape,{point,hidden.handle},&hidden,&random,"perf4/snapshot/open");
  auto&claim=Access::claims(c).back();std::vector<Located>before;for(std::size_t j=0;j<8;++j)before.push_back(claim.map(j));
  for(std::size_t j=0;j<8;++j){auto ref=native?(j>=5?Located{{},Fr(0),true}:Located{baseCoordinate(p,4+j),Fr(0),false}):locate(p,p.views.at(id),j);
   if(!ref.constant && ref.coordinate.root==0 && ref.coordinate.index<2){ref.bias+=Fr(ref.coordinate.index==0?11:13);ref.constant=true;ref.coordinate={};}
   need(before[j].coordinate==ref.coordinate && before[j].bias==ref.bias && before[j].constant==ref.constant,"PERF4 source cache differs from independent original locator and Statement X");}
  if(mutation==3 || mutation==6 || mutation==7 || mutation==9){c.authenticateUnifiedInput({Fr(11),Fr(13)});need(c.authenticated() && Access::metrics(c).map_calls>0,"PERF4 pristine positive authentication failed");continue;}
  if(mutation==5){indexBump=1;bool rejected=false;try{c.authenticateUnifiedInput({Fr(11),Fr(13)});}catch(const std::exception&e){rejected=std::string(e.what())=="classified source map changed after defer";}need(rejected && !c.authenticated(),"PERF4 static base locator bypassed live index-callback digest validation");continue;}
  if(mutation==0)p.views.at(id).spans[0].bias=Fr(9);
  if(mutation==1)p.base_runs[0].start.index=1;
  if(mutation==2)p.views.at(id).pieces[0].start.index=1;
  if(mutation==4)p.base_runs[0].start.index=1;
  if(mutation==8)p.views.at(id).spans[0].bias=Fr(9);
  for(std::size_t j=0;j<8;++j){auto after=claim.map(j);need(after.coordinate==before[j].coordinate && after.bias==before[j].bias && after.constant==before[j].constant,"PERF4 queued view map retained mutable layout metadata");}
  auto prefix=tape.prefix();auto messages=tape.data.size();auto calls=Access::metrics(c).map_calls;bool rejected=false;
  try{c.authenticateUnifiedInput({Fr(11),Fr(13)});}catch(const std::exception&e){rejected=std::string(e.what())=="unified missing/closed/mutated authentication";}
  need(rejected && !c.authenticated(),"PERF4 cached source map bypassed live layout identity barrier");
  need(tape.prefix()==prefix && tape.data.size()==messages && Access::metrics(c).map_calls==calls,"PERF4 mutated layout reached transcript or map preparation");
 }
 // Coarse-directory bucket boundaries and run cuts deliberately disagree.
 // Compare every native coordinate, including both4096and8192 boundaries.
 {
  Layout p;p.native=std::string(64,'a');p.master_actual=8195;
  p.roots={{"C_aux",8192,8192,true,false},{"C_lookup/queries",8192,8192,true,false}};
  p.base_runs={{0,4093,{0,0}},{4093,4096,{1,0}},{8189,6,{0,4093}}};
  Prepared data;FixedRandom random(29);Tape tape;Commitments c(tape,p,[](std::size_t j){return Fr(j+11);},&data,&random,std::vector<Fr>{Fr(11),Fr(13)});c.constrainBaseTails();
  std::vector<Fr>point(14,Fr(2));auto weights=r1::weights(point);Fr value=0;for(std::size_t j=0;j<8195;++j)value+=weights[j]*Fr(j==1?13:j+11);
  auto h=exchangeHidden(tape,"perf4/directory/value",value,&random);c.deferBase("perf4/directory/open",8195,16384,[](std::size_t j){return j;},h,point);auto&claim=Access::claims(c).back();
  for(std::size_t j=0;j<16384;++j){Located ref=j>=8195?Located{{},Fr(0),true}:Located{baseCoordinate(p,j),Fr(0),false};if(j<2){ref.bias=Fr(j==0?11:13);ref.constant=true;ref.coordinate={};}auto got=claim.map(j);need(got.coordinate==ref.coordinate && got.bias==ref.bias && got.constant==ref.constant,"PERF4 base directory bucket/cut/padding mismatch");}
  c.authenticateUnifiedInput({Fr(11),Fr(13)});need(c.authenticated(),"PERF4 directory full authentication failed");
 }
 std::cout<<"PERF4_IMMUTABLE_SOURCE_SNAPSHOT_AND_MUTATED_LAYOUT_REJECTION_PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

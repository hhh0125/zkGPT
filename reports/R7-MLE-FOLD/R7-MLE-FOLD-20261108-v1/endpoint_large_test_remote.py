from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');P=R/'src/paper_r7';p=P/'category_k_endpoint_tests.cpp';s=p.read_text()
s=s.replace(' static void endpoints(', ' static void requireFast(const Commitments&pcs){need(pcs.auth_metrics_.endpoint_fast_coordinates>0,"real verifier did not use admitted endpoint");}\n static void endpoints(',1)
at=s.index('\n}\nint main()')
extra=r'''
Layout largeFixture(){R7Context c;c.layout_sha256=std::string(64,'d');c.plan.master=r1::PCSpec::make("master",4096,4096,r1::Layout::RowMajor);
 c.plan.sources.push_back({r1::PCSpec::make("query",64,64,r1::Layout::RowMajor),{{2240,64,Fr(2)}}});c.plan.sources.push_back({r1::PCSpec::make("W",2048,2048,r1::Layout::RowMajor),{{128,2048,Fr(0)}}});
 c.objects={{"base","",ObjectKind::BaseMaster,4096,4096,{},false},{"query","base",ObjectKind::MappedChild,64,64,c.plan.sources[0].spans,false},{"W","base",ObjectKind::ModelW,2048,2048,c.plan.sources[1].spans,false},{"helper","",ObjectKind::ExpHelper,2047,2048,{},false}};
 return categoryLayout(c,{{2,64,Fr(0)}},256);
}
void largeRun(Tape&io,const Layout&layout,SessionRandom*rng,bool require_fast){Prepared prepared;auto native=std::make_shared<std::vector<Fr>>(4096);for(std::size_t j=0;j<native->size();++j)native->at(j)=Fr(j+1);Value base;if(io.prover()){base=[native](std::size_t j){return native->at(j);};prepared["helper"]=std::make_shared<std::vector<Fr>>(2048,Fr(13));prepared["helper"]->back()=Fr(999);}
 std::vector<Fr>x;for(std::size_t j=2;j<66;++j)x.push_back(native->at(j));Commitments pcs(io,layout,base,io.prover()?&prepared:nullptr,rng,x);pcs.constrainBaseTails();
 for(auto id:{"W","query","helper"}){auto source=pcs.view(id);std::vector<Fr>point;for(unsigned j=0;j<r1::log2exact(source.padded);++j)point.push_back(io.coin(std::string("large/")+id+"/point/"+std::to_string(j)));Fr y=0;if(io.prover()){std::vector<Fr>values(source.padded);for(std::size_t j=0;j<values.size();++j)values[j]=source.value(j);y=r1::mle(values,point);}auto h=exchangeHidden(io,std::string("large/")+id,y,rng);source.open(io,{point,h.handle},io.prover()?&h:nullptr,rng,std::string("large/")+id+"/opening");}
 pcs.authenticateUnifiedInput(x);need(pcs.authenticated(),"large endpoint protocol not authenticated");if(!io.prover() && require_fast)AuthenticationTestAccess::requireFast(pcs);
}
'''
s=s[:at]+extra+s[at:];anchor=' r1::init();SessionRandom rng;auto l=fixture();';assert anchor in s
s=s.replace(anchor,anchor+r'''
 {auto large=largeFixture();Tape producer(true);largeRun(producer,large,&rng,false);setenv("ZKGPT_CATEGORY_K_ENDPOINT","0",1);Tape dense(false);dense.data=producer.data;largeRun(dense,large,nullptr,false);setenv("ZKGPT_CATEGORY_K_ENDPOINT","1",1);Tape direct(false);direct.data=producer.data;largeRun(direct,large,nullptr,true);need(dense.at==direct.at && direct.at==producer.data.size() && dense.prefix()==direct.prefix(),"endpoint changed proof framing or challenges");unsetenv("ZKGPT_CATEGORY_K_ENDPOINT");}
''');p.write_text(s)

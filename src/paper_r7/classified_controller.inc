// Included only in the v2 branch of the actual complete Oneblock entrypoint.
// Shares statement isolation, native discovery, PipeIO and verifier-local ledger.
#ifdef R7_FS_ONEBLOCK
 const auto&trust=fsTrust;
#else
 auto trust=[](){master3::QuietStdout quiet;return model::loadTrust();}();
#endif
 #ifdef R7_FS_FULL12
 std::vector<model::Secret>modelSecrets;
#else
 std::optional<model::Secret>modelSecret;
#endif
 R7WitnessStore witness;Value master;
 std::optional<LLM>network;layeredCircuit publicGraph;const layeredCircuit*graph=nullptr;
 classified::Prepared prepared;
 timed("frozen-witness-and-prechallenge-lookup",[&]{if(isP){
  {master3::QuietStdout quiet;
#ifdef R7_FS_FULL12
 witness.generateR6(ctx,executionTarget(12).manifest_path);modelSecrets=full12::prepare(trust,ctx,masterValue(witness),io.session(),random);
#else
 witness.generateR6(ctx,"reports/R6A/oneblock-model-manifest-v1.txt");modelSecret=model::prepare(trust,io.session(),random);
#endif
 }
  graph=&witness.executionForR7().C;master=masterValue(witness);
  if(attack=="wrong-internal" || attack=="wrong-weight"){auto old=master3::trustedLayout(ctx);auto it=std::find_if(old.regions.begin(),old.regions.end(),[&](auto&r){return attack=="wrong-internal"?r.id=="R6/witness/round_0_affine":r.kind=="W";});need(it!=old.regions.end(),"classified attack native region");witness.executionForR7().val[0].at(it->offset)+=Fr(1);}
  prepared=classified::prepareLookup(ctx,master);
  if(attack=="wrong-Auxiliary" || attack=="wrong-lookup"){auto it=std::find_if(prepared.begin(),prepared.end(),[](auto&e){return e.first.find("/read")!=std::string::npos;});need(it!=prepared.end(),"classified read attack target");it->second->at(0)+=Fr(1);}
  if(attack=="wrong-mapping")std::swap(layout.base[0],layout.base[1]);
 }else{master3::QuietStdout quiet;network.emplace(ctx.plan.shape.layer_count,12,64,768,3072);r6::configure(*network);publicGraph=network->buildPublicCircuit();graph=&publicGraph;}});
 std::optional<classified::Commitments>commitments;
 timed("all-classified-roots-prechallenge-seal",[&]{
#ifdef R7_FS_FULL12
 commitments.emplace(io,layout,master,isP?&prepared:nullptr,rng,nullptr,nullptr,&trust.chunks,isP?&modelSecrets:nullptr);
#else
 commitments.emplace(io,layout,master,isP?&prepared:nullptr,rng,&trust,isP?&*modelSecret:nullptr);
#endif
});
 auto&pcs=*commitments;for(auto&r:layout.roots)checked(io,"classified/root/"+r.id+"/commitment");
 checked(io,"model/checkpoint-exporter-profile");checked(io,"model/anchor-hiding-rerandomization");
 #ifdef R7_FS_FULL12
 if(isP && attack=="wrong-model-witness")modelSecrets.front().values.at(0)+=Fr(1);
#else
 if(isP && attack=="wrong-model-witness")modelSecret->values.at(0)+=Fr(1);
#endif
 timed("complete-classified-base-knowledge-and-tail",[&]{pcs.registerBase();});for(auto&r:layout.roots)if(r.registered)checked(io,"classified/root/"+r.id+"/knowledge-tail");
 if(!isP && std::getenv("R7_CLASSIFIED_LAYOUT_FD")){std::ostringstream out;classified::writeLayout(out,layout);output(fd("R7_CLASSIFIED_LAYOUT_FD"),out.str());}
 timed("classified-public-X-Y-constants",[&]{
  auto entries=trustedPublicEntries(s,ctx);auto all=std::make_shared<PublicEntries>(std::move(entries.x));all->insert(all->end(),entries.y.begin(),entries.y.end());all->insert(all->end(),entries.constants.begin(),entries.constants.end());
  Writer binding;binding.text(liveSuite);binding.text(classified::identity(layout));binding.number(all->size());for(auto&e:*all){binding.number(e.first,8);binding.scalar(e.second);}bindPublic(io,"Classified/public-entries",binding.b);
  auto N=classified::power2(all->size());std::vector<Fr>point;for(unsigned j=0;j<r1::log2exact(N);++j)point.push_back(io.coin("Classified/public-query/"+std::to_string(j)));auto weights=r1::weights(point);Fr expected=0;for(std::size_t j=0;j<all->size();++j)expected+=weights[j]*all->at(j).second;
  pcs.deferBase("public/X-Y-constants",all->size(),N,[all](std::size_t j){return all->at(j).first;},publicHidden(io,"Classified/public-expected",expected),point);
 });for(auto id:{"public/X","public/Y","public/constants"})checked(io,id);
 std::optional<PendingGKR>pendingHolder;timed("all-native-GKR",[&]{pendingHolder.emplace(runHiddenGKR(io,*graph,isP?&witness.executionForR7():nullptr,ctx.plan.shape.sequence_length,rng));});auto&pending=*pendingHolder;
 // Every later private read is served by sealed classified roots/model W.
 // Terminal maps borrow graph wiring, never these native field arrays.
 if(isP){for(auto&values:witness.executionForR7().val)std::vector<Fr>().swap(values);master={};}
 std::set<std::string>objects,terminals;std::vector<VectorOracle>sources;
 for(auto&source:ctx.plan.sources){auto id="r7/"+source.spec.role;need(objects.insert(id).second,"classified repeated source");sources.push_back(pcs.view(id));}
 VectorFactory factory=[&](IO&live,std::string id,std::size_t actual,std::size_t padded,Value){need(&live==&io && objects.insert(id).second,"classified duplicate/foreign prepared view");auto v=pcs.view(id);need(v.actual==actual && v.padded==padded,"classified wrong native view shape");return v;};
 timed("native-Range-Exp-Lasso-Memory-Product",[&]{for(auto&r:ctx.plan.ranges)hiddenRange(io,sources.at(r.child),r.bits,factory,rng,"r7/"+r.id,true);for(auto&e:ctx.plan.exps)hiddenExp(io,sources.at(e.index_child),sources.at(e.value_child),e.profile,factory,rng,"r7/"+e.id,true);});
 for(auto&t:pending.terminals){const auto&l=graph->circuit.at(t.layer);const auto*indices=t.port?&l.ori_id_v:&l.ori_id_u;auto id="gkr/layer/"+std::to_string(t.layer)+"/terminal/"+(t.port?"v":"u")+"/"+std::to_string(t.matrix_point);need(terminals.insert(id).second,"classified repeated terminal");pcs.deferBase(id,t.actual,t.padded,[indices](std::size_t j){return indices->at(j);},t.value,t.point);}
 std::set<std::string>requiredTerminals;for(auto&r:plan.required)if(r.family=="GKR_MASTER_TERMINAL")requiredTerminals.insert(r.id);need(terminals==requiredTerminals,"classified native terminal omission/substitution");
 timed("all-source-original-handles",[&]{for(std::size_t j=0;j<sources.size();++j){auto&v=sources[j];auto label="Classified/source/"+std::to_string(j);std::vector<Fr>point;for(unsigned k=0;k<r1::log2exact(v.padded);++k)point.push_back(io.coin(label+"/query/"+std::to_string(k)));auto h=exchangeHidden(io,label+"/value",isP?evaluateOracle(v,point):Fr(0),rng);v.open(io,{point,h.handle},isP?&h:nullptr,rng,label+"/opening");}});
 need(objects.size()==layout.views.size(),"classified unused/missing native view");
 timed("mandatory-classified-authentication-barrier",[&]{pcs.authenticate();});need(pcs.authenticated(),"classified barrier INCOMPLETE");io.authenticationComplete();checked(io,"classified/authentication-barrier");
 for(auto&[id,v]:layout.views)checked(io,"classified/view/"+id);for(auto&id:terminals)checked(io,id);for(auto&source:ctx.plan.sources)checked(io,"mapped/"+source.spec.role);
 for(auto id:{"gkr/input-aggregation","gkr/input-aggregation-terminal","model/C_W_model-to-classified-input"})checked(io,id);for(auto&name:plan.model_regions)checked(io,"model/region/"+name);for(auto&r:plan.required)if(r.family=="CROSS_BLOCK_STATE")checked(io,r.id);
 if(isP)need(witness.legacyCommitmentAbsent(),"classified old nonhiding PC forbidden");if(!isP)io.complete();io.coin("Oneblock/all-mandatory-relations-verified/ack");
#ifdef R7_FS_ONEBLOCK
 pipe.finish();std::cerr<<pipe.metadataJson()<<'\n';
#endif
 io.save(true);
 struct rusage usage{};getrusage(RUSAGE_SELF,&usage);auto m=metrics();std::cerr<<"{\"status\":\""<<successStatus<<"\",\"suite\":\""<<liveSuite<<"\",\"required_count\":"<<plan.required.size()<<",\"coverage_sha256\":\""<<identity(plan)<<"\",\"layout_id\":\""<<classified::identity(layout)<<"\",\"root_count\":"<<layout.roots.size()<<",\"claims\":"<<pcs.claims()<<",\"registered_rows\":"<<pcs.registered_rows<<",\"registration_response_bytes\":"<<pcs.registration_bytes<<",\"GKR_rounds\":"<<pending.rounds<<",\"seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<",\"wire_bytes\":"<<pipe.wireBytes()<<",\"P_wire_bytes\":"<<pipe.proofBytes()<<",\"frames\":"<<pipe.frames()<<",\"peak_rss_kib\":"<<usage.ru_maxrss<<",\"MSM_calls\":"<<m.msm_calls<<",\"MSM_terms\":"<<m.msm_terms<<",\"production_gate\":\"BLOCKED_CRYPTOGRAPHIC_DESIGN\",\"may_enter_stage2\":false,\"phases\":[";for(std::size_t j=0;j<phases.size();++j){if(j)std::cerr<<',';auto&p=phases[j];std::cerr<<"{\"name\":\""<<p.name<<"\",\"seconds\":"<<p.seconds<<",\"P_wire_bytes\":"<<p.p_wire_bytes<<",\"MSM_calls\":"<<p.msm_calls<<",\"rss_end_kib\":"<<p.rss_kib<<",\"cumulative_peak_rss_kib\":"<<p.peak_rss_kib<<'}';}std::cerr<<"]}\n";return 0;

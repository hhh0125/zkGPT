def edit(name,old,new):
 p=Path('src/paper_r7')/name;s=p.read_text();assert old in s,(name,old);p.write_text(s.replace(old,new))
edit('classified_commitments.hpp','model::Secret*onlyP_model=nullptr);','model::Secret*onlyP_model=nullptr,const std::vector<model::Trust>*chunks=nullptr,std::vector<model::Secret>*onlyP_chunks=nullptr);')
edit('classified_commitments.hpp','static constexpr std::size_t map_limit=16u<<20,weight_limit=32u<<20;','const std::size_t map_limit,weight_limit;')
edit('classified_commitments.hpp','explicit Authentication(std::size_t n):claims(n){}','explicit Authentication(std::size_t n,bool full12=false):map_limit(full12?256u<<20:16u<<20),weight_limit(full12?256u<<20:32u<<20),claims(n){}')
edit('classified_commitments.cpp','const model::Trust*trust,model::Secret*modelSecret):','const model::Trust*trust,model::Secret*modelSecret,const std::vector<model::Trust>*chunks,std::vector<model::Secret>*chunkSecrets):')
edit('classified_commitments.cpp','need(!layout.master_actual,"Full12 partitioned commitments require independently pinned chunk-anchor runtime; unavailable");','auto modelCount=std::count_if(layout.roots.begin(),layout.roots.end(),[](auto&r){return r.model;});need(layout.master_actual?(chunks && chunks->size()==std::size_t(modelCount) && !trust && !modelSecret && (io.prover()?(chunkSecrets && chunkSecrets->size()==chunks->size()):!chunkSecrets)):(!chunks && !chunkSecrets),"classified Full12 independently loaded chunk trust boundary");')
edit('classified_commitments.cpp','roots_.resize(layout.roots.size());','roots_.resize(layout.roots.size());std::set<std::string>usedPrepared;')
edit('classified_commitments.cpp','if(root.spec.registered){for(std::size_t i=0;i<layout.base.size();++i)if(layout.base[i].root==k)packed->at(layout.base[i].index)=base(i);}','if(root.spec.registered){if(layout.master_actual){for(auto&run:layout.base_runs)if(run.start.root==k)for(std::size_t j=0;j<run.count;++j)packed->at(run.start.index+j)=base(run.begin+j);}else for(std::size_t i=0;i<layout.base.size();++i)if(layout.base[i].root==k)packed->at(layout.base[i].index)=base(i);}')
edit('classified_commitments.cpp','else for(auto&[id,view]:layout.views)if(!view.mapped && view.start.root==k)', 'else if(layout.master_actual){for(auto&[id,view]:layout.views)if(!view.mapped)for(auto&piece:view.pieces)if(piece.start.root==k){auto it=prepared->find(id);need(it!=prepared->end() && it->second->size()==view.padded && piece.begin<=view.padded && piece.count<=view.padded-piece.begin && piece.start.index<=packed->size() && piece.count<=packed->size()-piece.start.index,"classified prepared partition shape/bounds");std::copy_n(it->second->begin()+piece.begin,piece.count,packed->begin()+piece.start.index);usedPrepared.insert(id);}}\n   else for(auto&[id,view]:layout.views)if(!view.mapped && view.start.root==k)')
p=Path('src/paper_r7/classified_commitments.cpp');s=p.read_text();start=s.index(' for(std::size_t k=0;k<roots_.size();++k)if(roots_[k].spec.model)');end=s.index(' if(io.prover())need(prepared->empty()',start)
s=s[:start]+r''' if(io.prover() && layout.master_actual){need(usedPrepared.size()==prepared->size(),"classified unused/missing partitioned helper");prepared->clear();}
 std::size_t chunkIndex=0;
 for(std::size_t k=0;k<roots_.size();++k)if(roots_[k].spec.model){
  const model::Trust*activeTrust=layout.master_actual?&chunks->at(chunkIndex):trust;model::Secret*activeSecret=layout.master_actual?(io.prover()?&chunkSecrets->at(chunkIndex):nullptr):modelSecret;need(activeTrust,"classified missing model authority");
  std::string scope=layout.master_actual?"Full12/model/chunk/"+std::to_string(chunkIndex):"";std::optional<stage2::ScopedIO>scoped;if(!scope.empty())scoped.emplace(io,scope);IO&activeIO=scoped?static_cast<IO&>(*scoped):io;
  auto pub=model::registerModel(activeIO,*activeTrust,activeSecret,rng);auto&root=roots_[k];need(root.spec.actual==pub.spec.actual && root.spec.padded==pub.spec.padded,"classified W trusted dimensions");root.pc={root.spec.id,pub.spec.actual,pub.spec.padded,pub.spec.rows,pub.spec.columns,pub.rows,r1::Layout::RowMajor};root.basis=r1::generators(activeTrust->anchor.w.spec).g;
  if(io.prover()){root.value=[activeSecret](std::size_t i){return activeSecret->value(i);};root.blinds=activeSecret->blinds;}
  root.open=[activeTrust,pub,activeSecret,scope](IO&live,const Query&q,const Hidden*h,Random*r,const std::string&id){if(scope.empty())model::opening(live,*activeTrust,pub,q,activeSecret,h,r,id);else{stage2::ScopedIO scoped(live,scope);model::opening(scoped,*activeTrust,pub,q,activeSecret,h,r,id);}};
  ++chunkIndex;
 }
'''+s[end:];p.write_text(s)
edit('classified_commitments.cpp','if(!v.mapped)c.only_root=int(v.start.root);','if(!v.mapped && v.pieces.size()<=1)c.only_root=int(v.start.root);')
edit('classified_commitments.cpp','c.padded<=(1u<<24)','c.padded<=r1::kMaxVector')
edit('classified_commitments.cpp','at<layout_.base.size(),"classified base map out of bounds");return {layout_.base[at],','at<baseSize(layout_),"classified base map out of bounds");return {baseCoordinate(layout_,at),')
edit('classified_commitments.cpp','Authentication auth(claims_.size());','Authentication auth(claims_.size(),layout_.master_actual!=0);')
edit('fs_io.hpp','inline constexpr const char* hashSuite=','inline constexpr const char* full12Suite="R7-FULL12-CLASSIFIED-HYRAX-LOOKUP-FS-ROWMAJOR-CANDIDATE-v1";\ninline constexpr const char* hashSuite=')
edit('fs_io.hpp','CanonicalTranscript(const Bytes&header,const Bytes&public_instance);','CanonicalTranscript(const Bytes&header,const Bytes&public_instance,bool full12=false);')
edit('fs_io.cpp','b.proof_type=="Oneblock" ||','b.proof_type=="Full12" || b.proof_type=="Oneblock" ||')
edit('fs_io.cpp','w.text(suite);w.text(hashSuite);','w.text(b.proof_type=="Full12"?full12Suite:suite);w.text(hashSuite);')
edit('fs_io.cpp','r.text()==suite && r.text()==hashSuite','r.text()==(b.proof_type=="Full12"?full12Suite:suite) && r.text()==hashSuite')
edit('fs_io.cpp','std::uint64_t seq=0,msg=0,coins=0,rejected=0;','std::uint64_t seq=0,msg=0,coins=0,rejected=0,max_events=eventLimit;')
edit('fs_io.cpp','Impl(const Bytes&h,const Bytes&i){','Impl(const Bytes&h,const Bytes&i,bool full12){max_events=full12?16ull*eventLimit:eventLimit;')
edit('fs_io.cpp','need(seq<eventLimit &&','need(seq<max_events &&')
edit('fs_io.cpp','CanonicalTranscript::CanonicalTranscript(const Bytes&h,const Bytes&i):impl_(std::make_unique<Impl>(h,i))','CanonicalTranscript::CanonicalTranscript(const Bytes&h,const Bytes&i,bool full12):impl_(std::make_unique<Impl>(h,i,full12))')
edit('fs_io.cpp','std::uint64_t total=0,max_payload=0,file_syscalls=0;','std::uint64_t total=0,max_payload=0,file_syscalls=0,limit=byteLimit;std::string suite_name=suite;')
edit('fs_io.cpp','byteLimit-total','limit-total')
edit('fs_io.cpp','validate(b);auto&v=*impl_;Bytes h;','validate(b);auto&v=*impl_;v.limit=b.proof_type=="Full12"?64ull<<30:byteLimit;v.suite_name=b.proof_type=="Full12"?full12Suite:suite;Bytes h;')
edit('fs_io.cpp','std::uint64_t(v.original.st_size)<=byteLimit','std::uint64_t(v.original.st_size)<=v.limit')
edit('fs_io.cpp','std::make_unique<CanonicalTranscript>(h,b.public_instance)','std::make_unique<CanonicalTranscript>(h,b.public_instance,b.proof_type=="Full12")')
edit('fs_io.cpp','<<suite<<"\\\",\\\"session','<<v.suite_name<<"\\\",\\\"session')
print('PARTITIONED_COMMITMENTS_AND_FULL12_FS_RUNTIME_IMPLEMENTED')

def edit(name,old,new):
 p=Path(name);s=p.read_text();assert old in s,(name,old);p.write_text(s.replace(old,new))
edit('src/paper_r7/r6_adapter.hpp','const std::string&file,const std::string&pin);','const std::string&file,const std::string&pin,unsigned blocks=1);')
edit('src/paper_r7/r6_adapter.cpp','checkManifest(const std::string&file,const std::string&pin){','checkManifest(const std::string&file,const std::string&pin,unsigned blocks){')
edit('src/paper_r7/r6_adapter.cpp','r6::parseManifest(bytes,1);return pin;','executionTarget(blocks);r6::parseManifest(bytes,blocks);return pin;')
edit('src/paper_r7/r6_adapter.cpp','catch(...){throw std::runtime_error("R7 private witness generation/validation failed; details suppressed");}','catch(const std::bad_alloc&){throw std::runtime_error("R7 private witness resource exhaustion (std::bad_alloc); details suppressed");}\n catch(...){throw std::runtime_error("R7 private witness generation/validation failed; details suppressed");}')
edit('src/paper_r7/r6_adapter.cpp','checkManifest(manifest,c.model_manifest_sha256);','const auto target=executionTarget(c.plan.shape.layer_count);checkManifest(manifest,c.model_manifest_sha256,target.blocks);')
edit('src/paper_r7/r6_adapter.cpp','r6::parseManifest(r1::readFile(manifest),1);','r6::parseManifest(r1::readFile(manifest),target.blocks);')
edit('src/paper_r7/r6_adapter.cpp','std::make_unique<LLM>(1,12,64,768,3072)','std::make_unique<LLM>(target.blocks,12,64,768,3072)')
edit('src/paper_r7/r6_adapter.cpp','network_->createFiniteWitness(*execution_);','if(target.blocks==12)network_->createFull12FiniteWitness(*execution_);else network_->createFiniteWitness(*execution_);')
edit('src/neuralNetwork.hpp','void createFiniteWitness(prover &pr);','void createFiniteWitness(prover &pr);\n    // Explicit R7 Full12 witness-only entry. Never calls the legacy PC.\n    // The caller must enforce its memory/CPU budget and suppress private logs.\n    void createFull12FiniteWitness(prover &pr);')
p=Path('src/neuralNetwork.cpp');p.write_text(p.read_text()+'''
void neuralNetwork::createFull12FiniteWitness(prover &pr){
 if(!r6_engineering_ || layer_num!=12 || structure_only || pr.cc.w)
  throw std::invalid_argument("R7 Full12 finite witness requires native12 blocks and no existing commitment");
 r6_skip_commit_=true;try{create(pr,false);r6_skip_commit_=false;}catch(...){r6_skip_commit_=false;throw;}
 if(pr.cc.w)throw std::logic_error("R7 Full12 witness unexpectedly committed");
}
''')
print('FULL12_MANIFEST_AND_EXPLICIT_NO_LEGACY_WITNESS_PATH_CONNECTED')

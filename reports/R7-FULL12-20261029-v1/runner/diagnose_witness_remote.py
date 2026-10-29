p=Path('src/paper_r7/r6_adapter.cpp');s=p.read_text();old='executePrivateWitness([&]{if(target.blocks==12)network_->createFull12FiniteWitness(*execution_);else network_->createFiniteWitness(*execution_);network_->validateCurrentWitness(*execution_);});';assert old in s
new='''try{'''+old+'''}catch(...){
  if(target.blocks==12){std::size_t gates=0,maps=0,fields=0;for(auto&l:execution_->C.circuit){gates+=l.uni_gates.capacity()*sizeof(uniGate)+l.bin_gates.capacity()*sizeof(binGate);maps+=(l.ori_id_u.capacity()+l.ori_id_v.capacity())*sizeof(u32);}for(std::size_t i=1;i<execution_->val.size();++i)fields+=execution_->val[i].capacity()*sizeof(Fr);
   std::cerr<<"FULL12_WITNESS_ALLOCATION_METADATA master_fields="<<(execution_->val.empty()?0:execution_->val[0].size())<<" master_capacity_bytes="<<(execution_->val.empty()?0:execution_->val[0].capacity()*sizeof(Fr))<<" graph_gate_capacity_bytes="<<gates<<" graph_map_capacity_bytes="<<maps<<" nonmaster_capacity_bytes="<<fields<<'\\n';
  }throw;
 }'''
p.write_text(s.replace(old,new))
print('RESOURCE_FAILURE_METADATA_ADDED_NO_WITNESS_VALUES')

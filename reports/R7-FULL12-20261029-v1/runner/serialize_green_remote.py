p=Path('src/paper_r7/partitioned_layout.cpp');s=p.read_text();start=s.index('void writePartitionedLayout(')
s=s[:start]+r'''void writePartitionedLayout(std::ostream&o,const Layout&p){
 // Roles originate from native validated PC specs. No witness values are read.
 o<<"{\"suite\":\"R7-FULL12-CLASSIFIED-PARTITION-ROWMAJOR-v1\",\"status\":\"LAYOUT_PLANNED_NOT_AUTHENTICATED\",\"identity\":\""<<identity(p)<<"\",\"master_actual\":"<<baseSize(p)<<",\"dense_mapping_bytes\":0,\"base_coordinate_runs\":[";
 auto pieces=[&](const std::vector<Piece>&parts){bool first=true;for(auto&r:parts){if(!first)o<<',';first=false;o<<"{\"original\":"<<r.begin<<",\"count\":"<<r.count<<",\"root\":"<<r.start.root<<",\"local\":"<<r.start.index<<'}';}};
 pieces(p.base_runs);o<<"],\"roots\":[";bool first=true;
 for(auto&r:p.roots){if(!first)o<<',';first=false;const auto columns=r.model?r.padded/(std::size_t(1)<<(r1::log2exact(r.padded)/2)):std::min<std::size_t>(1024,r.padded);
  o<<"{\"id\":\""<<r.id<<"\",\"actual\":"<<r.actual<<",\"padded\":"<<r.padded<<",\"rows\":"<<r.padded/columns<<",\"columns\":"<<columns<<",\"registered\":"<<(r.registered?"true":"false")<<",\"model\":"<<(r.model?"true":"false")<<",\"layout\":\"RowMajor\",\"basis_status\":\""<<(r.model?"PENDING_INDEPENDENT_CHUNK_ANCHOR":"EXISTING_HYRAX_A3")<<"\"}";
 }
 o<<"],\"views\":[";first=true;
 for(auto&[id,v]:p.views){if(!first)o<<',';first=false;
  o<<"{\"id\":\""<<id<<"\",\"kind\":"<<unsigned(v.kind)<<",\"actual\":"<<v.actual<<",\"padded\":"<<v.padded<<",\"mapped\":"<<(v.mapped?"true":"false")<<",\"source_spans\":[";
  bool sf=true;for(auto&span:v.spans){if(!sf)o<<',';sf=false;o<<"{\"base\":"<<span.offset<<",\"count\":"<<span.count<<",\"signed_bias_Fr_decimal\":\""<<span.bias.getStr()<<"\"}";}
  o<<"],\"pieces\":[";pieces(v.pieces);o<<"]}";
 }
 o<<"]}\n";
}
}
'''
p.write_text(s);print('PARTITION_REPORT_NOW_SELF_CONTAINED_FOR_ROUTING')

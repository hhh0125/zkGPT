from pathlib import Path
R=Path('/home/yttan/project/zkGPT/zkGPT-main/zkGPT-work');assert Path.cwd().resolve()==R
P=R/'src/paper_r7';p=P/'classified_commitments.cpp';s=p.read_text();a=s.index('class Weights {');b=s.index('\n',a);s=s[:a]+s[b:]
a=s.index('struct Commitments::Authentication {');b=s.index('\nFr Commitments::prepareClaim',a);definition=s[a:b].replace('struct Commitments::Authentication','struct Authentication');s=s[:a]+'''Commitments::Weights::Weights(const std::vector<Fr>&q,AuthMetrics*m):split_(q.size()/2){auto t=processCPU();a_=r1::weights({q.begin(),q.begin()+split_});b_=r1::weights({q.begin()+split_,q.end()});if(m){++m->weights;m->weight_terms+=a_.size()+b_.size();m->weights_cpu+=processCPU()-t;}}
std::size_t Commitments::Weights::bytes()const{return (a_.capacity()+b_.capacity())*sizeof(Fr);}
Fr Commitments::Weights::at(std::size_t i)const{return a_[i&(a_.size()-1)]*b_[i>>split_];}
'''+s[b:];p.write_text(s)
p=P/'classified_commitments.hpp';s=p.read_text().replace(' struct Authentication;', ''' class Weights {
  std::vector<Fr>a_,b_;unsigned split_;
  public:explicit Weights(const std::vector<Fr>&,AuthMetrics*=nullptr);
  std::size_t bytes()const;Fr at(std::size_t)const;
 };
 '''+definition);s=s.replace('AuthMetrics*=nullptr','AuthMetrics* =nullptr');p.write_text(s)
print('PRIVATE TYPES AVAILABLE TO DIRECTED FRIEND HARNESS')

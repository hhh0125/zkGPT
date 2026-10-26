#include "sigma.hpp"
#include <set>
namespace zkgpt::r7::crypto {
void sigma(IO&io,const Relations&s,const std::vector<Hidden>*w,Random*rng){
 need(!s.handles.empty() && s.handles.size()<=64 && s.linear.size()<=32 && s.products.size()<=4,"R7 Sigma relation bound");
 Writer statement;statement.text(s.role);auto parent=io.prefix();statement.raw(Bytes(parent.begin(),parent.end()));statement.number(s.handles.size());std::set<std::string>roles;
 for(auto&h:s.handles){validate(h,io.session(),h.role);need(roles.insert(h.role).second,"R7 Sigma duplicate handle role");auto b=encode(h);statement.number(b.size());statement.raw(b);}
 statement.number(s.linear.size());for(auto&line:s.linear){need(line.size()==s.handles.size(),"R7 linear relation dimension");for(auto&a:line)statement.scalar(a);}
 statement.number(s.products.size());for(auto&p:s.products)for(auto index:p){need(index<s.handles.size(),"R7 product handle index");statement.number(index);}
 bindPublic(io,s.role+"/statement",statement.b);
 if(io.prover()){need(w && rng && w->size()==s.handles.size(),"R7 Sigma witness shape");for(std::size_t i=0;i<w->size();++i)need(encode(w->at(i).handle)==encode(s.handles[i]),"R7 Sigma full handle substitution");}
 std::vector<std::array<Fr,2>> masks;std::vector<std::array<Fr,5>> mulmasks;Writer first;
 if(io.prover()){
  for(std::size_t i=0;i<s.handles.size();++i){masks.push_back({rng->scalar(),rng->scalar()});first.point(scalarCommit(masks.back()[0],masks.back()[1]));}
  for(auto&p:s.products){std::array<Fr,5>b;for(auto&x:b)x=rng->scalar();mulmasks.push_back(b);first.point(scalarCommit(b[0],b[1]));first.point(scalarCommit(b[2],b[3]));auto pp=parameters(1);first.point(s.handles[p[0]].point*b[2]+pp.hiding*b[4]);}
 }
 auto bytes=io.message(s.role+"/first-masks",first.b);Reader read{bytes};std::vector<G1>A;std::vector<std::array<G1,3>>M;
 for(std::size_t i=0;i<s.handles.size();++i)A.push_back(read.point());
 for(std::size_t i=0;i<s.products.size();++i)M.push_back({read.point(),read.point(),read.point()});
 read.end();
 std::vector<Fr>weights;for(std::size_t i=0;i<s.linear.size();++i)weights.push_back(io.coin(s.role+"/rho/"+std::to_string(i)));
 G1 residual;residual.clear();Fr blind=0;
 for(std::size_t i=0;i<s.linear.size();++i)for(std::size_t j=0;j<s.handles.size();++j){auto a=weights[i]*s.linear[i][j];residual+=s.handles[j].point*a;if(io.prover())blind+=w->at(j).blind*a;}
 Fr mask=0;Writer compressed;if(io.prover()){mask=rng->scalar();compressed.point(parameters(1).hiding*mask);}
 bytes=io.message(s.role+"/linear-mask-B",compressed.b);Reader rb{bytes};auto B=rb.point();rb.end();auto c=io.coin(s.role+"/shared-c");
 Writer response;if(io.prover()){
  for(std::size_t i=0;i<s.handles.size();++i){response.scalar(masks[i][0]+c*w->at(i).value);response.scalar(masks[i][1]+c*w->at(i).blind);}
  response.scalar(mask+c*blind);
  for(std::size_t i=0;i<s.products.size();++i){auto idx=s.products[i];auto&b=mulmasks[i];auto&X=w->at(idx[0]);auto&Y=w->at(idx[1]);auto&Z=w->at(idx[2]);response.scalar(b[0]+c*X.value);response.scalar(b[1]+c*X.blind);response.scalar(b[2]+c*Y.value);response.scalar(b[3]+c*Y.blind);response.scalar(b[4]+c*(Z.blind-X.blind*Y.value));}
 }
 bytes=io.message(s.role+"/responses",response.b);Reader rr{bytes};
 for(std::size_t i=0;i<s.handles.size();++i){auto x=rr.scalar(),r=rr.scalar();need(scalarCommit(x,r)==A[i]+s.handles[i].point*c,"R7 Sigma representation equation failed");}
 auto z=rr.scalar();need(parameters(1).hiding*z==B+residual*c,"R7 Sigma compressed linear relation failed");
 for(std::size_t i=0;i<s.products.size();++i){auto idx=s.products[i];std::array<Fr,5>v;for(auto&x:v)x=rr.scalar();need(M[i][0]+s.handles[idx[0]].point*c==scalarCommit(v[0],v[1]),"R7 product Sigma X equation failed");need(M[i][1]+s.handles[idx[1]].point*c==scalarCommit(v[2],v[3]),"R7 product Sigma Y equation failed");need(M[i][2]+s.handles[idx[2]].point*c==s.handles[idx[0]].point*v[2]+parameters(1).hiding*v[4],"R7 product Sigma multiplication equation failed");}
 rr.end();
}
}

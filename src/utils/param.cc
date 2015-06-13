#include <glog/logging.h>
#include <cmath>
#include <chrono>
#include <random>
#include "utils/param.h"
#include "mshadow/tensor.h"
#include "utils/singleton.h"
using namespace mshadow;
using std::vector;
using std::string;
namespace singa {

Msg* Param::GenPutMsg(void* arg){
  char buf[128];
  sprintf(buf, "%d %f %f", size(),
      learning_rate_multiplier(), weight_decay_multiplier());
  Msg* msg=new Msg();
  msg->set_type(kPut);
  int v=version();
  if(arg!=nullptr)
    v=*(int*)arg;
  msg->set_target(owner(), v);
  msg->add_frame(buf, strlen(buf));
  msg->add_frame(mutable_cpu_data(), size()*sizeof(float));
	return msg;
}

Msg* Param::GenGetMsg(void* arg){
  Msg* msg=new Msg();
  msg->set_type(kGet);
  int v=version();
  if(arg!=nullptr)
    v=*(int*)arg;
  msg->set_target(owner(), v);
  return msg;
}

Msg* Param::GenUpdateMsg(void* arg){
  Msg* msg=new Msg();
  msg->set_type(kUpdate);
  int v=version();
  if(arg!=nullptr)
    v=*(int*)arg;
  msg->set_target(owner(), v);
  msg->add_frame(mutable_cpu_grad(), size()*sizeof(float));
  return msg;
}

Msg* Param::GenSyncMsg(void* arg){
  return nullptr;
}

Msg* Param::HandlePutMsg(Msg** msg){
  int size;
  float lr, wc;
  sscanf(static_cast<char*>((*msg)->frame_data()), "%d %f %f",
      &size, &lr, &wc);
  proto_.set_learning_rate_multiplier(lr);
  proto_.set_weight_decay_multiplier(wc);
  CHECK((*msg)->next_frame());
  vector<int> shape{size};
  data_=std::make_shared<Blob<float>>(shape);
  data_->set_version((*msg)->target_second());
  grad_.Reshape(shape);
  history_.Reshape(shape);
  CHECK_EQ(size* sizeof(float), (*msg)->frame_size());
  memcpy(mutable_cpu_data(), (*msg)->frame_data(), size*sizeof(float));
  delete (*msg);
  *msg=nullptr;
  return nullptr;
}

Msg* Param::HandleGetMsg(Msg** msg){
  if((*msg)->target_second()<=version()){
    (*msg)->add_frame(mutable_cpu_data(), sizeof(float)*size());
    (*msg)->SwapAddr();
    (*msg)->set_type(kRGet);
  }
  return *msg;
}

int Param::ParseUpdateMsg(Msg** msg){
  CHECK((*msg)->frame_size());
  memcpy(mutable_cpu_grad(), (*msg)->frame_data(),(*msg)->frame_size());
  delete (*msg);
  *msg=nullptr;
  return 1;
}

Msg* Param::GenUpdateResponseMsg(void* arg){
  Msg* msg=new Msg();
  msg->set_type(kRUpdate);
  int v=version();
  if(arg!=nullptr)
    v=*(int*)arg;
  msg->set_target(owner(), v);
  msg->add_frame(mutable_cpu_data(), size()*sizeof(float));
  return msg;
}

Msg* Param::HandleSyncMsg(Msg** msg){
  delete *msg;
  *msg=nullptr;
  return nullptr;
}

int Param::ParseSyncResponseMsg(Msg** msg){
  delete *msg;
  *msg=nullptr;
  return 1;
}
int Param::ParsePutResponseMsg(Msg **msg){
  return ParseSyncResponseMsg(msg);
}
int Param::ParseGetResponseMsg(Msg **msg){
  CHECK((*msg)->frame_size());
  memcpy(mutable_cpu_data(), (*msg)->frame_data(), (*msg)->frame_size());
  // must be set after all other settings are done!
  set_version((*msg)->target_second());
  delete *msg;
  *msg=nullptr;
  return 1;
}
int Param::ParseUpdateResponseMsg(Msg **msg){
  return ParseGetResponseMsg(msg);
}

void Param::Setup(const ParamProto& proto, const vector<int>& shape,
    int fan_in){
  data_=std::make_shared<Blob<float>>(shape);
  grad_.Reshape(shape);
  history_.Reshape(shape);
  proto_=proto;
  fan_in_=fan_in;
}

void Param::Setup(const ParamProto& proto){
  CHECK(proto.has_data());
  // get proto
  proto_=proto;
  // get shape
  vector<int> shape;
  const BlobProto& blob = proto_.data();
  for (int i = 0; i < blob.shape_size(); ++i){
    shape.push_back(blob.shape(i));
  }
  data_=std::make_shared<Blob<float>>(shape);
  // set data
  data_->FromProto(blob);
  // clear used data
  proto_.clear_data();
  //fan_in_=fan_in;
  grad_.Reshape(shape);
  history_.Reshape(shape);
}

void Param::Init(int v){
  Tensor<cpu, 1> data(mutable_cpu_data(), Shape1(size()));
  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  auto random=ASingleton<Random<cpu>>::Instance(seed);
  switch (proto_.init_method()) {
  case ParamProto::kConstant:
    data=proto_.value();
    break;
  case ParamProto::kUniform:
    random->SampleUniform(data, proto_.low(), proto_.high());
    if(proto_.value())
      data*= proto_.value();
    break;
  case ParamProto::kUniformSqrtFanIn:
    CHECK_GT(fan_in_,0);
    random->SampleUniform(data, proto_.low(), proto_.high());
    if(proto_.value())
      data*= proto_.value()/ sqrt(fan_in_ / 3.0f);
    break;
  case ParamProto::kUniformSqrtFanInOut:
    random->SampleUniform(data, proto_.low(), proto_.high());
    if(proto_.value())
      data*= proto_.value()/ sqrt(data_->shape()[0] +data_->shape()[1]);
    break;
  case ParamProto::kGaussian:
    random->SampleGaussian(data, proto_.mean(), proto_.std());
    if(proto_.value())
      data*= proto_.value();
    break;
  case ParamProto::kGaussainSqrtFanIn:
    random->SampleGaussian(data, proto_.mean(), proto_.std());
    if(proto_.value())
      data*= proto_.value()/ sqrt(data_->shape()[0]);
    break;
  default:
    LOG(ERROR) << "Illegal parameter init method ";
    break;
  }
  set_version(v);
}

void Param::ToProto(ParamProto* proto) const{
  //copy configuration
  *proto = proto_;
  //copy data
  data_->ToProto(proto->mutable_data());
}

/**************************RandomSyncParam********************************
const vector<int> RandomSyncParam::RandomSample(int seed, int m, int n){
  vector<int> samples(m);
  std::mt19937 gen(seed);
  std::uniform_real_distribution<float> dist(0.f,1.f);
  for(int i=0,k=0;i<n&&k<m;i++)
    if((m-k)*1.0f/(n-i)>dist(gen)){
      samples[k++]=i;
    }
  return samples;
}

zmsg_t* RandomSyncParam::HandleSyncMsg(zmsg_t** msg){
  int64_t start=zclock_mono();
  char* control=zframe_strdup(zmsg_first(*msg));
  int seed, count;
  sscanf(control, "%d-%d", &seed,&count);
  delete control;
  zframe_t* syncframe=zmsg_next(*msg);
  CHECK_EQ(zframe_size(syncframe), count*sizeof(float));
  float* syncptr=(float*)zframe_data(syncframe);
  float* dptr=data_.mutable_cpu_data();
  int k=0;
  if(count==data_.count()){
    for(int idx=0;idx<count;idx++){
      float x=dptr[idx];
      dptr[idx]+=syncptr[k];
      syncptr[k]=x;
      k++;
    }
  }else{
    for(int idx: RandomSample(seed, count, data_.count())){
      float x=dptr[idx];
      dptr[idx]+=syncptr[k];
      syncptr[k]=x;
      k++;
    }
  }
  CHECK_EQ(k,count);
  CHECK_EQ(zframe_size(syncframe), count*sizeof(float));
  return *msg;
}

zmsg_t *RandomSyncParam::GenSyncMsgFromWorker(float sample_ratio){
  int64_t start=zclock_mono();
  zmsg_t* msg=zmsg_new();
  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  int m=data_.count()*sample_ratio;
  zmsg_addstrf(msg, "%u-%d", seed, m);
  float* updateptr=new float[m];
  float* dptr=data_.mutable_cpu_data();
  float* sdptr=snapshot_.mutable_cpu_data();
  int k=0;
  if(m==data_.count()){
    for(int idx=0;idx<m;idx++)
      updateptr[k++]=dptr[idx]-sdptr[idx];
  }else{
    const vector<int> samples=RandomSample(seed, m, data_.count());
    for(int idx:samples){
      updateptr[k++]=dptr[idx]-sdptr[idx];
    }
  }
  CHECK_EQ(k,m);
  zframe_t* frame=zframe_new(updateptr, sizeof(float)*m);
  zmsg_append(msg, &frame);
  delete updateptr;
  worker_gen_sync+=zclock_mono()-start;
  return msg;
}

void RandomSyncParam::ParseSyncMsgFromPS(zmsg_t** msg){
  int64_t start=zclock_mono();
  //LOG(ERROR)<<"worker sync "<<id();
  char* control=zmsg_popstr(*msg);
  int seed, count;
  sscanf(control, "%u-%d", &seed, &count);
  //LOG(ERROR)<<"worker sync "<<id()<<" "<<control;
  delete control;
  zframe_t* psdataframe=zmsg_pop(*msg);
  CHECK_EQ(zframe_size(psdataframe), count*sizeof(float));
  float* psdptr=(float*)zframe_data(psdataframe);
  float* dptr=data_.mutable_cpu_data();
  float* sdptr=snapshot_.mutable_cpu_data();
  int k=0;
  if(count==data_.count()){
    for(int idx=0;idx<count;idx++){
      dptr[idx]+=psdptr[k++]-sdptr[idx];
      sdptr[idx]=dptr[idx];
    }
  }else{
    for(int idx: RandomSample(seed, count, data_.count())){
      dptr[idx]+=psdptr[k++]-sdptr[idx];
      sdptr[idx]=dptr[idx];
    }
  }
  zframe_destroy(&psdataframe);
  worker_handle_sync+=zclock_mono()-start;
  zmsg_destroy(msg);
}


void RandomSyncParam::Setup(const ParamProto& proto, const vector<int>& shape,
    int fan_in){
  Param::Setup(proto, shape, fan_in);
  snapshot_.Reshape(shape);
}

void RandomSyncParam::Init(){
  Param::Init();
  memcpy(snapshot_.mutable_cpu_data(), data_.mutable_cpu_data(),
      sizeof(float)*data_.count());
}
*/

/***************************ElasticParam************************************
zmsg_t* ElasticParam::HandleSyncMsg(zmsg_t** msg){
  int64_t start=zclock_mono();
  char* control=zframe_strdup(zmsg_first(*msg));
  float alpha;int count;
  sscanf(control, "%f-%d", &alpha,&count);
  delete control;
  zframe_t* syncframe=zmsg_next(*msg);
  CHECK_EQ(size(), count);
  Tensor<cpu, 1> server(data_.mutable_cpu_data(), Shape1(count));
  Tensor<cpu, 1> worker((float*)zframe_data(syncframe), Shape1(count));
  worker=(worker-server)*alpha;
  server+=worker;
  return *msg;
}

zmsg_t *ElasticParam::GenSyncMsgFromWorker(float alpha){
  int64_t start=zclock_mono();
  zmsg_t* msg=zmsg_new();
  zmsg_addstrf(msg, "%f-%d", alpha, size());
  zmsg_addmem(msg, mutable_cpu_data(), sizeof(float)*size());
  worker_gen_sync+=zclock_mono()-start;
  return msg;
}

void ElasticParam::ParseSyncMsgFromPS(zmsg_t** msg){
  int64_t start=zclock_mono();
  //LOG(ERROR)<<"worker sync "<<id();
  char* control=zmsg_popstr(*msg);
  float alpha;int count;
  sscanf(control, "%f-%d", &alpha, &count);
  delete control;
  zframe_t* frame=zmsg_pop(*msg);
  CHECK_EQ(zframe_size(frame), count*sizeof(float));
  Tensor<cpu, 1> diff((float*)zframe_data(frame), Shape1(count));
  Tensor<cpu, 1> data(mutable_cpu_data(), Shape1(count));
  data-=diff;
  zframe_destroy(&frame);
  zmsg_destroy(msg);
  worker_handle_sync+=zclock_mono()-start;
}
*/
}  // namespace singa

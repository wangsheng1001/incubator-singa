#ifndef INCLUDE_UTILS_PARAM_H_
#define INCLUDE_UTILS_PARAM_H_
#include <vector>
#include <string>
#include <map>
#include <functional>
#include "proto/model.pb.h"
#include "utils/blob.h"
#include "communication/msg.h"
// Base paramter class.
namespace singa {
class Param {
 public:
  Param():data_(nullptr){}
  virtual ~Param(){};
  

  virtual Msg* GenGetMsg(void* arg=nullptr);
  virtual Msg* GenPutMsg(void* arg=nullptr);
  virtual Msg* GenUpdateMsg(void* arg=nullptr);
  virtual Msg* GenSyncMsg(void* arg=nullptr);

  virtual Msg* HandleGetMsg(Msg** msg);
  virtual Msg* HandlePutMsg(Msg** msg);
  virtual int ParseUpdateMsg(Msg** msg);
  virtual Msg* GenUpdateResponseMsg(void* arg=nullptr);
  virtual Msg* HandleSyncMsg(Msg** msg);

  virtual int ParseGetResponseMsg(Msg** msg);
  virtual int ParsePutResponseMsg(Msg** msg);
  virtual int ParseUpdateResponseMsg(Msg** msg);
  virtual int ParseSyncResponseMsg(Msg** msg);

  /**
   * setup param shape
   */
  virtual void Setup(const ParamProto& proto, const std::vector<int>& shape, int fan_in);
  /**
   * setup used during restore, the proto should contains shape and data as well;
   */
  virtual void Setup(const ParamProto& proto);
  /*
   * fill the data according to initmethod, i.e., random/gaussian/fixed value
   */
  virtual void Init(int v=0);
  /*
   * copy to a ParamProto object which will be dumped to disk when checkpointing
   */
  virtual void ToProto(ParamProto* proto) const;
  
  void ShareData(shared_ptr<Param> other){
    proto_.set_owner(other->owner());
    if(data_!=nullptr)
      CHECK(std::equal(data_->shape().begin(), data_->shape().end(),
          other->data_->shape().begin()));
    data_=other->data_;
  }
  float learning_rate_multiplier() {
    return proto_.learning_rate_multiplier();
  }
  float weight_decay_multiplier() {
    return proto_.weight_decay_multiplier();
  }
  /*
  const int split_threshold(){
    return proto_.split_threshold();
  }
  */
  const std::string& name() {
    return proto_.name();
  }
  /**
   * if the Param shares data with others, then owner is the id of that param.
   * otherwise it is itself's id.
   */
  const int owner() const{
    return proto_.owner();
  }
  int id() const{
    return proto_.id();
  }
  void set_id(int id){
    proto_.set_id(id);
    proto_.set_owner(id);
  }

  int version() const {
    return data_->version(); // TODO store version in data blob
  }
  void set_version(int v) {
    data_->set_version(v); // TODO read version from data blob
  }
   /**
    * @return num of floats.
    */
  int size() const {
    return data_->count();
  }
  /**
   * Return const mem address for the content of this parameter
   */
  const Blob<float> &data() {
    return *data_;
  }
  Blob<float> *mutable_data() {
    return data_.get();
  }
  /**
   * Return gradient of this parameter
   */
  const Blob<float> &grad() {
    return grad_;
  }
  Blob<float> *mutable_grad() {
    return &grad_;
  }

  const Blob<float> &history() {
    return history_;
  }
  Blob<float> *mutable_history() {
    return &history_;
  }

  float* mutable_cpu_data(){
    return data_->mutable_cpu_data();
  }
  float* mutable_cpu_grad(){
    return grad_.mutable_cpu_data();
  }
  float* mutable_cpu_history(){
    return history_.mutable_cpu_data();
  }
 protected:
  /**
   * name of the parameter used to share wights between neuralnets
   */
  std::string name_;
  shared_ptr<Blob<float>> data_;
  //! gradient, history gradient of this parameter
  Blob<float> grad_, history_;
  ParamProto proto_;
  int fan_in_;
};
/**
 * Sync with server by randomly sampling some parameters for every sync.
class RandomSyncParam: public Param{
 public:
  virtual zmsg_t* HandleSyncMsg(zmsg_t** msg);
  virtual zmsg_t *GenSyncMsgFromWorker(float sample_ratio);
  virtual void ParseSyncMsgFromPS(zmsg_t** msg);
  virtual void Setup(const ParamProto& proto, const vector<int>& shape, int fan_in);
  virtual void Init();

  float* mutable_cpu_snapshot(){
    return snapshot_.mutable_cpu_data();
  }
  const float* cpu_snapshot(){
    return snapshot_.cpu_data();
  }

 protected:
  const vector<int> RandomSample(int seed, int m, int n);


  Blob<float> snapshot_;
};
 */
/**
 * Sync with server by elastic SGD see http://arxiv.org/abs/1412.6651.
class ElasticParam: public Param{
 public:
  virtual zmsg_t* HandleSyncMsg(zmsg_t** msg);
  virtual zmsg_t *GenSyncMsgFromWorker(float moving_rate);
  virtual void ParseSyncMsgFromPS(zmsg_t** msg);
};
 */


}  // namespace singa

#endif  // INCLUDE_UTILS_PARAM_H_

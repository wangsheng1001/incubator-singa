#include <fstream>
#include "gtest/gtest.h"
#include "utils/common.h"
#include "utils/factory.h"
#include "utils/singleton.h"
#include "proto/model.pb.h"
#include "trainer/server.h"

using namespace singa;


TEST(CheckpointTest, Checkpoint){

  // register param
  Singleton<Factory<singa::Param>>::Instance()->Register(
      "Param", CreateInstance(singa::Param, singa::Param));
  
  // create param object
  Factory<Param>* factory = Singleton<Factory<Param>>::Instance();
  std::map<int,shared_ptr<Param>> data;
  ParamProto proto;
  shared_ptr<Param> p;

  for (int i = 0; i < 10; ++i){
    p = shared_ptr<Param>(factory->Create("Param"));
    p->Setup(proto, std::vector<int>{10}, 0);
    p->Init(3);
    p->set_id(i);
    data[i] = p;
  }

  // do checkpoint
  Server::Checkpoint("/tmp/singa/test.cp", data);

  std::map<int,shared_ptr<Param>> new_data;
  // restore
  Server::Restore("/tmp/singa/test.cp", new_data);

  ASSERT_EQ(new_data.size(), data.size());

  for (int i = 0; i < 10; ++i){
    ASSERT_EQ(data[i]->size(), new_data[i]->size());
    int size = data[i]->size();
    for (int j = 0; j < size; ++j){
      ASSERT_EQ(data[i]->data().cpu_data()[j], new_data[i]->data().cpu_data()[j]);
    }
  }

}


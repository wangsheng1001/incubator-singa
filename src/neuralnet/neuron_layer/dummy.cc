/************************************************************
*
* Licensed to the Apache Software Foundation (ASF) under one
* or more contributor license agreements.  See the NOTICE file
* distributed with this work for additional information
* regarding copyright ownership.  The ASF licenses this file
* to you under the Apache License, Version 2.0 (the
* "License"); you may not use this file except in compliance
* with the License.  You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an
* "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
* KIND, either express or implied.  See the License for the
* specific language governing permissions and limitations
* under the License.
*
*************************************************************/

#include "singa/neuralnet/neuron_layer/dummy.h"
#include <glog/logging.h>

namespace singa {

void DummyLayer::Setup(const LayerProto& proto,
                       const vector<Layer*>& srclayers) {
  Layer::Setup(proto, srclayers);
  if (proto.dummy_conf().input()) {  // use as input layer
    CHECK_EQ(srclayers.size(), 0);
    input_ = true;
    vector<int> shape;
    for (int s : proto.dummy_conf().shape()) shape.push_back(s);
    data_.Reshape(shape);
    grad_.ReshapeLike(data_);
  } else {
    CHECK_EQ(srclayers.size(), 1);
    data_.ReshapeLike(srclayers[0]->data(this));
    grad_.ReshapeLike(srclayers[0]->grad(this));
  }
  if (proto.dummy_conf().output()) {  // use as output layer
    output_ = true;
  }
}

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> dis(0, 1);

void DummyLayer::ComputeFeature(int flag, const vector<Layer*>& srclayers) {
  if (input_) {
    // randomly init data with [0,1] values
    for (int i = 0; i < data_.count(); ++i)
      data_.mutable_cpu_data()[i] = dis(gen);
  }
  if (srclayers.size() > 0)
    data_.CopyFrom(srclayers[0]->data(this));
}

void DummyLayer::ComputeGradient(int flag, const vector<Layer*>& srclayers) {
  if (output_) {
    // randomly init data with [0,1] values
    for (int i = 0; i < data_.count(); ++i)
      grad_.mutable_cpu_data()[i] = dis(gen);
  }
  if (srclayers.size() > 0) {
    srclayers[0]->mutable_grad(this)->CopyFrom(grad_);
  }
}

} // namespace singa

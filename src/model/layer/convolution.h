/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef SRC_MODEL_LAYER_CONVOLUTION_H_
#define SRC_MODEL_LAYER_CONVOLUTION_H_
#include <string>
#include <utility>
#include <vector>
#include <stack>
#include "singa/model/layer.h"

namespace singa {
class Convolution : public Layer {
 public:
  /// \copydoc Layer::layer_type()
  const std::string layer_type() const override { return "Convolution"; }

  /// \copydoc Layer::Setup(const LayerConf&);
  void Setup(const LayerConf &conf) override;

  // void SetupParam(const Tensor &input);
  /// \copydoc Layer::Forward(int flag, const Tensor&)
  const Tensor Forward(int flag, const Tensor &input) override;

  /// \copydoc Layer::Backward(int, const Tensor&, const Tensor&);
  const std::pair<Tensor, vector<Tensor>> Backward(int flag,
                                                   const Tensor &grad) override;

  size_t kernel_w() const { return kernel_w_; }
  size_t kernel_h() const { return kernel_h_; }
  size_t pad_w() const { return pad_w_; }
  size_t pad_h() const { return pad_h_; }
  size_t stride_w() const { return stride_w_; }
  size_t stride_h() const { return stride_h_; }
  size_t num_filters() const { return num_filters_; }
  size_t batchsize() const { return batchsize_; }
  size_t channels() const { return channels_; }
  size_t height() const { return height_; }
  size_t width() const { return width_; }
  bool bias_term() const { return bias_term_; }
  const Tensor &weight() const { return weight_; }
  const Tensor &bias() const { return bias_; }

  void set_weight(Tensor w) {
    weight_.ResetLike(w);
    weight_.CopyData(w);
  }
  void set_bias(Tensor b) {
    bias_.ResetLike(b);
    bias_.CopyData(b);
  }

 protected:
  size_t kernel_w_, pad_w_, stride_w_;
  size_t kernel_h_, pad_h_, stride_h_;
  size_t batchsize_, channels_, height_, width_;
  size_t col_height_, col_width_, conv_height_, conv_width_, num_filters_;
  Tensor weight_, bias_;
  // store intermediate data, i.e., input tensor
  std::stack<Tensor> buf_;
  bool bias_term_;
};
}  // namespace singa
#endif  // SRC_MODEL_LAYER_CONVOLUTION_H_
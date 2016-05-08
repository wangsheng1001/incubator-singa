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

#include "singa/core/tensor.h"
#include "singa/core/math.h"

namespace singa {
Tensor::~Tensor() {
  if (blob_ != nullptr) {
    if (blob_->DecRefCount() == 0)
      device_->FreeBlob(blob_);
    blob_ = nullptr;
  }
}

Tensor::Tensor(const Shape& shape, DataType dtype)
    : data_type_(dtype), device_(&hostDeviceSingleton), shape_(shape) {
  blob_ = device_->NewBlob(Product(shape) * SizeOf(dtype));
}

Tensor::Tensor(const Shape& shape, Device* device, DataType dtype)
    : data_type_(dtype), device_(device), shape_(shape) {
  blob_ = device_->NewBlob(Product(shape) * SizeOf(dtype));
}

Tensor::Tensor(const Tensor& t)
    : transpose_(t.transpose_),
      data_type_(t.data_type_),
      device_(t.device_),
      blob_(t.blob_),
      shape_(t.shape_) {
  blob_->IncRefCount();
}

Tensor::Tensor(Tensor&& t)
    : transpose_(t.transpose_),
      data_type_(t.data_type_),
      device_(t.device_),
      blob_(t.blob_),
      shape_(t.shape_) {
  t.blob_ = nullptr;
}

void Tensor::ToDevice(Device* dst) {
  if (device_ != dst) {
    Tensor tmp(shape_, dst);
    tmp.CopyData(*this);
    if (blob_->DecRefCount() <= 0)
      device_->FreeBlob(blob_);
    blob_ = tmp.blob_;
    tmp.blob_ = nullptr;
  }
}

void Tensor::ToHost() {
  ToDevice(&hostDeviceSingleton);
}

void Tensor::CopyDataFromHostPtr(const void* src, size_t size) {
  device_->CopyDataFromHostPtr(blob_, src, size);
}

void Tensor::CopyData(const Tensor& src) {
  CHECK_EQ(Size(), src.Size());
  MyCopyData(this, src, Size() * SizeOf(data_type_), 0, 0);
}

Tensor Tensor::Clone() {
  Tensor t(shape_, device_, data_type_);
  t.CopyData(*this);
  return t;
}

Tensor Tensor::T() const {
  Tensor t(*this);
  t.transpose_ = ~transpose_;
  return t;
}

void Tensor::operator=(const Tensor& t) {
  transpose_ = t.transpose_;
  data_type_ = t.data_type_;
  shape_ = t.shape_;
  device_ = t.device_;
  if (blob_->DecRefCount() == 0)
    device_->FreeBlob(blob_);
  blob_ = t.blob_;
  blob_->IncRefCount();
}

void Tensor::operator=(Tensor&& t) {
  transpose_ = t.transpose_;
  shape_ = t.shape_;
  device_ = t.device_;
  if (blob_->DecRefCount() == 0)
    device_->FreeBlob(blob_);
  blob_ = t.blob_;
  t.blob_ = nullptr;
}


void Tensor::operator+=(const Tensor& t) {
  Add(*this, t, this);
}
//====================Tensor Operations=======================================

void MyCopyData(Tensor* dst,
              const Tensor& src,
              int len,
              int dst_offset,
              int src_offset) {
  CHECK_GE(src.Size(), src_offset + len);
  CHECK_GE(dst->Size(), dst_offset + len);
  Device* src_dev = src.device(), *dst_dev = dst->device();
  Blob* src_blob = src.blob(), *dst_blob = dst->blob();
  if (dst_dev->device_lib() != src_dev->device_lib()) {
    // let the none cpp device conduct copy op
    if (dst_dev->device_lib() == kCpp) {
      src_dev->CopyData(dst_blob, *src_blob, len, dst_offset, src_offset);
    } else if (src_dev->device_lib() == kCpp) {
      dst_dev->CopyData(dst_blob, *src_blob, len, dst_offset, src_offset);
    } else {
      // LOG(FATAL) << "Not support mem copy betwee Cuda and OpenCL device";
    }
  } else {
    src_dev->CopyData(dst_blob, *src_blob, len, dst_offset, src_offset);
  }
}

Tensor operator+(const Tensor& lhs, const Tensor& rhs) {
  Tensor ret(lhs.shape(), lhs.device());
  Add(lhs, rhs, &ret);
  return ret;
}

void Add(const Tensor& lhs,const Tensor& rhs, Tensor *ret) {
  TYPE_LIB_SWITCH(lhs.data_type(), DType, lhs.device()->device_lib(), Lib, {
      ret->device()->Submit(
        [lhs, rhs, ret](Context* ctx) {
          math::Add<DType, Lib>(lhs.Size(),
            lhs.blob(),
            rhs.blob(),
            ret->blob(),
            ctx);},
        {lhs.blob(), rhs.blob()}, {ret->blob()});
      }
      );
}
/*
Tensor operator-(const Tensor& lhs, const Tensor& rhs) {
  Tensor ret(lhs.shape(), lhs.device());
  Sub(lhs, rhs, &ret);
  return ret;
}

void Sub(const Tensor& lhs, const Tensor& rhs, Tensor *ret) {
  TYPE_LIB_SWITCH(lhs.data_type(), DType, lhs.device()->device_lib(), Lib, {
      ret->device()->Submit(
        [lhs, rhs, ret](Context* ctx) {
          math::Sub<DType, Lib>(
            lhs.Size(),
            lhs.blob(),
            rhs.blob(),
            ret->blob(),
            ctx);}
        , {lhs.blob(), rhs.blob()}, {ret->blob()});
      });
}
*/

//================Blas operations============================================

//================Neural Net operations======================================

void Conv(const math::OpConf* conf,
            const Tensor& input,
            const Tensor& W,
            const Tensor &b,
            Tensor* ret) {
  TYPE_LIB_SWITCH(input.data_type(), DType, input.device()->nn_lib(), Lib, {
      ret->device()->Submit(
        [conf, input, W, b, ret](Context *ctx) {
          math::Conv<DType, Lib>(
            conf,
            input.blob(),
            W.blob(),
            b.blob(),
            ret->blob(),
            ctx);}
        , {input.blob(), W.blob(), b.blob()}, {ret->blob()});
      });
}

}  /* singa */

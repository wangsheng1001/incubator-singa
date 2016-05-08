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

#include "singa/core/math.h"
#include "singa/core/common.h"

#ifdef USE_CBLAS
#include <cblas.h>
#endif

namespace singa {
namespace math {
template<>
void Add<float, lib::Cpp>(int count,
                     const Blob* lhs,
                     const Blob* rhs,
                     Blob* ret,
                     Context* ctx) {
  // CHECK_EQ(ctx->stream, nullptr);
  float *dptr = static_cast<float*>(ret->mutable_data());
  const float *lptr = static_cast<const float*>(lhs->data());
  const float *rptr = static_cast<const float*>(rhs->data());
  for (int i = 0; i < count; i++) {
    dptr[i] = lptr[i] + rptr[i];
  }
}

#ifdef USE_CBLAS
template<>
void Dot<float, lib::Cpp>(int count,
                     const Blob* lhs,
                     const Blob* rhs,
                     float* ret,
                     Context* ctx) {
  float dptr = ret->mutable_data(), lptr = lhs->data(), rptr = rhs->data();
  *ret = cblas_sdot(count, lptr, 1, rptr, 1);
}

#endif
}
}

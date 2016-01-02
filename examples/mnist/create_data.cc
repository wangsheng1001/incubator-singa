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

//
// This code creates DataShard for MNIST dataset.
// It is adapted from the convert_mnist_data from Caffe
//
// Usage:
//    create_shard.bin input_image_file input_label_file output_folder
// The MNIST dataset could be downloaded at
//    http://yann.lecun.com/exdb/mnist/

#include <glog/logging.h>
#include <cstdint>
#include <iostream>

#include <fstream>
#include <string>

#include "singa/io/store.h"
#include "singa/utils/common.h"
#include "singa/proto/common.pb.h"

using std::string;

uint32_t swap_endian(uint32_t val) {
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | (val >> 16);
}

// output is the full path, unlike create_data in CIFAR with only
// specifies the directory
void create_data(const char* image_filename, const char* label_filename,
        const char* output) {
  // Open files
  std::ifstream image_file(image_filename, std::ios::in | std::ios::binary);
  std::ifstream label_file(label_filename, std::ios::in | std::ios::binary);
  CHECK(image_file) << "Unable to open file " << image_filename;
  CHECK(label_file) << "Unable to open file " << label_filename;
  // Read the magic and the meta data
  uint32_t magic;
  uint32_t num_items;
  uint32_t num_labels;
  uint32_t rows;
  uint32_t cols;

  image_file.read(reinterpret_cast<char*>(&magic), 4);
  magic = swap_endian(magic);
  CHECK_EQ(magic, 2051) << "Incorrect image file magic.";
  label_file.read(reinterpret_cast<char*>(&magic), 4);
  magic = swap_endian(magic);
  CHECK_EQ(magic, 2049) << "Incorrect label file magic.";
  image_file.read(reinterpret_cast<char*>(&num_items), 4);
  num_items = swap_endian(num_items);
  label_file.read(reinterpret_cast<char*>(&num_labels), 4);
  num_labels = swap_endian(num_labels);
  CHECK_EQ(num_items, num_labels);
  image_file.read(reinterpret_cast<char*>(&rows), 4);
  rows = swap_endian(rows);
  image_file.read(reinterpret_cast<char*>(&cols), 4);
  cols = swap_endian(cols);

  // read backend from the job.conf
  string store_backend = (output_folder.find("hdfs")!=-1) ? "hdfsfile" : "kvfile";  
  auto store = singa::io::CreateStore(store_backend);
  char label;
  char* pixels = new char[rows * cols];
  int count = 0;
  const int kMaxKeyLength = 10;
  char key[kMaxKeyLength];
  string value;

  singa::RecordProto image;
  image.add_shape(rows);
  image.add_shape(cols);
  LOG(INFO) << "A total of " << num_items << " items.";
  LOG(INFO) << "Rows: " << rows << " Cols: " << cols;
  for (int item_id = 0; item_id < num_items; ++item_id) {
    image_file.read(pixels, rows * cols);
    label_file.read(&label, 1);
    image.set_pixel(pixels, rows*cols);
    image.set_label(label);
    snprintf(key, kMaxKeyLength, "%08d", item_id);
    image.SerializeToString(&value);
    store->Write(string(key), value);
  }
  delete pixels;
  store->Flush();
  delete store;
}

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cout << "This program create a DataShard for a MNIST dataset\n"
        "Usage:\n"
        "    create_shard.bin  input_image_file input_label_file"
        " output_db_file\n"
        "The MNIST dataset could be downloaded at\n"
        "    http://yann.lecun.com/exdb/mnist/\n"
        "You should gunzip them after downloading.";
  } else {
    google::InitGoogleLogging(argv[0]);
    create_data(argv[1], argv[2], argv[3]);
  }
  return 0;
}

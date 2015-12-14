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

#include "singa/driver.h"

#include <glog/logging.h>
#include <set>
#include <string>
#include <vector>
#include "singa/neuralnet/layer.h"
#include "singa/utils/common.h"
#include "singa/utils/tinydir.h"
#include "singa/utils/cluster.h"
#include "singa/server.h"
#include "singa/stub.h"
#include "singa/worker.h"

#include "singa/neuralnet/connection_layer/bridge.h"
#include "singa/neuralnet/connection_layer/concate.h"
#include "singa/neuralnet/connection_layer/slice.h"
#include "singa/neuralnet/connection_layer/split.h"
#include "singa/neuralnet/input_layer/deprecated.h"
#include "singa/neuralnet/input_layer/csv.h"
#include "singa/neuralnet/input_layer/image_preprocess.h"
#include "singa/neuralnet/input_layer/prefetch.h"
#include "singa/neuralnet/input_layer/record.h"
#include "singa/neuralnet/input_layer/store.h"
#include "singa/neuralnet/loss_layer/euclidean.h"
#include "singa/neuralnet/loss_layer/softmax.h"
#include "singa/neuralnet/neuron_layer/argsort.h"
#include "singa/neuralnet/neuron_layer/convolution.h"
#include "singa/neuralnet/neuron_layer/dropout.h"
#include "singa/neuralnet/neuron_layer/dummy.h"
#include "singa/neuralnet/neuron_layer/inner_product.h"
#include "singa/neuralnet/neuron_layer/lrn.h"
#include "singa/neuralnet/neuron_layer/pooling.h"
#include "singa/neuralnet/neuron_layer/rbm.h"
#include "singa/neuralnet/neuron_layer/relu.h"
#include "singa/neuralnet/neuron_layer/sigmoid.h"
#include "singa/neuralnet/neuron_layer/stanh.h"
#include "singa/neuralnet/neuron_layer/softmax.h"
#include "singa/neuralnet/output_layer/record.h"
#include "singa/neuralnet/output_layer/csv.h"

extern "C" void openblas_set_num_threads(int num);

namespace singa {

void Driver::Init(int argc, char **argv) {
  // unique job ID generated from singa-run.sh, passed in as "-singa_job <id>"
  int arg_pos = ArgPos(argc, argv, "-singa_job");
  job_id_ = (arg_pos != -1) ? atoi(argv[arg_pos+1]) : -1;
  // global signa conf passed by singa-run.sh as "-singa_conf <path>"
  arg_pos = ArgPos(argc, argv, "-singa_conf");
  if (arg_pos != -1)
    ReadProtoFromTextFile(argv[arg_pos+1], &singa_conf_);
  else
    ReadProtoFromTextFile("conf/singa.conf", &singa_conf_);
  // job conf passed by users as "-conf <path>"
  arg_pos = ArgPos(argc, argv, "-conf");
  CHECK_NE(arg_pos, -1);
  ReadProtoFromTextFile(argv[arg_pos+1], &job_conf_);

  // register layers

  // input and output layers
  RegisterLayer<RecordInputLayer, int>(kRecordInput);
  RegisterLayer<CSVInputLayer, int>(kCSVInput);
  RegisterLayer<ImagePreprocessLayer, int>(kImagePreprocess);
  RegisterLayer<RecordOutputLayer, int>(kRecordOutput);
  RegisterLayer<CSVOutputLayer, int>(kCSVOutput);

  RegisterLayer<BridgeDstLayer, int>(kBridgeDst);
  RegisterLayer<BridgeSrcLayer, int>(kBridgeSrc);

  RegisterLayer<ArgSortLayer, int>(kArgSort);
  RegisterLayer<ConvolutionLayer, int>(kConvolution);
  RegisterLayer<CConvolutionLayer, int>(kCConvolution);
  RegisterLayer<CPoolingLayer, int>(kCPooling);
  RegisterLayer<ConcateLayer, int>(kConcate);
  RegisterLayer<DropoutLayer, int>(kDropout);
  RegisterLayer<DummyLayer, int>(kDummy);
  RegisterLayer<EuclideanLossLayer, int>(kEuclideanLoss);
  RegisterLayer<InnerProductLayer, int>(kInnerProduct);
  RegisterLayer<LabelLayer, int>(kLabel);
  RegisterLayer<LRNLayer, int>(kLRN);
  RegisterLayer<MnistLayer, int>(kMnist);
  RegisterLayer<PrefetchLayer, int>(kPrefetch);
  RegisterLayer<PoolingLayer, int>(kPooling);
  RegisterLayer<RBMHidLayer, int>(kRBMHid);
  RegisterLayer<RBMVisLayer, int>(kRBMVis);
  RegisterLayer<RGBImageLayer, int>(kRGBImage);
  RegisterLayer<ReLULayer, int>(kReLU);
  RegisterLayer<ShardDataLayer, int>(kShardData);
  RegisterLayer<SigmoidLayer, int>(kSigmoid);
  RegisterLayer<SliceLayer, int>(kSlice);
  RegisterLayer<SoftmaxLossLayer, int>(kSoftmaxLoss);
  RegisterLayer<SplitLayer, int>(kSplit);
  RegisterLayer<STanhLayer, int>(kSTanh);
  RegisterLayer<SoftmaxLayer, int>(kSoftmax);

#ifdef USE_LMDB
  RegisterLayer<LMDBDataLayer, int>(kLMDBData);
#endif

  // register updaters
  RegisterUpdater<AdaGradUpdater>(kAdaGrad);
  RegisterUpdater<NesterovUpdater>(kNesterov);
  // TODO(wangwei) RegisterUpdater<kRMSPropUpdater>(kRMSProp);
  RegisterUpdater<SGDUpdater>(kSGD);

  // register learning rate change methods
  RegisterLRGenerator<LRGenerator>(kFixed);
  RegisterLRGenerator<FixedStepLRGen>(kFixedStep);
  RegisterLRGenerator<StepLRGen>(kStep);
  RegisterLRGenerator<LinearLRGen>(kLinear);
  RegisterLRGenerator<ExpLRGen>(kExponential);
  RegisterLRGenerator<InvLRGen>(kInverse);
  RegisterLRGenerator<InvTLRGen>(kInverseT);

  // register workers
  RegisterWorker<BPWorker>(kBP);
  RegisterWorker<CDWorker>(kCD);

  // register params
  RegisterParam<Param>(0);

  // register param init methods
  RegisterParamGenerator<ParamGenerator>(kConstant);
  RegisterParamGenerator<GaussianGen>(kGaussian);
  RegisterParamGenerator<UniformGen>(kUniform);
  RegisterParamGenerator<GaussianSqrtFanInGen>(kGaussianSqrtFanIn);
  RegisterParamGenerator<UniformSqrtFanInGen>(kUniformSqrtFanIn);
  RegisterParamGenerator<UniformSqrtFanInOutGen>(kUniformSqrtFanInOut);
}

void Driver::InitLog(char* arg)
{
    google::InitGoogleLogging(arg);
}

void Driver::Train(bool resume, const std::string str){
  JobProto job_conf;
  job_conf.ParseFromString(str);
  Train(resume,job_conf);

}

void Driver::Train(bool resume, const JobProto& job_conf) {
  Cluster::Setup(job_id_, singa_conf_, job_conf.cluster());
  if (singa_conf_.has_log_dir())
    SetupLog(singa_conf_.log_dir(),
        std::to_string(job_id_) + "-" + job_conf.name());
  tinydir_dir workspace;
  if (tinydir_open(&workspace, job_conf.cluster().workspace().c_str()) == -1)
    LOG(FATAL) << "workspace not exist: " << job_conf.cluster().workspace();
  if (job_conf.num_openblas_threads() != 1)
    LOG(WARNING) << "openblas luanches "
                 << job_conf.num_openblas_threads() << " threads";
  openblas_set_num_threads(job_conf.num_openblas_threads());

  JobProto job;
  job.CopyFrom(job_conf);
  if (resume)
    SetupForResume(&job);
  job.set_id(job_id_);
  Train(job);
}

void Driver::Test(const JobProto& job_conf) {
  Cluster::Setup(job_id_, singa_conf_, job_conf.cluster());
  Cluster::Get()->Register(getpid(), "localhost");
  // TODO(wangwei) extend to a group with multiple workers
  auto worker = Worker::Create(job_conf.train_one_batch());
  worker->Setup(0, 0, job_conf, nullptr, nullptr, nullptr);
  auto net = NeuralNet::Create(job_conf.neuralnet(), kTest, 1);
  vector<string> paths;
  for (const auto& p : job_conf.checkpoint_path())
    paths.push_back(p);
  net->Load(paths);
  worker->Test(job_conf.test_steps(), kTest,  net);
}

void Driver::Train(const JobProto& job_conf) {
  auto cluster = Cluster::Get();
  int nserver_grps = cluster->nserver_groups();
  int grp_size = cluster->nworkers_per_group();
  Stub stub;
  // no need to create Stub if there is only a single worker without servers,
  // i.e., the training will be conducted by the single worker.
  if (grp_size > 1 || nserver_grps > 0) {
    stub.Setup();
    // TODO(wangwei)  register endpoint to zookeeper if > 1 procs;
    cluster->Register(getpid(), stub.endpoint());  // getpid() is from unistd.h
  }

  NeuralNet* net = NeuralNet::Create(job_conf.neuralnet(), kTrain, grp_size);
  const vector<Worker*> workers = CreateWorkers(job_conf, net);
  const vector<Server*> servers = CreateServers(job_conf, net);

#ifdef USE_MPI
  int nthreads = workers.size() + servers.size() + 1;
  for (int i = 0; i < nthreads; i++)
    MPIQueues.push_back(make_shared<SafeQueue>());
#endif

  vector<std::thread> threads;
  for (auto server : servers)
    threads.push_back(std::thread(&Server::Run, server));
  for (auto worker : workers)
    threads.push_back(std::thread(&Worker::Run, worker));
  if (grp_size > 1 || nserver_grps > 0) {
    int nservers_per_grp = cluster->nservers_per_group();
    int lcm = LeastCommonMultiple(nservers_per_grp, nserver_grps);
    auto slices = Param::ComputeSlices(lcm, net->params());
    auto slice2server = PartitionSlices(nservers_per_grp, slices);
    stub.Run(slice2server, workers, servers);
  }

  for (auto& thread : threads)
    thread.join();
  for (auto server : servers)
    delete server;
  delete net;
  std::set<NeuralNet*> deleted{net, nullptr};
  for (auto worker : workers) {
    for (auto ptr : worker->GetNets())
    if (deleted.find(ptr) == deleted.end()) {
      delete ptr;
      deleted.insert(ptr);
    }
    delete worker;
  }
}

void Driver::SetupForResume(JobProto* job_conf) {
  tinydir_dir dir;
  std::string folder = Cluster::Get()->checkpoint_folder();
  tinydir_open(&dir, folder.c_str());
  int latest_step = 0;
  // there would be multi checkpoint files (from diff workers) for one step
  vector<std::string> ck_files;
  // iterate all files to get the files for the last checkpoint
  while (dir.has_next) {
    tinydir_file file;
    tinydir_readfile(&dir, &file);
    tinydir_next(&dir);
    char* ch = strstr(file.name, "step");
    if (ch == nullptr) {
      if (file.name[0] != '.')
        LOG(INFO) << "Irregular file in checkpoint folder: " << file.name;
      continue;
    }
    LOG(INFO) << "Add checkpoint file for resume: " << ch;
    int step = atoi(ch+4);
    if (step == latest_step) {
      ck_files.push_back(file.name);
    } else if (step > latest_step) {
      latest_step = step;
      ck_files.clear();
      ck_files.push_back(std::string(file.name));
    }
  }
  if (latest_step > 0) {
    job_conf->set_step(latest_step);
    if (!job_conf->has_reset_param_version())
      job_conf->set_reset_param_version(false);
    job_conf->clear_checkpoint_path();
    for (auto ck_file : ck_files)
      job_conf->add_checkpoint_path(folder + "/" + ck_file);
  }
  tinydir_close(&dir);
}

const vector<Worker*> Driver::CreateWorkers(const JobProto& job_conf,
    NeuralNet* net) {
  auto cluster = Cluster::Get();
  vector<Worker*> workers;
  if (!cluster->has_worker()) return workers;
  int wgrp_size = cluster->nworkers_per_group();
  int nservers_per_grp = cluster->nservers_per_group();
  int nserver_grps = cluster->nserver_groups();
  int lcm = LeastCommonMultiple(nserver_grps, nservers_per_grp);
  const vector<int> rng = cluster->ExecutorRng(cluster->procs_id(),
      cluster->nworkers_per_group(), cluster->nworkers_per_procs());
  int gstart = rng[0], gend = rng[1], wstart = rng[2], wend = rng[3];
  for (int gid = gstart; gid < gend; gid++) {
    NeuralNet* train_net = nullptr, *test_net = nullptr, *val_net = nullptr;
    if (gid == gstart) {
      train_net = net;
      Param::SliceParams(lcm, train_net->params());
      // test and validation are performed by the 1st group.
      if (gid == 0 && job_conf.test_steps() > 0) {
        test_net = NeuralNet::Create(job_conf.neuralnet(), kTest, 1);
        test_net->ShareParamsFrom(train_net);
      }
      if (gid == 0 && job_conf.validate_steps() > 0) {
        val_net = NeuralNet::Create(job_conf.neuralnet(), kVal, 1);
        val_net->ShareParamsFrom(train_net);
      }
    } else {
      train_net = NeuralNet::Create(job_conf.neuralnet(), kTrain, wgrp_size);
      if (cluster->share_memory()) {
        train_net->ShareParamsFrom(net);
      } else {
        Param::SliceParams(lcm, train_net->params());
      }
    }
    for (int wid = wstart; wid < wend; wid++) {
      auto *worker = Worker::Create(job_conf.train_one_batch());
      // TODO(wangwei) extend to test among workers in a grp
      if (wid == 0)
        worker->Setup(gid, wid, job_conf, train_net, val_net, test_net);
      else
        worker->Setup(gid, wid, job_conf, train_net, nullptr, nullptr);
      workers.push_back(worker);
    }
  }
  return workers;
}

const vector<Server*> Driver::CreateServers(const JobProto& job_conf,
    NeuralNet* net) {
  auto cluster = Cluster::Get();
  vector<Server*> servers;
  if (!cluster->has_server()) return servers;
  int nservers_per_grp = cluster->nservers_per_group();
  int nserver_grps = cluster->nserver_groups();
  int lcm = LeastCommonMultiple(nserver_grps, nservers_per_grp);
  auto slices = Param::ComputeSlices(lcm, net->params());
  // partition among server groups, each group maintains one sub-set for sync
  auto slice2group = PartitionSlices(nserver_grps, slices);
  // partition within one server group, each server updates for one sub-set
  auto slice2server = PartitionSlices(nservers_per_grp, slices);

  int server_procs = cluster->procs_id();
  // if true, server procs (logical) id starts after worker procs
  if (cluster->server_worker_separate())
    server_procs -= cluster->nworker_procs();
  const vector<int> rng = cluster->ExecutorRng(server_procs,
      cluster->nservers_per_group(), cluster->nservers_per_procs());
  int gstart = rng[0], gend = rng[1], start = rng[2], end = rng[3];
  for (int gid = gstart; gid < gend; gid++) {
    for (int sid = start; sid < end; sid++) {
      auto server = new Server(gid, sid, job_conf, slice2group, slice2server);
      servers.push_back(server);
    }
  }
  return servers;
}

}  // namespace singa

#include "singa.h"

namespace singa {

void Driver::Init(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);

  //  unique job ID generated from singa-run.sh, passed in as "-singa_job <id>"
  int job_id_pos = ArgPos(argc, argv, "-singa_job");
  CHECK_NE(job_id_pos, -1);
  job_id_ = atoi(argv[job_id_pos]);

  //  global signa conf passed by singa-run.sh as "-singa_conf <path>"
  int singa_conf_pos = ArgPos(argc, argv, "-singa_conf");
  CHECK_NE(singa_conf_pos, -1);
  singa_conf_.insert(0, argv[singa_conf_pos]);

  //  job conf passed by users as "-conf <path>"
  int job_conf_pos = ArgPos(argc, argv, "-conf");
  CHECK_NE(job_conf_pos, -1);
  job_conf_.insert(0, argv[job_conf_pos]);

  // register layers
  RegisterLayer<BridgeDstLayer>(kBridgeDst);
  RegisterLayer<BridgeSrcLayer>(kBridgeSrc);
  RegisterLayer<ConvolutionLayer>(kConvolution);
  RegisterLayer<ConcateLayer>(kConcate);
  RegisterLayer<DropoutLayer>(kDropout);
  RegisterLayer<InnerProductLayer>(kInnerProduct);
  RegisterLayer<LabelLayer>(kLabel);
  RegisterLayer<LRNLayer>(kLRN);
  RegisterLayer<MnistLayer>(kMnist);
  RegisterLayer<PrefetchLayer>(kPrefetch);
  RegisterLayer<PoolingLayer>(kPooling);
  RegisterLayer<RGBImageLayer>(kRGBImage);
  RegisterLayer<ReLULayer>(kReLU);
  RegisterLayer<ShardDataLayer>(kShardData);
  RegisterLayer<SliceLayer>(kSlice);
  RegisterLayer<SoftmaxLossLayer>(kSoftmaxLoss);
  RegisterLayer<SplitLayer>(kSplit);
  RegisterLayer<TanhLayer>(kTanh);
  RegisterLayer<RBMVisLayer>(kRBMVis);
  RegisterLayer<RBMHidLayer>(kRBMHid);
#ifdef USE_LMDB
  RegisterLayer<LMDBDataLayer>(kLMDBData);
#endif

  // register updater
  RegisterUpdater<AdaGradUpdater>(kAdaGrad);
  RegisterUpdater<NesterovUpdater>(kNesterov);
  // TODO(wangwei) RegisterUpdater<kRMSPropUpdater>(kRMSProp);
  RegisterUpdater<SGDUpdater>(kSGD);

  // register worker
  RegisterWorker<BPWorker>(kBP);
  RegisterWorker<CDWorker>(kCD);

  // register param
  RegisterParam<Param>(0);
}

template<typename T>
int Driver::RegisterLayer(int type) {
  auto factory = Singleton<Factory<singa::Layer>>::Instance();
  factory->Register(type, CreateInstance(T, Layer));
  return 1;
}

template<typename T>
int Driver::RegisterParam(int type) {
  auto factory = Singleton<Factory<singa::Param>>::Instance();
  factory->Register(type, CreateInstance(T, Param));
  return 1;
}

template<typename T>
int Driver::RegisterUpdater(int type) {
  auto factory = Singleton<Factory<singa::Updater>>::Instance();
  factory->Register(type, CreateInstance(T, Updater));
  return 1;
}

template<typename T>
int Driver::RegisterWorker(int type) {
  auto factory = Singleton<Factory<singa::Worker>>::Instance();
  factory->Register(type, CreateInstance(T, Worker));
  return 1;
}

void Driver::Submit(bool resume, const JobProto& jobConf) {
  SingaProto conf;
  ReadProtoFromTextFile(singa_conf_.c_str(), &conf);
  if (conf.has_log_dir())
    SetupLog(conf.log_dir(), std::to_string(job_id_) + "-" + jobConf.name());
  if (jobConf.num_openblas_threads() != 1)
    LOG(WARNING) << "openblas with "
      << jobConf.num_openblas_threads() << " threads";
  openblas_set_num_threads(jobConf.num_openblas_threads());

  JobProto job;
  job.CopyFrom(jobConf);
  job.set_id(job_id_);
  Trainer trainer;
  trainer.Start(resume, conf, &job);
}

}  // namespace singa

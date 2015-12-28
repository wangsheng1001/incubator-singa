#!/usr/bin/env python
import sys, re, subprocess
from layer import *
from utils.utility import * 
from utils.message import * 
from google.protobuf import text_format

class Model(object):

  def __init__(self, name='my model', argv=[], label=False):
    '''
    optional
      name  = (string) // name of model/job
      label = (bool)   // exist label layer (depreciated)
    '''
    self.jobconf = Message('Job', name=name).proto 
    self.layers = []
    self.label = label
    self.argv = argv
    self.result = None
    self.last_checkpoint_path = None
    self.cudnn = False
    
  def exist_datalayer(self, phase):
    for ly in self.layers:
      if enumPhase(phase) in ly.layer.include:
        return True
    return False

  def compile(self, optimizer=None, cluster=None, loss=None, topk=1, **kwargs):
    '''
    required
      optimizer = (Updater) // updater settings, e.g., SGD
      cluster   = (Cluster) // cluster settings
    optional
      loss      = (string)  // name of loss function type
      topk      = (int)     // the number of results considered to compute accuracy
    '''
    assert optimizer != None, 'optimizer (Updater component) should be set'
    assert cluster != None, 'cluster (Cluster component) should be set'  
    setval(self.jobconf, updater=optimizer.proto)
    setval(self.jobconf, cluster=cluster.proto)

    # take care of loss function layer
    if loss == None:
      print 'loss layer is not set'
    else:
      if hasattr(self.layers[-1], 'mask'):
        ly = self.layers[-1].mask
      else:
        ly = self.layers[-1].layer

      # take care of the last layer
      if ly.type == enumLayerType('softmax'):
        # revise the last layer
        if loss == 'categorical_crossentropy':
          setval(ly, type=enumLayerType('softmaxloss'))
          setval(ly.softmaxloss_conf, topk=topk) 
        elif loss == 'mean_squared_error':
          setval(ly, type=enumLayerType('euclideanloss'))
      else:
        # add new layer
        if loss == 'categorical_crossentropy':
          self.add(Activation('softmaxloss', topk=topk))
        elif loss == 'mean_squared_error':
          self.add(Activation('euclideanloss'))
        elif loss == 'user_loss_rnnlm': # user-defined loss layer for rnnlm
          self.add(UserLossRNNLM(nclass=kwargs['nclass'], vocab_size=kwargs['in_dim']))

  def build(self):
    '''
    construct neuralnet proto
    '''
    net = NetProto() 
    slyname = self.layers[0].layer.name
    for i in range(len(self.layers)):
      ly = net.layer.add()
      ly.CopyFrom(self.layers[i].layer)
      lastly = ly
      if self.layers[i].is_datalayer == True:
        continue
      getattr(ly, 'srclayers').append(slyname)
      slyname = ly.name
      if hasattr(self.layers[i], 'mask'):
        mly = net.layer.add()
        mly.CopyFrom(self.layers[i].mask)
        getattr(mly, 'srclayers').append(slyname)
        slyname = mly.name
        lastly = mly
      if hasattr(self.layers[i], 'bidirect'):
        bly = net.layer.add()
        bly.CopyFrom(self.layers[i].bidirect)
        getattr(bly, 'srclayers').append(slyname)

    # deal with label layer (depreciated)
    if self.label == True:
      label_layer = Layer(name='label', type=kLabel)      
      ly = net.layer.add()
      ly.CopyFrom(label_layer.layer)
      getattr(ly, 'srclayers').append(self.layers[0].layer.name)
      getattr(lastly, 'srclayers').append(label_layer.layer.name)
    else:
      if lastly.name == 'RBMVis':
        getattr(lastly, 'srclayers').append(bly.name)
      else:
        getattr(lastly, 'srclayers').append(self.layers[0].layer.name)

    # use of cudnn
    if self.cudnn == True:
      self.setCudnnLayerType(net) 

    setval(self.jobconf, neuralnet=net)

  def fit(self, data=None, alg='bp', nb_epoch=0,
          with_test=False, execpath='', device=None, **fields):
    '''
    required
      data        = (Data)     // Data class object for training data
      alg         = (string)   // algorithm, e.g., 'bp', 'cd'
      nb_epoch    = (int)      // the number of training steps
    optional
      with_test   = (bool)     // flag if singa runs for test data
      execpath    = (string)   // path to user own singa (executable file)
      device      = (int/list) // a list of gpu ids
      **fields (KEY=VALUE)
        batch_size       = (int)    // batch size for training data
        train_steps      = (int)    // the number of steps for training, i.e., epoch
        disp_freq        = (int)    // frequency to display training info
        disp_after       = (int)    // display after this number 
        validate_data    = (Data)   // validation data, specified in load_data()
        validate_freq    = (int)    // frequency of validation
        validate_steps   = (int)    // total number of steps for validation
        validate_after   = (int)    // start validation after this number
        checkpoint_path  = (string) // path to checkpoint file
        checkpoint_freq  = (int)    // frequency for checkpoint
        checkpoint_after = (int)    // start checkpointing after this number
    '''
    assert data != None, 'Training data shold be set'
    assert nb_epoch > 0, 'Training steps shold be set'

    if 'batch_size' in fields:  # if new value is set, replace the batch_size
      setval(data.layer.store_conf, batchsize=fields['batch_size'])

    # insert layer for training
    if self.exist_datalayer('train') == False: 
      self.layers.insert(0, data)
    setval(self.jobconf, train_steps=nb_epoch)
    setval(self.jobconf, disp_freq=nb_epoch/10)
    if 'disp_freq' in fields:
      setval(self.jobconf, disp_freq=fields['disp_freq'])

    if 'validate_data' in fields:
      self.layers.insert(1, fields['validate_data'])
      setval(self.jobconf, validate_freq=nb_epoch/10)

    setval(self.jobconf, **fields)

    # loading checkpoint if it is set
    if data.checkpoint != None:
      setval(self.jobconf, checkpoint_path=data.checkpoint)

    # save model parameter (i.e., checkpoint_path)
    setval(self.jobconf, checkpoint_freq=nb_epoch)
    self.last_checkpoint_path = '{0}/step{1}-worker0'.format(
                     self.jobconf.cluster.workspace, nb_epoch) 
    
    # set Train_one_batch component, using backprogapation at default
    setval(self.jobconf, train_one_batch=Algorithm(type=enumAlgType(alg)).proto)

    # use of cudnn
    if device != None:
      setval(self.jobconf, gpu=device)
      self.cudnn = True

    # start to run singa for training
    if with_test == False: 
      self.build()  # construct Nneuralnet Component
      #self.display()
      return SingaRun(jobproto=self.jobconf, argv=self.argv, execpath=execpath)
    else:
      # run singa in evaluate() with test data
      pass


  def evaluate(self, data=None, alg='bp',
               checkpoint_path=None, execpath='', **fields):
    '''
    required
      data = (Data)   // Data class object for testing data
    optional
      checkpoint_path = (list)   // checkpoint path is necessary only for testing
      execpaths       = (string) // path to user's own executable 
      **fields (KEY=VALUE)
        batch_size   = (int)  // batch size for testing data
        test_freq    = (int)  // frequency of testing
        test_steps   = (int)  // total number of steps for testing 
        test_after   = (int)  // start testing after this number of steps 
    '''
    assert data != None, 'Testing data should be set'
    is_testonly = False

    if 'batch_size' in fields:  # if new value is set, replace the batch_size
      setval(data.layer.store_conf, batchsize=fields['batch_size'])

    # insert layer for testing
    if self.exist_datalayer('test') == False: 
      self.layers.insert(0, data)

    # loading checkpoint if singa runs only for testing
    if self.exist_datalayer('train') == False: 
      is_testonly = True
      if checkpoint_path == None:
        print 'checkpoint_path has not been specified'
      else:
        setval(self.jobconf, checkpoint_path=checkpoint_path)

    steps = fields['test_steps'] if 'test_steps' in fields else 10
    setval(self.jobconf, test_steps=steps)
    setval(self.jobconf, **fields)
    
    # set Train_one_batch component, using backprogapation at default
    setval(self.jobconf, train_one_batch=Algorithm(type=enumAlgType(alg)).proto)

    self.build()  # construct Nneuralnet Component

    #--- generate job.conf file for debug purpose 
    #filename = 'job.conf'
    #with open(filename, 'w') as f:
    #  f.write(text_format.MessageToString(self.jobconf.cluster))
    self.display()

    #--- run singa --- 
    return SingaRun(jobproto=self.jobconf, argv=self.argv, execpath=execpath, testmode=is_testonly)
    #return SingaRun_script(filename=filename, execpath=execpath)
    

  def display(self):
    ''' print out job proto
    '''
    print text_format.MessageToString(self.jobconf)

  def setCudnnLayerType(self, net):
    ''' convert LayerType to CdunnLayerType
    '''
    for i in range(len(net.layer)):
      ly_type = net.layer[i].type
      cudnn_ly_type = ly_type
      if ly_type == kCConvolution: cudnn_ly_type = kCudnnConv
      elif ly_type == kCPooling: cudnn_ly_type = kCudnnPool
      elif ly_type == kLRN: cudnn_ly_type = kCudnnLRN
      elif ly_type == kSoftmax: cudnn_ly_type = kCudnnSoftmax
      elif ly_type == kSoftmaxLoss: cudnn_ly_type = kCudnnSoftmaxLoss
      elif ly_type == kSTanh:
        cudnn_ly_type = kCudnnActivation
        net.layer[i].activation_conf.type = STANH 
      elif ly_type == kSigmoid:
        cudnn_ly_type = kCudnnActivation
        net.layer[i].activation_conf.type = SIGMOID 
      elif ly_type == kReLU:
        cudnn_ly_type = kCudnnActivation
        net.layer[i].activation_conf.type = RELU 
      net.layer[i].type = cudnn_ly_type


class Energy(Model):
  def __init__(self, name='my model', argv=[], label=False):
    super(Energy, self).__init__(name=name, argv=argv, label=label)

  def add(self, layer):
    if hasattr(layer, 'layer_type'):
      if layer.layer_type == kRBMVis:
        dim = 0 
        for i in range(1, len(layer.out_dim)):
          parw = Parameter(name='w', init='none', level=i)
          parb = Parameter(name='b', init='none', level=i)
          dim = layer.out_dim[i-1]
          self.layers.append(Dense(dim, w_param=parw, b_param=parb, activation='sigmoid'))
        self.layers.append(layer)


class Sequential(Model):
  def __init__(self, name='my model', argv=[], label=False):
    super(Sequential, self).__init__(name=name, argv=argv, label=label)

  def add(self, layer):
    if hasattr(layer, 'layer_type'):
      if layer.layer_type == 'AutoEncoder':
        if layer.param_share == True:
          dim = 0 
          # Encoding
          for i in range(1, len(layer.hid_dim)+1):
            parw = Parameter(name='w', init='none', level=i)
            parb = Parameter(name='b', init='none', level=i)
            dim = layer.hid_dim[i-1]
            if i == len(layer.hid_dim): activation = None
            else: activation = layer.activation
            self.layers.append(Dense(dim, w_param=parw, b_param=parb, activation=activation))
          # Decoding
          for i in range(len(layer.hid_dim), 0, -1):
            parw = Parameter(name=generateName('w',2), init='none')
            parb = Parameter(name=generateName('b',2), init='none')
            setval(parw.param, share_from='w'+str(i))
            setval(parb.param, name='b'+str(i))
            if i == 1: dim = layer.out_dim
            else: dim = layer.hid_dim[i-2]
            self.layers.append(Dense(dim, w_param=parw, b_param=parb, activation=layer.activation, transpose=True))
        else:
          # MLP
          for i in range(1, len(layer.hid_dim)+2):
            parw = Parameter(name='w', init='none', level=i)
            parb = Parameter(name='b', init='none', level=i)
            if i == len(layer.hid_dim)+1: dim = layer.out_dim
            else: dim = layer.hid_dim[i-1]
            self.layers.append(Dense(dim, w_param=parw, b_param=parb, activation=layer.activation))
      else:
        self.layers.append(layer)
    else:
      self.layers.append(layer)


class Store(object):
  def __init__(self, **kwargs):
    '''
    **kwargs
        path       = (string)  // path to dataset
        backend    = (string)  // 
        batch_size = (int)     // batch size of dataset
        shape      = (int)     // 

    '''
    self.proto = Message('Store', **kwargs).proto

class Algorithm(object):
  def __init__(self, type=enumAlgType('bp'), **kwargs):
    alg = Message('Alg', alg=type, **kwargs).proto
    if type == enumAlgType('cd'):
      setval(alg.cd_conf, **kwargs)
    self.proto = alg

class Updater(object):
  def __init__(self, upd_type, lr, lr_type,
               decay, momentum,
               step, step_lr, **fields):

    upd = Message('Updater', type=upd_type, **fields).proto
    setval(upd.learning_rate, base_lr=lr) 
    if decay > 0:
      setval(upd, weight_decay=decay) 
    if momentum > 0:
      setval(upd, momentum=momentum) 

    if lr_type == None:
      setval(upd.learning_rate, type=kFixed) 
    elif lr_type == 'step':
      cp = Message('Step', change_freq=60, gamma=0.997)
      setval(upd.learning_rate, type=kStep, step_conf=cp.proto) 
    elif lr_type == 'fixed':
      cp = Message('FixedStep', step=step, step_lr=step_lr)
      setval(upd.learning_rate, type=kFixedStep, fixedstep_conf=cp.proto) 
    elif lr_type == 'linear':
      cp = Message('Linear', change_freq=10, final_lr=0.1)
      setval(upd.learning_rate, type=kLinear, linear_conf=cp.proto) 
    self.proto = upd

class SGD(Updater):
  def __init__(self, lr=0.01, lr_type=None,
               decay=0, momentum=0,
               step=(0), step_lr=(0.01), **fields):
    '''
    required
       lr      = (float)  // base learning rate
    optional
       lr_type = (string) // type of learning rate, 'Fixed' at default
       decay    = (float) // weight decay
       momentum = (float) // momentum
       **fields (KEY=VALUE)

    '''
    assert lr
    super(SGD, self).__init__(upd_type=kSGD,
               lr=lr, lr_type=lr_type,
               decay=decay, momentum=momentum,
               step=step, step_lr=step_lr, **fields)

class AdaGrad(Updater):
  def __init__(self, lr=0.01, lr_type=None,
               decay=0, momentum=0,
               step=(0), step_lr=(0.01), **fields):
    '''
    required
       lr      = (float)  // base learning rate
    optional
       lr_type = (string) // type of learning rate, 'Fixed' at default
       decay    = (float) // weight decay
       momentum = (float) // momentum
       **fields (KEY=VALUE)

    '''
    assert lr
    super(AdaGrad, self).__init__(upd_type=kAdaGrad,
               lr=lr, lr_type=lr_type,
               decay=decay, momentum=momentum,
               step=step, step_lr=step_lr, **fields)


class Cluster(object):
  def __init__(self, workspace=None,
               nworker_groups=1, nserver_groups=1,
               nworkers_per_group=1, nservers_per_group=1,
               nworkers_per_procs=1, nservers_per_procs=1,
               **fields):
    '''
    required
      workspace = (string) // workspace path
    optional
      nworker_groups     = (int)
      nserver_groups     = (int)
      nworkers_per_group = (int)
      nservers_per_group = (int)
      nworkers_per_procs = (int)
      nservers_per_procs = (int)
      **fields
        server_worker_separate = (bool)
    '''
    assert workspace != None, 'need to set workspace'
    self.proto = Message('Cluster', workspace=workspace).proto
    # optional
    self.proto.nworker_groups = nworker_groups 
    self.proto.nserver_groups = nserver_groups 
    self.proto.nworkers_per_group = nworkers_per_group 
    self.proto.nservers_per_group = nservers_per_group 
    self.proto.nworkers_per_procs = nworkers_per_procs 
    self.proto.nservers_per_procs = nservers_per_procs 
    # other fields
    setval(self.proto, **fields)



def StoreResults(lines):

  resultDic = {} 
  for line in lines:
    line = re.findall(r'[\w|*.*]+', line)
    if 'Train' in line:
      step = line[line.index('step')+1]
      if 'accuracy' in line:
        resultDic.setdefault(step,{})['acc'] = line[line.index('accuracy')+1] 
      if 'loss' in line:
        resultDic.setdefault(step,{})['loss'] = line[line.index('loss')+1] 
      if 'ppl' in line:
        resultDic.setdefault(step,{})['ppl'] = line[line.index('ppl')+1] 
      if 'Squared' in line:
        resultDic.setdefault(step,{})['se'] = line[line.index('Squared')+2] 
  return resultDic

def SingaRun(jobproto='', argv=[], execpath='', testmode=False):

  import singa.driver as driver
  d = driver.Driver()
  d.InitLog(argv[0]) 
  d.Init(argv)
  if testmode == True:
    d.Test(jobproto.SerializeToString())
  else:
    d.Train(False, jobproto.SerializeToString())

  logfile = '/tmp/singa-log/{0}.ERROR'.format(argv[0].split('/')[-1])
  fin = open(logfile, 'r')
  result = StoreResults(fin.readlines())
 
  return result

def SingaRun_script(filename='', execpath=''):
  SINGAROOT = '../../../'
  conf = 'examples/' + filename
  if execpath=='':
    cmd = SINGAROOT+'bin/singa-run.sh ' \
        + '-conf %s ' % conf 
  else:
    cmd = SINGAROOT+'bin/singa-run.sh ' \
        + '-conf %s ' % conf \
        + '-exec %s ' % execpath 

  procs = subprocess.Popen(cmd.strip().split(' '), stdout = subprocess.PIPE, stderr = subprocess.STDOUT)

  resultDic = {} 
  outputlines = iter(procs.stdout.readline, '')
  resultDic = StoreResults(outputlines)

  #TODO better format to store the result??
  return resultDic


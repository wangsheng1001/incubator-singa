#!/usr/bin/env bash
#
#/**
# * Copyright 2015 The Apache Software Foundation
# *
# * Licensed to the Apache Software Foundation (ASF) under one
# * or more contributor license agreements.  See the NOTICE file
# * distributed with this work for additional information
# * regarding copyright ownership.  The ASF licenses this file
# * to you under the Apache License, Version 2.0 (the
# * "License"); you may not use this file except in compliance
# * with the License.  You may obtain a copy of the License at
# *
# *     http://www.apache.org/licenses/LICENSE-2.0
# *
# * Unless required by applicable law or agreed to in writing, software
# * distributed under the License is distributed on an "AS IS" BASIS,
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# * See the License for the specific language governing permissions and
# * limitations under the License.
# */
# 
# console to list/view/kill singa jobs
#

usage="Usage: singa-console.sh <command> <args>\n
        list           :  list running singa jobs\n
        view <job id>  :  view procs of a singa job\n
        kill <job id>  :  kill a singa job"

if [ $# == 0 ]; then
  echo -e $usage
  exit 1
fi

# get environment variables
. `dirname "${BASH_SOURCE-$0}"`/singa-env.sh
cd $SINGA_HOME

case $1 in
  list)
    if [ $# != 1 ]; then
      echo -e $usage
      exit 1
    fi
    ./singatool list || exit 1
    ;;

  view)
    if [ $# != 2 ]; then
      echo -e $usage
      exit 1
    fi
    ./singatool view $2 || exit 1
    ;;

  kill)
    if [ $# != 2 ]; then
      echo -e $usage
      exit 1
    fi
    hosts=`./singatool view "$2"`
    [ $? == 0 ] || exit 1
    ssh_options="-oStrictHostKeyChecking=no \
             -oUserKnownHostsFile=/dev/null \
             -oLogLevel=quiet"
    if [ `head -1 "$SINGA_CONF"/hostfile` == localhost ]; then
      local_procs=1
    fi
    for i in ${hosts[@]}; do
      proc=(`echo $i | tr '|' ' '`)
      singa_kill="kill -9 "${proc[1]}
      if [ -z $local_procs ]; then
        echo Kill singa @ $i ...
        ssh $ssh_options ${proc[0]} $singa_kill
      else
        echo Kill singa @ ${proc[1]} ...
        $singa_kill
      fi
    done
    ./singatool remove $2 || exit 1
    ;;
  
  *)
    echo -e $usage
    exit 1
esac

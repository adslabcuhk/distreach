#!/usr/bin/env bash
cd ../../
source scripts/common.sh
cd nocache/tofino

source /root/.bashrc

$SDE/run_p4_tests.sh -p netcache -t ${SWITCH_ROOTPATH}/netcache/tofino/configure/ --target hw --setup

# bfrt_python ./setup_61_165.py true

#!/usr/bin/env bash
cd ../../
source scripts/common.sh
cd farreach/tofino

source /root/.zshrc

cd $SDE
./run_p4_tests.sh -p netbufferv4 -t ${SWITCH_ROOTPATH}/farreach/tofino/clear_obselete_snapshottoken/ --target hw --setup

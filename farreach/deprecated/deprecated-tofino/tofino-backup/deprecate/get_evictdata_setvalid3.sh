set -x
source ../../scripts/common.sh
#!/usr/bin/env bash

cd $SDE
./run_p4_tests.sh -p netbufferv4 -t ${SWITCH_ROOTPATH}/netreach-v4-lsm/tofino/get_evictdata_setvalid3/ --target hw --setup

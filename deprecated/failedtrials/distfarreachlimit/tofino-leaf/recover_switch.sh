source ../../scripts/common.sh
#!/usr/bin/env bash

cd $SDE
./run_p4_tests.sh -p distfarreachlimitleaf -t ${SWITCH_ROOTPATH}/distfarreachlimit/tofino-leaf/recover_switch/ --target hw --setup
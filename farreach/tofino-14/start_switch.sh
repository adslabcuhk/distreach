set -x
#!/usr/bin/env bash

source /root/.zshrc

cd $SDE
./run_switchd.sh -p netbufferv4

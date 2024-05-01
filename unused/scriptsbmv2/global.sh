# set -x
# Global information unchanged during experiments

USER="ssy"

SWITCH_PRIVATEKEY=".ssh/switch-private-key"
CONNECTION_PRIVATEKEY=".ssh/connection-private-key"

MAIN_CLIENT="dl11" # used in recovery mode
SECONDARY_CLIENT="dl20"

# NOTE: must be consistent with each config.ini
SERVER0="dl21"
SERVER1="dl30"

LEAFSWITCH="bf3"
# NOTE:SPINESWITCH is not used at this stage
# SPINESWITCH="bf3" 

CLIENT_ROOTPATH="/root/farreach-private"
SWITCH_ROOTPATH="/root/farreach-private"
SERVER_ROOTPATH="/root/farreach-private"

# backup rocksdb after loading phase
BACKUPS_ROOTPATH="/tmp/rocksdbbackups"

# experiment
EVALUATION_SCRIPTS_PATH="${CLIENT_ROOTPATH}/scriptsbmv2"
EVALUATION_OUTPUT_PREFIX="/root/aeresults"

# network settings

MAIN_CLIENT_TOSWITCH_IFNAME="s1-eth1"
MAIN_CLIENT_TOSWITCH_IP="10.0.1.1"
MAIN_CLIENT_TOSWITCH_MAC="00:00:0a:00:01:01"
MAIN_CLIENT_TOSWITCH_FPPORT="1\/0"
MAIN_CLIENT_TOSWITCH_PIPEIDX="1"
MAIN_CLIENT_LOCAL_IP="10.0.1.1"
# 172.16.112.11"

SECONDARY_CLIENT_TOSWITCH_IFNAME="s1-eth2"
SECONDARY_CLIENT_TOSWITCH_IP="10.0.1.2"
SECONDARY_CLIENT_TOSWITCH_MAC="00:00:0a:00:01:02"
SECONDARY_CLIENT_TOSWITCH_FPPORT="2\/0"
SECONDARY_CLIENT_TOSWITCH_PIPEIDX="0"
SECONDARY_CLIENT_LOCAL_IP=""10.0.1.2"
# 172.16.112.20"

SERVER0_TOSWITCH_IFNAME="s1-eth3"
SERVER0_TOSWITCH_IP="10.0.1.3"
SERVER0_TOSWITCH_MAC="00:00:0a:00:01:03"
SERVER0_TOSWITCH_FPPORT="3\/0"
SERVER0_TOSWITCH_PIPEIDX="1"
SERVER0_LOCAL_IP=""10.0.1.3"
# 172.16.112.21"

SERVER1_TOSWITCH_IFNAME="s1-eth4"
SERVER1_TOSWITCH_IP="10.0.1.4"
SERVER1_TOSWITCH_MAC="00:00:0a:00:01:04"
SERVER1_TOSWITCH_FPPORT="4\/0"
SERVER1_TOSWITCH_PIPEIDX="1"
SERVER1_LOCAL_IP=""10.0.1.4"
# 172.16.112.30"

CONTROLLER_LOCAL_IP="10.0.1.3"
SWITCHOS_LOCAL_IP="10.0.1.5"
SWITCH_RECIRPORT_PIPELINE1TO0="7\/0"
SWITCH_RECIRPORT_PIPELINE0TO1="12\/0"

# CPU settings

SERVER0_WORKER_CORENUM="1"
SERVER0_TOTAL_CORENUM="2"
SERVER1_WORKER_CORENUM="1"
SERVER1_TOTAL_CORENUM="2"

is_global_included=1
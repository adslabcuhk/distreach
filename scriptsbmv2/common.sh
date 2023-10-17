# set -x
if [ "x${is_global_included}" != "x1" ]
then
	source scriptsbmv2/global.sh
fi

function readini() {
	tmpfile=$1
	result=$(awk -F '=' -v tmpsection=[$2] -v tmpkey=$3 '$0==tmpsection {flag = 1; next} /^\[/ {flag = 0; next} flag && $1==tmpkey {print $2}' ${tmpfile})
	echo ${result}
}

DIRNAME="netcache"

##### method-related variables #####

with_controller=0
if [ "x${DIRNAME}" == "xfarreach" ] || [ "x${DIRNAME}" == "xnetcache" ] || [ "x${DIRNAME}" == "xdistfarreach" ] || [ "x${DIRNAME}" == "xdistcache" ]
then
	with_controller=1
fi

with_reflector=0
if [ "x${DIRNAME}" == "xdistfarreach" ] || [ "x${DIRNAME}" == "xdistcache" ]
then
	with_reflector=1
fi

is_distributed=0
if [ "x${DIRNAME}" == "xdistfarreach" ] || [ "x${DIRNAME}" == "xdistcache" ] || [ "x${DIRNAME}" == "xdistnocache" ]
then
	is_distributed=1
fi

##### Parse ${DRINAME}/config.ini #####

#CURRENT_ROOTPATH=${CLIENT_ROOTPATH}
#if [ $(hostname) -eq ${SERVER0} ] || [ $(hostname) -eq ${SERVER1} ]
#then
#	CURRENT_ROOTPATH=${SERVER_ROOTPATH}
#elif [ $(hostname) -eq ${LEAFSWITCH} ] || [ $(hostname) -eq ${SPINESWITCH} ]
#	CURRENT_ROOTPATH=${SWITCH_ROOTPATH}
#fi

configfile=${DIRNAME}/config.ini
echo "[COMMON] load configuration from ${configfile}"

# for server rotation (must be consistent with each config.ini)
workloadname=$(readini ${configfile} "global" "workload_name")
workloadmode=$(readini ${configfile} "global" "workload_mode")
dynamicpattern=$(readini ${configfile} "global" "dynamic_ruleprefix")
bottleneck_serveridx=$(readini ${configfile} "global" "bottleneck_serveridx_for_rotation")
server_total_logical_num=$(readini ${configfile} "global" "server_total_logical_num")
server_total_logical_num_for_rotation=$(readini ${configfile} "global" "server_total_logical_num_for_rotation")

if [ "x${DIRNAME}" == "xfarreach" ]
then
	snapshot_period=$(readini ${configfile} "controller" "controller_snapshot_period") # in units of ms
	cache_size=$(readini ${configfile} "switch" "switch_kv_bucket_num")
else
	snapshot_period=0
	cache_size=0
fi

is_common_included=1

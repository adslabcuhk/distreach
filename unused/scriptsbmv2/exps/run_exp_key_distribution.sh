set -x
#!/bin/bash
# run this scripts on ${MAIN_CLIENT} (main client)
# exp_key_distribution

source scriptsbmv2/global.sh
if [ $# -ne 1 ]; then
	echo "Usage: bash scriptsbmv2/exps/run_exp_key_distribution.sh <roundnumber>"
	exit
fi
roundnumber=$1

exp5_server_scale=16
exp5_method_list=("farreach" "netcache" "nocache")
exp5_workload_list=("skewness-90" "skewness-95" "uniform")
exp5_existed_workload_list=("synthetic")
exp5_backup_workload_list=("skewness-99")
exp5_output_path="${EVALUATION_OUTPUT_PREFIX}/exp5/${roundnumber}"

### Create output directory
mkdir -p ${exp5_output_path}

for exp5_method in ${exp5_method_list[@]}; do
  sed -i "/^DIRNAME=/s/=.*/=\"${exp5_method}\"/" ${CLIENT_ROOTPATH}/scriptsbmv2/common.sh
  cd ${CLIENT_ROOTPATH}/
  bash scriptsbmv2/remote/sync_file.sh scripts common.sh

  for exp5_workload in ${exp5_workload_list[@]}; do
    echo "[exp5][${exp5_method}][${exp5_workload}]"

    ### Preparation
    echo "[exp5][${exp5_method}][${exp5_workload}] run workload with $exp5_workload" servers
    cd ${SWITCH_ROOTPATH}/${exp5_method}
    bash localscriptsbmv2/stopswitchtestbed.sh
    
    echo "[exp5][${exp5_method}][${exp5_workload}] update ${exp5_method} config with ${exp5_workload}"
    cp ${CLIENT_ROOTPATH}/${exp5_method}/configs/config.ini.bmv2 ${CLIENT_ROOTPATH}/${exp5_method}/config.ini
    sed -i "/^workload_name=/s/=.*/="${exp5_workload}"/" ${CLIENT_ROOTPATH}/${exp5_method}/config.ini
    sed -i "/^server_total_logical_num=/s/=.*/="${exp5_server_scale}"/" ${CLIENT_ROOTPATH}/${exp5_method}/config.ini
    sed -i "/^server_total_logical_num_for_rotation=/s/=.*/="${exp5_server_scale}"/" ${CLIENT_ROOTPATH}/${exp5_method}/config.ini
    sed -i "/^controller_snapshot_period=/s/=.*/=10000/" ${CLIENT_ROOTPATH}/${exp5_method}/config.ini
    sed -i "/^switch_kv_bucket_num=/s/=.*/=10000/" ${CLIENT_ROOTPATH}/${exp5_method}/config.ini
    if [ "x${exp5_workload}" == "xuniform" ]; then
      sed -i "/^bottleneck_serveridx_for_rotation=/s/=.*/="5"/" ${CLIENT_ROOTPATH}/${exp5_method}/config.ini
    else
      sed -i "/^bottleneck_serveridx_for_rotation=/s/=.*/="8"/" ${CLIENT_ROOTPATH}/${exp5_method}/config.ini
    fi

    cd ${CLIENT_ROOTPATH}
    echo "[exp5][${exp5_method}][${exp5_workload}] prepare server rotation" 
    bash scriptsbmv2/remote/prepare_server_rotation.sh

    echo "[exp5][${exp5_method}][${exp5_workload}] start switchos" 
    cd ${SWITCH_ROOTPATH}/${exp5_method}/bmv2; 
    nohup python network.py &
    cd ${SWITCH_ROOTPATH}/${exp5_method}; 
    bash localscriptsbmv2/launchswitchostestbed.sh

    sleep 20s

    ### Evaluation
    echo "[exp5][${exp5_method}][${exp5_workload}] test server rotation" 
    bash scriptsbmv2/remote/test_server_rotation.sh

    echo "[exp5][${exp5_method}][${exp5_workload}] stop server rotation"
    bash scriptsbmv2/remote/stop_server_rotation.sh

    echo "[exp5][${exp5_method}][${exp5_workload}] sync json file and calculate"
    bash scriptsbmv2/remote/calculate_statistics.sh 0


    ### Cleanup
    cp ${CLIENT_ROOTPATH}/benchmark/output/${exp5_workload}-statistics/${exp5_method}-static${exp5_server_scale}-client0.out  ${exp5_output_path}/${exp5_workload}-${exp5_method}-static${exp5_server_scale}-client0.out 
    cp ${CLIENT_ROOTPATH}/benchmark/output/${exp5_workload}-statistics/${exp5_method}-static${exp5_server_scale}-client1.out  ${exp5_output_path}/${exp5_workload}-${exp5_method}-static${exp5_server_scale}-client1.out 
    echo "[exp5][${exp5_method}][${exp5_workload}] stop switchos" 
    cd ${SWITCH_ROOTPATH}/${exp5_method}; 
    bash localscriptsbmv2/stopswitchtestbed.sh
  done
  
  ### Backup for generated workloads
  existed=0
  for exp5_existed_workload in ${exp5_existed_workload_list[@]}; do
    cp ${CLIENT_ROOTPATH}/benchmark/output/${exp5_existed_workload}-statistics/${exp5_method}-static${exp5_server_scale}-client0.out  ${exp5_output_path}/${exp5_backup_workload_list[${existed}]}-${exp5_method}-static${exp5_server_scale}-client0.out 
    cp ${CLIENT_ROOTPATH}/benchmark/output/${exp5_existed_workload}-statistics/${exp5_method}-static${exp5_server_scale}-client1.out  ${exp5_output_path}/${exp5_backup_workload_list[${existed}]}-${exp5_method}-static${exp5_server_scale}-client1.out 
    existed=$((++existed))
  done
done



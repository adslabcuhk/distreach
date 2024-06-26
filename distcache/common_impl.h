#ifndef COMMON_IMPL_H
#define COMMON_IMPL_H

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../common/iniparser/iniparser_wrapper.h"
#include "../common/key.h"
#include "../common/packet_format_impl.h"
#include "../common/socket_helper.h"
#include "../common/val.h"
#include "../common/workloadparser/parser.h"
#include "message_queue_impl.h"

/*
 * Class and alias
 */

#define CURMETHOD_ID NETCACHE_ID

typedef GetRequest<netreach_key_t> get_request_t;
typedef PutRequest<netreach_key_t, val_t> put_request_t;
typedef DelRequest<netreach_key_t> del_request_t;
typedef ScanRequest<netreach_key_t> scan_request_t;
typedef GetResponse<netreach_key_t, val_t> get_response_t;
typedef PutResponse<netreach_key_t> put_response_t;
typedef DelResponse<netreach_key_t> del_response_t;
typedef ScanResponseSplit<netreach_key_t, val_t> scan_response_split_t;
typedef GetRequestPOP<netreach_key_t> get_request_pop_t;
typedef GetRequestNLatest<netreach_key_t> get_request_nlatest_t;
typedef GetResponseLatestSeq<netreach_key_t, val_t> get_response_latest_seq_t;
typedef GetResponseLatestSeqInswitchCase1<netreach_key_t, val_t> get_response_latest_seq_inswitch_case1_t;
typedef GetResponseDeletedSeq<netreach_key_t, val_t> get_response_deleted_seq_t;
typedef GetResponseDeletedSeqInswitchCase1<netreach_key_t, val_t> get_response_deleted_seq_inswitch_case1_t;
typedef PutRequestSeq<netreach_key_t, val_t> put_request_seq_t;
typedef PutRequestPopSeq<netreach_key_t, val_t> put_request_pop_seq_t;
typedef PutRequestSeqInswitchCase1<netreach_key_t, val_t> put_request_seq_inswitch_case1_t;
typedef PutRequestSeqCase3<netreach_key_t, val_t> put_request_seq_case3_t;
typedef PutRequestPopSeqCase3<netreach_key_t, val_t> put_request_pop_seq_case3_t;
typedef DelRequestSeq<netreach_key_t> del_request_seq_t;
typedef DelRequestSeqInswitchCase1<netreach_key_t, val_t> del_request_seq_inswitch_case1_t;
typedef DelRequestSeqCase3<netreach_key_t> del_request_seq_case3_t;
typedef ScanRequestSplit<netreach_key_t> scan_request_split_t;
typedef CachePop<netreach_key_t, val_t> cache_pop_t;
typedef CachePopInswitch<netreach_key_t, val_t> cache_pop_inswitch_t;
typedef CachePopInswitchAck<netreach_key_t> cache_pop_inswitch_ack_t;
typedef CacheEvict<netreach_key_t, val_t> cache_evict_t;
typedef CacheEvictAck<netreach_key_t> cache_evict_ack_t;
typedef CacheEvictCase2<netreach_key_t, val_t> cache_evict_case2_t;
typedef WarmupRequest<netreach_key_t> warmup_request_t;
typedef WarmupAck<netreach_key_t> warmup_ack_t;
typedef LoadRequest<netreach_key_t, val_t> load_request_t;
typedef LoadAck<netreach_key_t> load_ack_t;
typedef CachePopAck<netreach_key_t> cache_pop_ack_t;
typedef CacheEvictLoadfreqInswitch<netreach_key_t> cache_evict_loadfreq_inswitch_t;
typedef CacheEvictLoadfreqInswitchAck<netreach_key_t> cache_evict_loadfreq_inswitch_ack_t;
typedef CacheEvictLoaddataInswitch<netreach_key_t> cache_evict_loaddata_inswitch_t;
typedef CacheEvictLoaddataInswitchAck<netreach_key_t, val_t> cache_evict_loaddata_inswitch_ack_t;
// NOTE: both loadsnapshotdata_inswitch_t and cache_evict_loaddata_inswitch_t have op_hdr.key to be forwarded into corresponding egress pipeline and inswitch_hdr.idx to read corresponding register; both loadsnapshotdata_inswitch_ack_t and cache_evict_loaddata_inswitch_ack_t have val_hdr + seq_hdr + stat_hdr to report data from data plane to switch os;
// NOTE: loadsnapshotdata_inswitch_ack_t has an extra inswitch_hdr.idx
// DEPRECATED: we simply reuse cache_evict_loaddata_inswitch_t and cache_evict_loaddata_inswitch_ack_t for loading snapshot data
// NOTE: we cannot simply reuse cache_evict_loaddata_inswitch_/ack_t, as reflector cannot distinguish the two optypes and does not know which one of switchos,popserver.popclient_for_reflector or switchos.snapshotserver.snapshotclient_for_reflector it should forward ACK to; actually loading evicted data or snapshot data can happen simultaneously!
typedef LoadsnapshotdataInswitch<netreach_key_t> loadsnapshotdata_inswitch_t;
typedef LoadsnapshotdataInswitchAck<netreach_key_t, val_t> loadsnapshotdata_inswitch_ack_t;
typedef SetvalidInswitch<netreach_key_t> setvalid_inswitch_t;
typedef SetvalidInswitchAck<netreach_key_t> setvalid_inswitch_ack_t;
typedef NetcacheGetRequestPop<netreach_key_t> netcache_getreq_pop_t;
typedef NetcacheCachePop<netreach_key_t> netcache_cache_pop_t;
typedef NetcacheCachePopAck<netreach_key_t, val_t> netcache_cache_pop_ack_t;
typedef NetcacheCachePopFinish<netreach_key_t> netcache_cache_pop_finish_t;
typedef NetcacheCachePopFinishAck<netreach_key_t> netcache_cache_pop_finish_ack_t;
typedef NetcacheWarmupRequestInswitchPop<netreach_key_t> netcache_warmupreq_inswitch_pop_t;
typedef NetcacheCacheEvict<netreach_key_t> netcache_cache_evict_t;
typedef NetcacheCacheEvictAck<netreach_key_t> netcache_cache_evict_ack_t;
typedef NetcachePutRequestSeqCached<netreach_key_t, val_t> netcache_put_request_seq_cached_t;
typedef NetcacheDelRequestSeqCached<netreach_key_t> netcache_del_request_seq_cached_t;
typedef NetcacheValueupdate<netreach_key_t, val_t> netcache_valueupdate_t;
typedef NetcacheValueupdateAck<netreach_key_t> netcache_valueupdate_ack_t;
// For large value
typedef PutRequestLargevalue<netreach_key_t, val_t> put_request_largevalue_t;
typedef PutRequestLargevalueSeq<netreach_key_t, val_t> put_request_largevalue_seq_t;
typedef GetResponseLargevalue<netreach_key_t, val_t> get_response_largevalue_t;
typedef GetResponseLargevalueServer<netreach_key_t, val_t> get_response_largevalue_server_t;
typedef NetcacheCachePopAckNLatest<netreach_key_t, val_t> netcache_cache_pop_ack_nlatest_t;
typedef NetcacheCachePopInswitchNLatest<netreach_key_t, val_t> netcache_cache_pop_inswitch_nlatest_t;
typedef PutRequestLargevalueSeqCached<netreach_key_t, val_t> put_request_largevalue_seq_cached_t;
typedef PutRequestLargevalueSeqCase3<netreach_key_t, val_t> put_request_largevalue_seq_case3_t;

/*
 * Constants
 */

// size_t max_sending_rate = size_t(20 * 1024); // limit sending rate to x (e.g., the aggregate rate of servers)
// const size_t rate_limit_period = 1000 * 1000; // 1s

/*
 * Parameters
 */
#undef WARMUP_RAW_WORKLOAD
#undef DYNAMIC_RULEPATH
#define WARMUP_RAW_WORKLOAD(buf, workload) \
	sprintf(buf, "../benchmarkdist/output/%s-hotest.out", workload)

#define DYNAMIC_RULEPATH(buf, workload, prefix) \
	sprintf(buf, "../benchmarkdist/output/%s-%srules/", workload, prefix)
// global
const char* workload_name = nullptr;
int workload_mode = 0;
int dynamic_periodnum = 0;
int dynamic_periodinterval = 0;
const char* dynamic_ruleprefix = nullptr;
uint32_t client_physical_num = 0;
uint32_t server_physical_num = 0;
uint32_t client_total_logical_num = 0;
uint32_t server_total_logical_num = 0;
uint32_t server_total_logical_num_for_rotation = 0;
// NOTE: even under server rotation, max_server_total_logical_num = server_total_logical_num in prepare/load/warmup phase; max_server_total_logical_num != server_total_logical_num ONLY in transaction phase; (prepare phase: launch and configure switch and switchos; ONLY controller and server restart for each time of experiment under server rotation)
uint32_t max_server_total_logical_num = 0;
uint16_t bottleneck_serveridx_for_rotation = 0;

// common client configuration
short client_rotationdataserver_port = 0;
short client_sendpktserver_port_start = 0;
short client_rulemapserver_port_start = 0;
short client_worker_port_start = 0;

// each physical client
std::vector<uint32_t> client_logical_nums;
std::vector<const char*> client_ips;
std::vector<uint8_t*> client_macs;  // allocate uint8_t[6] for each macaddr
std::vector<const char*> client_fpports;
std::vector<uint32_t> client_pipeidxes;
std::vector<const char*> client_ip_for_client0_list;

// server common configuration
// server: loading phase (for loader instead of ycsb benchmark)
uint32_t server_load_factor = 1;
// server: transaction phase
short server_worker_port_start = -1;
short server_evictserver_port_start = -1;
short server_popserver_port_start = -1;
short server_valueupdateserver_port_start = -1;
short transaction_loadfinishserver_port = -1;

// each physical server
std::vector<uint32_t> server_worker_corenums;
std::vector<uint32_t> server_total_corenums;
std::vector<std::vector<uint16_t>> server_logical_idxes_list;
std::vector<const char*> server_ips;
std::vector<uint8_t*> server_macs;
std::vector<const char*> server_fpports;
std::vector<uint32_t> server_pipeidxes;
std::vector<const char*> server_ip_for_controller_list;

// leafswitchos
std::vector<const char*> leafswitchos_ips;
// spineswitchos
std::vector<const char*> spineswitchos_ips;
short lead_spine_switchos_sync_port = 10079;
// controller
const char* controller_ip_for_server = nullptr;
const char* controller_ip_for_switchos = nullptr;
short controller_popserver_port_start = -1;
short controller_evictserver_port = -1;
uint32_t controller_snapshot_period = 0;  // ms
short controller_warmupfinishserver_port = -1;

// switch
uint32_t switch_partition_count;
uint32_t switch_kv_bucket_num;
uint32_t switch_pipeline_num;
const char* switchos_ip = nullptr;
const char* spineswitchos_ip = nullptr;
uint32_t switchos_sample_cnt = 0;
short switchos_popserver_port = -1;
short switchos_snapshotserver_port = -1;
short switchos_specialcaseserver_port = -1;
short switchos_ptf_popserver_port = -1;
short switchos_ptf_snapshotserver_port = -1;

// reflector
const char* reflector_ip_for_switchos = nullptr;
short reflector_dp2cpserver_port = -1;
short reflector_cp2dpserver_port = -1;

// calculated metadata
char raw_load_workload_filename[256];  // used by split_workload for loading phase
char server_load_workload_dir[256];
char raw_warmup_workload_filename[256];
char raw_run_workload_filename[256];  // used by split_workload for transaction phase
char client_workload_dir[256];
// size_t per_client_per_period_max_sending_rate;
uint64_t perserver_keyrange = 0;  // use size_t to avoid int overflow

// others (xindex)
// size_t bg_n = 1;

// Packet types used by switchos.paramserver and ptf framework
/*int SWITCHOS_GET_FREEIDX = -1; // ptf get freeidx from paramserver
int SWITCHOS_GET_KEY_FREEIDX = -1; // ptf get key and freeidx from paramserver
int SWITCHOS_SET_EVICTDATA = -1; // ptf set evictidx, evictvallen, evictval, evictstat, and evictseq to paramserver
int SWITCHOS_GET_EVICTKEY = -1; // ptf get evictkey
int SWITCHOS_GET_CACHEDEMPTYINDEX = -1; // ptf get cached_empty_index*/
// int SWITCHOS_SETVALID0 = -1;
// int SWITCHOS_SETVALID0_ACK = -1;
// int SWITCHOS_ADD_CACHE_LOOKUP_SETVALID1 = -1;
// int SWITCHOS_ADD_CACHE_LOOKUP_SETVALID1_ACK = -1;
int SWITCHOS_ADD_CACHE_LOOKUP = -1;
int SWITCHOS_ADD_CACHE_LOOKUP_ACK = -1;
int CACHE_FREQUENCYREQ = -1;
int CACHE_FREQUENCYACK = -1;
int SETDELETEDREQ = -1;
int SETDELETEDACK = -1;
// int SWITCHOS_GET_EVICTDATA_SETVALID3 = -1;
// int SWITCHOS_GET_EVICTDATA_SETVALID3_ACK = -1;
// int SWITCHOS_SETVALID3 = -1;
// int SWITCHOS_SETVALID3_ACK = -1;
int SWITCHOS_REMOVE_CACHE_LOOKUP = -1;
int SWITCHOS_REMOVE_CACHE_LOOKUP_ACK = -1;
int SWITCHOS_CLEANUP = -1;
int SWITCHOS_CLEANUP_ACK = -1;
int SWITCHOS_ENABLE_SINGLEPATH = -1;
int SWITCHOS_ENABLE_SINGLEPATH_ACK = -1;
int SWITCHOS_SET_SNAPSHOT_FLAG = -1;
int SWITCHOS_SET_SNAPSHOT_FLAG_ACK = -1;
int SWITCHOS_DISABLE_SINGLEPATH = -1;
int SWITCHOS_DISABLE_SINGLEPATH_ACK = -1;
int SWITCHOS_LOAD_SNAPSHOT_DATA = -1;
int SWITCHOS_LOAD_SNAPSHOT_DATA_ACK = -1;
int SWITCHOS_RESET_SNAPSHOT_FLAG_AND_REG = -1;
int SWITCHOS_RESET_SNAPSHOT_FLAG_AND_REG_ACK = -1;
int SWITCHOS_PTF_POPSERVER_END = -1;
int SWITCHOS_PTF_SNAPSHOTSERVER_END = -1;

// Packet types used by switchos/controller/server for snapshot
int SNAPSHOT_CLEANUP = -1;
int SNAPSHOT_CLEANUP_ACK = -1;
int SNAPSHOT_PREPARE = -1;
int SNAPSHOT_PREPARE_ACK = -1;
int SNAPSHOT_SETFLAG = -1;
int SNAPSHOT_SETFLAG_ACK = -1;
int SNAPSHOT_START = -1;
int SNAPSHOT_START_ACK = -1;
int SNAPSHOT_GETDATA = -1;
int SNAPSHOT_GETDATA_ACK = -1;
int SNAPSHOT_SENDDATA = -1;
int SNAPSHOT_SENDDATA_ACK = -1;
int LEAF_SPINE_SYNC_FLAG = 1;
/*
 * Get configuration
 */

inline void parse_ini(const char* config_file, uint32_t rack_idx);
inline void parse_control_ini(const char* config_file);
void free_common();

inline void parse_ini(const char* config_file, uint32_t rack_idx = 0) {
    IniparserWrapper ini;
    ini.load(config_file);

    // global
    workload_name = ini.get_workload_name();
    workload_mode = ini.get_workload_mode();
    dynamic_periodnum = ini.get_dynamic_periodnum();
    dynamic_periodinterval = ini.get_dynamic_periodinterval();
    dynamic_ruleprefix = ini.get_dynamic_ruleprefix();
    val_t::MAX_VALLEN = ini.get_max_vallen();

    client_physical_num = ini.get_client_physical_num();
    server_physical_num = ini.get_server_physical_num();
    client_total_logical_num = ini.get_client_total_logical_num();
    server_total_logical_num = ini.get_server_total_logical_num();
    server_total_logical_num_for_rotation = ini.get_server_total_logical_num_for_rotation();
#ifdef SERVER_ROTATION
    max_server_total_logical_num = server_total_logical_num_for_rotation;
#else
    max_server_total_logical_num = server_total_logical_num;
#endif
    bottleneck_serveridx_for_rotation = ini.get_bottleneck_serveridx_for_rotation();
#ifdef SERVER_ROTATION
    INVARIANT(bottleneck_serveridx_for_rotation < server_total_logical_num_for_rotation);
#endif
    ParserIterator::load_batch_size = ini.get_load_batch_size() * server_physical_num / 2;

    if (workload_mode == 0) {  // static workload
#ifndef SERVER_ROTATION
        printf("[ERROR] you should enable SERVER_ROTATION in common/helper.h for static workload mode\n");
        exit(-1);
#endif
    } else {
#ifdef SERVER_ROTATION
        printf("[ERROR] you should disable SERVER_ROTATION in common/helper.h for dynamic workload mode\n");
        exit(-1);
#endif
    }

    printf("workload_name: %s\n", workload_name);
    COUT_VAR(workload_mode);
    COUT_VAR(dynamic_periodnum);
    COUT_VAR(dynamic_periodinterval);
    printf("dynamic_ruleprefix: %s\n", dynamic_ruleprefix);
    COUT_VAR(val_t::MAX_VALLEN);
    COUT_VAR(ParserIterator::load_batch_size);
    COUT_VAR(client_physical_num);
    COUT_VAR(server_physical_num);
    COUT_VAR(client_total_logical_num);
    COUT_VAR(server_total_logical_num);
    COUT_VAR(server_total_logical_num_for_rotation);
    COUT_VAR(max_server_total_logical_num);
    printf("\n");

    // common client configuration
    client_rotationdataserver_port = ini.get_client_rotationdataserver_port();
    client_sendpktserver_port_start = ini.get_client_sendpktserver_port_start();
    client_rulemapserver_port_start = ini.get_client_rulemapserver_port_start();
    client_worker_port_start = ini.get_client_worker_port_start();
    COUT_VAR(client_rotationdataserver_port);
    COUT_VAR(client_sendpktserver_port_start);
    COUT_VAR(client_rulemapserver_port_start);
    COUT_VAR(client_worker_port_start);
    printf("\n");

    // each physical client
    int tmp_client_total_logical_num = 0;
    for (uint32_t client_physical_idx = 0; client_physical_idx < client_physical_num; client_physical_idx++) {
        client_logical_nums.push_back(ini.get_client_logical_num(client_physical_idx));
        tmp_client_total_logical_num += client_logical_nums[client_physical_idx];
        client_ips.push_back(ini.get_client_ip(client_physical_idx));
        uint8_t* tmp_client_mac = new uint8_t[6];
        ini.get_client_mac(tmp_client_mac, client_physical_idx);
        client_macs.push_back(tmp_client_mac);
        client_fpports.push_back(ini.get_client_fpport(client_physical_idx));
        client_pipeidxes.push_back(ini.get_client_pipeidx(client_physical_idx));
        client_ip_for_client0_list.push_back(ini.get_client_ip_for_client0(client_physical_idx));

        printf("client_logical_nums[%d]: %d\n", client_physical_idx, client_logical_nums[client_physical_idx]);
        printf("client_ips[%d]: %s\n", client_physical_idx, client_ips[client_physical_idx]);
        printf("client_macs[%d]: ", client_physical_idx);
        dump_macaddr(client_macs[client_physical_idx]);
        printf("client_fpports[%d]: %s\n", client_physical_idx, client_fpports[client_physical_idx]);
        printf("client_pipeidxes[%d]: %d\n", client_physical_idx, client_pipeidxes[client_physical_idx]);
    }
    INVARIANT(tmp_client_total_logical_num == client_total_logical_num);
    printf("\n");

    // server common configuration
    // server: loading phase for loader instead of ycsb benchmark
    server_load_factor = ini.get_server_load_factor();
    // server: transaction phase
    server_worker_port_start = ini.get_server_worker_port_start();
    server_evictserver_port_start = ini.get_server_evictserver_port_start();
    server_popserver_port_start = ini.get_server_popserver_port_start();
    server_valueupdateserver_port_start = ini.get_server_valueupdateserver_port_start();
    transaction_loadfinishserver_port = ini.get_transaction_loadfinishserver_port();

    COUT_VAR(server_load_factor);
    COUT_VAR(server_worker_port_start);
    COUT_VAR(server_evictserver_port_start);
    COUT_VAR(server_popserver_port_start);
    COUT_VAR(server_valueupdateserver_port_start);
    COUT_VAR(transaction_loadfinishserver_port);
    printf("\n");

    // each physical server
    int tmp_server_total_logical_num = 0;
    for (size_t server_physical_idx = 0; server_physical_idx < server_physical_num; server_physical_idx++) {
        server_worker_corenums.push_back(ini.get_server_worker_corenum(server_physical_idx));
        server_total_corenums.push_back(ini.get_server_total_corenum(server_physical_idx));
        server_logical_idxes_list.push_back(ini.get_server_logical_idxes(server_physical_idx));
        if (server_worker_corenums[server_physical_idx] > server_total_corenums[server_physical_idx]) {
            printf("[ERROR] server[%d] worker corenum %d must <= total corenum %d\n", server_physical_idx, server_worker_corenums[server_physical_idx], server_total_corenums[server_physical_idx]);
            exit(-1);
        }
        if (server_worker_corenums[server_physical_idx] < server_logical_idxes_list[server_physical_idx].size()) {
            printf("[ERROR] server[%d] worker corenum %d < thread num %d, which could incur CPU contention!\n", server_physical_idx, server_worker_corenums[server_physical_idx], server_logical_idxes_list[server_physical_idx].size());
        }
        for (size_t i = 0; i < server_logical_idxes_list[server_physical_idx].size(); i++) {
            if (server_logical_idxes_list[server_physical_idx][i] >= max_server_total_logical_num) {
                printf("[ERROR] server logical idx %d cannot >= max_server_total_logical_num %d\n", server_logical_idxes_list[server_physical_idx][i], max_server_total_logical_num);
                exit(-1);
            }
        }
        tmp_server_total_logical_num += server_logical_idxes_list[server_physical_idx].size();
        server_ips.push_back(ini.get_server_ip(server_physical_idx));
        uint8_t* tmp_server_mac = new uint8_t[6];
        ini.get_server_mac(tmp_server_mac, server_physical_idx);
        server_macs.push_back(tmp_server_mac);
        server_fpports.push_back(ini.get_server_fpport(server_physical_idx));
        server_pipeidxes.push_back(ini.get_server_pipeidx(server_physical_idx));
        server_ip_for_controller_list.push_back(ini.get_server_ip_for_controller(server_physical_idx));

        printf("server_worker_corenums[%d]: %d\n", server_physical_idx, server_worker_corenums[server_physical_idx]);
        printf("server_total_corenums[%d]: %d\n", server_physical_idx, server_total_corenums[server_physical_idx]);
        printf("server_logical_idxes_list[%d]: ", server_physical_idx);
        for (size_t i = 0; i < server_logical_idxes_list[server_physical_idx].size(); i++) {
            printf("%d ", server_logical_idxes_list[server_physical_idx][i]);
        }
        printf("\n");
        printf("server_ips[%d]: %s\n", server_physical_idx, server_ips[server_physical_idx]);
        printf("server_macs[%d]: ", server_physical_idx);
        dump_macaddr(server_macs[server_physical_idx]);
        printf("server_fpports[%d]: %s\n", server_physical_idx, server_fpports[server_physical_idx]);
        printf("server_pipeidxes[%d]: %d\n", server_physical_idx, server_pipeidxes[server_physical_idx]);
        printf("server_ip_for_controller_list[%d]: %s\n", server_physical_idx, server_ip_for_controller_list[server_physical_idx]);
    }
    INVARIANT(tmp_server_total_logical_num == server_total_logical_num);
    printf("\n");

    // controller
    controller_ip_for_server = ini.get_controller_ip_for_server(rack_idx);
    controller_ip_for_switchos = ini.get_controller_ip_for_switchos(rack_idx);
    controller_popserver_port_start = ini.get_controller_popserver_port_start(rack_idx);
    controller_evictserver_port = ini.get_controller_evictserver_port(rack_idx);
    controller_snapshot_period = ini.get_controller_snapshot_period(rack_idx);
    controller_warmupfinishserver_port = ini.get_controller_warmupfinishserver_port(rack_idx);

    printf("controller ip for server: %s\n", controller_ip_for_server);
    printf("controller ip for switchos: %s\n", controller_ip_for_switchos);
    COUT_VAR(controller_popserver_port_start);
    COUT_VAR(controller_evictserver_port);
    COUT_VAR(controller_snapshot_period);
    COUT_VAR(controller_warmupfinishserver_port);
    printf("\n");

    // switch
    switch_partition_count = ini.get_switch_partition_count(rack_idx);
    switch_kv_bucket_num = ini.get_switch_kv_bucket_num(rack_idx);
    switch_pipeline_num = ini.get_switch_pipeline_num(rack_idx);
    val_t::SWITCH_MAX_VALLEN = ini.get_switch_max_vallen(rack_idx);
    switchos_ip = ini.get_switchos_ip(rack_idx);
    switchos_sample_cnt = ini.get_switchos_sample_cnt(rack_idx);
    switchos_popserver_port = ini.get_switchos_popserver_port(rack_idx);
    // switchos_snapshotserver_port = ini.get_switchos_snapshotserver_port(rack_idx);
    switchos_specialcaseserver_port = ini.get_switchos_specialcaseserver_port(rack_idx);
    switchos_ptf_popserver_port = ini.get_switchos_ptf_popserver_port(rack_idx);
    switchos_ptf_snapshotserver_port = ini.get_switchos_ptf_snapshotserver_port(rack_idx);

    // spineswitch
    leafswitchos_ips = ini.get_leafswitchos_ips(server_physical_num / 2);
    spineswitchos_ips = ini.get_spineswitchos_ips(server_physical_num / 2);
    spineswitchos_ip = spineswitchos_ips[rack_idx];
    // validate pipeidxes of clients and servers
    for (size_t i = 0; i < client_physical_num; i++) {
        INVARIANT(client_pipeidxes[i] >= 0 && client_pipeidxes[i] < switch_pipeline_num);
    }
    for (size_t i = 0; i < server_physical_num; i++) {
        INVARIANT(server_pipeidxes[i] >= 0 && server_pipeidxes[i] < switch_pipeline_num);
    }

    COUT_VAR(switch_partition_count);
    COUT_VAR(switch_kv_bucket_num);
    COUT_VAR(switch_pipeline_num);
    COUT_VAR(val_t::SWITCH_MAX_VALLEN);
    COUT_VAR(switchos_popserver_port);
    printf("switchos ip: %s\n", switchos_ip);
    COUT_VAR(switchos_sample_cnt);
    // COUT_VAR(switchos_snapshotserver_port);
    COUT_VAR(switchos_specialcaseserver_port);
    COUT_VAR(switchos_ptf_popserver_port);
    COUT_VAR(switchos_ptf_snapshotserver_port);
    if (switchos_sample_cnt > switch_kv_bucket_num) {
        switchos_sample_cnt = switch_kv_bucket_num;
        printf("[WARNING] switchos_sample_cnt > switch_kv_bucket_num, set switchos_sample_cnt as switch_kv_bucket_num\n");
    }
    printf("\n");

    // reflector
    reflector_ip_for_switchos = ini.get_reflector_ip_for_switchos(rack_idx);
    reflector_dp2cpserver_port = ini.get_reflector_dp2cpserver_port(rack_idx);
    reflector_cp2dpserver_port = ini.get_reflector_cp2dpserver_port(rack_idx);

    printf("reflector ip for switchos: %s\n", reflector_ip_for_switchos);
    COUT_VAR(reflector_dp2cpserver_port);
    COUT_VAR(reflector_cp2dpserver_port);
    printf("\n");

    // calculated metadata

    LOAD_RAW_WORKLOAD(raw_load_workload_filename, workload_name);
    LOAD_SPLIT_DIR(server_load_workload_dir, workload_name, max_server_total_logical_num);  // get the split directory for loading phase
    WARMUP_RAW_WORKLOAD(raw_warmup_workload_filename, workload_name);
    RUN_RAW_WORKLOAD(raw_run_workload_filename, workload_name);
    RUN_SPLIT_DIR(client_workload_dir, workload_name, client_total_logical_num);
    // max_sending_rate *= server_num;
    // per_client_per_period_max_sending_rate = max_sending_rate / client_num / (1 * 1000 * 1000 / rate_limit_period);
    perserver_keyrange = 64 * 1024 / max_server_total_logical_num;  // 2^16 / server_num

    printf("raw_load_workload_filename for loading phase: %s\n", raw_load_workload_filename);
    printf("server_load_workload_dir for loading phase: %s\n", server_load_workload_dir);
    printf("raw_warmup_workload_filename for warmup phase: %s\n", raw_warmup_workload_filename);
    printf("raw_run_workload_filename for transaction phase: %s\n", raw_run_workload_filename);
    printf("client_workload_dir for transaction phase: %s\n", client_workload_dir);
    COUT_VAR(perserver_keyrange);
    printf("\n");
}

inline void parse_control_ini(const char* config_file) {
    IniparserWrapper ini;
    ini.load(config_file);

    /*SWITCHOS_GET_FREEIDX = ini.get_switchos_get_freeidx();
    SWITCHOS_GET_KEY_FREEIDX = ini.get_switchos_get_key_freeidx();
    SWITCHOS_SET_EVICTDATA = ini.get_switchos_set_evictdata();
    SWITCHOS_GET_EVICTKEY = ini.get_switchos_get_evictkey();
    SWITCHOS_GET_CACHEDEMPTYINDEX = ini.get_switchos_get_cachedemptyindex();
    SNAPSHOT_START = ini.get_snapshot_start();
    SNAPSHOT_SERVERSIDE = ini.get_snapshot_serverside();
    SNAPSHOT_SERVERSIDE_ACK = ini.get_snapshot_serverside_ack();
    SNAPSHOT_DATA = ini.get_snapshot_data();
    COUT_VAR(SWITCHOS_GET_FREEIDX);
    COUT_VAR(SWITCHOS_GET_KEY_FREEIDX);
    COUT_VAR(SWITCHOS_SET_EVICTDATA);
    COUT_VAR(SWITCHOS_GET_EVICTKEY);
    COUT_VAR(SWITCHOS_GET_CACHEDEMPTYINDEX);
    COUT_VAR(SNAPSHOT_START);
    COUT_VAR(SNAPSHOT_SERVERSIDE);
    COUT_VAR(SNAPSHOT_SERVERSIDE_ACK);
    COUT_VAR(SNAPSHOT_DATA);*/

    // SWITCHOS_SETVALID0 = ini.get_switchos_setvalid0();
    // SWITCHOS_SETVALID0_ACK = ini.get_switchos_setvalid0_ack();
    // SWITCHOS_ADD_CACHE_LOOKUP_SETVALID1 = ini.get_switchos_add_cache_lookup_setvalid1();
    // SWITCHOS_ADD_CACHE_LOOKUP_SETVALID1_ACK = ini.get_switchos_add_cache_lookup_setvalid1_ack();
    SWITCHOS_ADD_CACHE_LOOKUP = ini.get_switchos_add_cache_lookup();
    SWITCHOS_ADD_CACHE_LOOKUP_ACK = ini.get_switchos_add_cache_lookup_ack();
    CACHE_FREQUENCYREQ = 10086;
    CACHE_FREQUENCYACK = 10087;
    SETDELETEDREQ = 10088;
    SETDELETEDACK = 10089;

    // SWITCHOS_GET_EVICTDATA_SETVALID3 = ini.get_switchos_get_evictdata_setvalid3();
    // SWITCHOS_GET_EVICTDATA_SETVALID3_ACK = ini.get_switchos_get_evictdata_setvalid3_ack();
    // SWITCHOS_SETVALID3 = ini.get_switchos_setvalid3();
    // SWITCHOS_SETVALID3_ACK = ini.get_switchos_setvalid3_ack();
    SWITCHOS_REMOVE_CACHE_LOOKUP = ini.get_switchos_remove_cache_lookup();
    SWITCHOS_REMOVE_CACHE_LOOKUP_ACK = ini.get_switchos_remove_cache_lookup_ack();
    SWITCHOS_CLEANUP = ini.get_switchos_cleanup();
    SWITCHOS_CLEANUP_ACK = ini.get_switchos_cleanup_ack();
    SWITCHOS_ENABLE_SINGLEPATH = ini.get_switchos_enable_singlepath();
    SWITCHOS_ENABLE_SINGLEPATH_ACK = ini.get_switchos_enable_singlepath_ack();
    SWITCHOS_SET_SNAPSHOT_FLAG = ini.get_switchos_set_snapshot_flag();
    SWITCHOS_SET_SNAPSHOT_FLAG_ACK = ini.get_switchos_set_snapshot_flag_ack();
    SWITCHOS_DISABLE_SINGLEPATH = ini.get_switchos_disable_singlepath();
    SWITCHOS_DISABLE_SINGLEPATH_ACK = ini.get_switchos_disable_singlepath_ack();
    SWITCHOS_LOAD_SNAPSHOT_DATA = ini.get_switchos_load_snapshot_data();
    SWITCHOS_LOAD_SNAPSHOT_DATA_ACK = ini.get_switchos_load_snapshot_data_ack();
    SWITCHOS_RESET_SNAPSHOT_FLAG_AND_REG = ini.get_switchos_reset_snapshot_flag_and_reg();
    SWITCHOS_RESET_SNAPSHOT_FLAG_AND_REG_ACK = ini.get_switchos_reset_snapshot_flag_and_reg_ack();
    SWITCHOS_PTF_POPSERVER_END = ini.get_switchos_ptf_popserver_end();
    SWITCHOS_PTF_SNAPSHOTSERVER_END = ini.get_switchos_ptf_snapshotserver_end();

    SNAPSHOT_CLEANUP = ini.get_snapshot_cleanup();
    SNAPSHOT_CLEANUP_ACK = ini.get_snapshot_cleanup_ack();
    SNAPSHOT_PREPARE = ini.get_snapshot_prepare();
    SNAPSHOT_PREPARE_ACK = ini.get_snapshot_prepare_ack();
    SNAPSHOT_SETFLAG = ini.get_snapshot_setflag();
    SNAPSHOT_SETFLAG_ACK = ini.get_snapshot_setflag_ack();
    SNAPSHOT_START = ini.get_snapshot_start();
    SNAPSHOT_START_ACK = ini.get_snapshot_start_ack();
    SNAPSHOT_GETDATA = ini.get_snapshot_getdata();
    SNAPSHOT_GETDATA_ACK = ini.get_snapshot_getdata_ack();
    SNAPSHOT_SENDDATA = ini.get_snapshot_senddata();
    SNAPSHOT_SENDDATA_ACK = ini.get_snapshot_senddata_ack();
}

void free_common() {
    for (uint32_t client_physical_idx = 0; client_physical_idx < client_physical_num; client_physical_idx++) {
        if (client_macs[client_physical_idx] != NULL) {
            delete[] client_macs[client_physical_idx];
            client_macs[client_physical_idx] = NULL;
        }
    }
    for (uint32_t server_physical_idx = 0; server_physical_idx < server_physical_num; server_physical_idx++) {
        if (server_macs[server_physical_idx] != NULL) {
            delete[] server_macs[server_physical_idx];
            server_macs[server_physical_idx] = NULL;
        }
    }
}

#endif

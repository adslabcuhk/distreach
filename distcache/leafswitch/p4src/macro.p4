
// Uncomment it if support range query, or comment it otherwise
// Change netcache.p4, common.py, and helper.h accordingly
//#define RANGE_SUPPORT

// Uncomment it before evaluation
// NOTE: update config.ini accordingly
//#define DEBUG

// NOTE: 1B optype does not need endian conversion
// 0b0001
#define PUTREQ 0x0001
//#define WARMUPREQ 0x0011
//#define LOADREQ 0x0021
// 0b0011
#define PUTREQ_SEQ 0x0003
#define PUTREQ_POP_SEQ 0x0013
#define PUTREQ_SEQ_CASE3 0x0023
#define PUTREQ_POP_SEQ_CASE3 0x0033
#define NETCACHE_PUTREQ_SEQ_CACHED 0x0043
#define PUTREQ_SEQ_BEINGEVICTED 0x0053
#define PUTREQ_SEQ_CASE3_BEINGEVICTED 0x0063
#define PUTREQ_SEQ_BEINGEVICTED_SPINE 0x0073
#define PUTREQ_SEQ_CASE3_BEINGEVICTED_SPINE 0x0083
// 0b0110
#define DELREQ_SEQ_INSWITCH 0x0006
// For large value
#define PUTREQ_LARGEVALUE_SEQ_INSWITCH 0x0016
// 0b0111
// 0b1111
#define GETRES_LATEST_SEQ_INSWITCH 0x000f
#define GETRES_DELETED_SEQ_INSWITCH 0x001f
#define GETRES_LATEST_SEQ_INSWITCH_CASE1 0x002f
#define GETRES_DELETED_SEQ_INSWITCH_CASE1 0x003f
#define PUTREQ_SEQ_INSWITCH_CASE1 0x004f
#define DELREQ_SEQ_INSWITCH_CASE1 0x005f
#define LOADSNAPSHOTDATA_INSWITCH_ACK 0x006f
#define CACHE_POP_INSWITCH 0x007f
#define CACHE_POP_INSWITCH_SPINE 0x017f
#define NETCACHE_VALUEUPDATE_INSWITCH 0x008f
// For large value
#define NETCACHE_CACHE_POP_INSWITCH_NLATEST 0x015f
// 0b1011
#define GETRES_LATEST_SEQ 0x000b
#define GETRES_DELETED_SEQ 0x001b
#define CACHE_EVICT_LOADDATA_INSWITCH_ACK 0x002b
#define NETCACHE_VALUEUPDATE 0x003b
// 0b1001
#define GETRES 0x09
// 0b0101
#define PUTREQ_INSWITCH 0x0005
// 0b0100
#define GETREQ_INSWITCH 0x0004
#define DELREQ_INSWITCH 0x0014
#define CACHE_EVICT_LOADFREQ_INSWITCH 0x0024
#define CACHE_EVICT_LOADDATA_INSWITCH 0x0034
#define LOADSNAPSHOTDATA_INSWITCH 0x0044
#define SETVALID_INSWITCH 0x0054
#define NETCACHE_WARMUPREQ_INSWITCH 0x0064
#define NETCACHE_WARMUPREQ_INSWITCH_POP 0x0074
// For large value
#define PUTREQ_LARGEVALUE_INSWITCH 0x00a4
// 0b0010
#define DELREQ_SEQ 0x0002
#define DELREQ_SEQ_CASE3 0x0012
#define NETCACHE_DELREQ_SEQ_CACHED 0x0022
// For large value (PUTREQ_LARGEVALUE_SEQ_CACHED ONLY for netcache/distcache; PUTREQ_LARGEVALUE_SEQ_CASE3 ONLY for farreach/distfarreach)
#define PUTREQ_LARGEVALUE_SEQ 0x0032
#define PUTREQ_LARGEVALUE_SEQ_CACHED 0x0042
#define PUTREQ_LARGEVALUE_SEQ_CASE3 0x0052
// For read blocking under cache eviction rare case
#define DELREQ_SEQ_BEINGEVICTED 0x0062
#define DELREQ_SEQ_CASE3_BEINGEVICTED 0x0072
#define PUTREQ_LARGEVALUE_SEQ_BEINGEVICTED 0x0082
#define PUTREQ_LARGEVALUE_SEQ_CASE3_BEINGEVICTED 0x0092
#define DELREQ_SEQ_BEINGEVICTED_SPINE 0x00a2
#define DELREQ_SEQ_CASE3_BEINGEVICTED_SPINE 0x00b3
#define PUTREQ_LARGEVALUE_SEQ_BEINGEVICTED_SPINE 0x00c2
#define PUTREQ_LARGEVALUE_SEQ_CASE3_BEINGEVICTED_SPINE 0x00d2
// 0b1000
#define PUTRES 0x0008
#define DELRES 0x0018
// 0b0000
#define WARMUPREQ 0x0000
#define SCANREQ 0x0010
#define SCANREQ_SPLIT 0x0020
#define GETREQ 0x0030
#define DELREQ 0x0040
#define GETREQ_POP 0x0050
#define GETREQ_NLATEST 0x0060
#define CACHE_POP_INSWITCH_ACK 0x0070
#define SCANRES_SPLIT 0x0080
#define CACHE_POP 0x0090
#define CACHE_EVICT 0x00a0
#define CACHE_EVICT_ACK 0x00b0
#define CACHE_EVICT_CASE2 0x00c0
#define WARMUPACK 0x00d0
#define LOADACK 0x00e0
#define CACHE_POP_ACK 0x00f0
#define CACHE_EVICT_LOADFREQ_INSWITCH_ACK 0x0100
#define SETVALID_INSWITCH_ACK 0x0110
#define NETCACHE_GETREQ_POP 0x0120
// NOTE: NETCACHE_CACHE_POP/_ACK, NETCACHE_CACHE_POP_FINISH/_ACK, NETCACHE_CACHE_EVICT/_ACK only used by end-hosts
#define NETCACHE_CACHE_POP 0x0130
#define NETCACHE_CACHE_POP_ACK 0x0140
#define NETCACHE_CACHE_POP_FINISH 0x0150
#define NETCACHE_CACHE_POP_FINISH_ACK 0x0160
#define NETCACHE_CACHE_EVICT 0x0170
#define NETCACHE_CACHE_EVICT_ACK 0x0180
#define NETCACHE_VALUEUPDATE_ACK 0x0190
// For large value (NETCACHE_CACHE_POP_ACK_NLATEST is ONLY used by end-hosts)
#define PUTREQ_LARGEVALUE 0x02d0
#define DISTNOCACHE_PUTREQ_LARGEVALUE_SPINE 0x02e0
#define GETRES_LARGEVALUE_SERVER 0x02f0
#define GETRES_LARGEVALUE 0x0300
#define LOADREQ 0x0310
#define LOADREQ_SPINE 0x0320
#define NETCACHE_CACHE_POP_ACK_NLATEST 0x0330
#define GETREQ_BEINGEVICTED 0x0340

#ifndef DEBUG

// NOTE: limited by 12 stages and 64*4B PHV (not T-PHV) (fields in the same ALU must be in the same PHV group)
// 32K * (2B vallen + 128B value + 4B frequency + 1B status)
#define KV_BUCKET_COUNT 32768
//#define KV_BUCKET_COUNT 16384
// 64K * 2B counter
#define CM_BUCKET_COUNT 65536
// 32K * 4B counter
#define SEQ_BUCKET_COUNT 32768
// 256K * 1b counter
#define BF_BUCKET_COUNT 262144

#else

#define KV_BUCKET_COUNT 1
#define CM_BUCKET_COUNT 1
#define SEQ_BUCKET_COUNT 1
#define BF_BUCKET_COUNT 1

#endif

// hot_threshold=10 + sampling_ratio=0.5 -> hot_pktcnt=20 during each clean period (NOTE: cached key will not update CM)
// NOTE: it can be reconfigured by MAT
#define DEFAULT_HH_THRESHOLD 10

// egress_pipeline_num * kv_bucket_count
//#define LOOKUP_ENTRY_COUNT 65536
#define LOOKUP_ENTRY_COUNT 32768

// MAX_SERVER_NUM <= 128
#define MAX_SERVER_NUM 128
// RANGE_PARTITION_ENTRY_NUM = 10 * MAX_SERVER_NUM < 16 * MAX_SERVER_NUM
#define RANGE_PARTITION_ENTRY_NUM 2048
// RANGE_PARTITION_FOR_SCAN_ENDKEY_ENTRY_NUM = 1 * MAX_SERVER_NUM
#define RANGE_PARTITION_FOR_SCAN_ENDKEY_ENTRY_NUM 128
// PROCESS_SCANREQ_SPLIT_ENTRY_NUM = 2 * MAX_SERVER_NUM
#define PROCESS_SCANREQ_SPLIT_ENTRY_NUM 256
// HASH_PARTITION_ENTRY_NUM = 9 * MAX_SERVER_NUM < 16 * MAX_SERVER_NUM
#define HASH_PARTITION_ENTRY_NUM 2048

// hash partition range
#define PARTITION_COUNT 32768

#define SWITCHIDX_FOREVAL 0xFFFF

//#define CPU_PORT 192
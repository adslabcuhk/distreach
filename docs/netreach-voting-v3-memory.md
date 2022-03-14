# Tofino-based NetREACH (voting-based) + DPDK-based XIndex (in-memory KVS) + variable-length key-value pair w/ snapshot (netreach-voting-v3-memory)

- Client calculates op_hdr.hashidx as slot index to reduce 1 stage (similar as OmniMon)
	+ We calculate meta.hashidx for hash partition in switch
- Offload some functions to egress pipeline to reduce stages
- Using 4B for each unit of key instead of 2B to reduce 1 stage
- TODO: If with IP-level fragmentation
	+ Configure MTU
	+ Only process the first fragmented packet

## Overview

- Design features
	+ Parameter-free decision
		* Existing: large parameter -> miss hot keys; small parameter -> too many hot keys -> insufficient cache capacity and switch OS bottleneck
		* Challenge: Slow warmup with incast populations -> data-plane-based cache population
			- NOTE: Mirroring GETRES_POP_EVICT/PUTREQ_POP_EVICT does not incur switch OS bottleneck since we do not need send old key-value pair
			to server if the entry is invalid during warmup phase
	+ Data-plane-based cache update
		* Data-plane-based value update
		* Data-plane-based cache population
			- GETREQ: response-based cache population
			- PUTREQ: recirculation-based cache population
			- Atomicity: 
				+ Use lock bit + recirculation-based blocking
				+ Use seq to cope with packet reordering of PUT
				+ Controller periodically checks lock bit to cope with rare packet loss
			- NOTE: recirculation is acceptable
				+ We do not block normal packets
				+ NetReach packets must be hashed into the same slot with different keys of currently cached one
				+ Population time is limited: us-level for GET and ns-level for PUT
				+ # of blocked packets is limited by # of clients (e.g., one thread in a host) due to one-by-one pattern of NetReach client library
			- NOTE: recirculation-based blocking guarantees that version of switch >= server
	+ Crash-consistent backup
		* Case 1: change on in-switch cache value;
		* Case 2: change on in-switch cache key-value pair;
		* Case 3: change on server KVS for range query;
	+ CBF-based fast path
		* In LSM-based KVS, as data is very large, we can only maintain CBF for buffer and buffer_temp
		* Now we focus on in-memory KVS where data can fit into memory, so we can directly maintain CBF for data array
			- As we keep group merge/split and model merge/split, we cannot free data, buffer, and CBF in deconstructor
	+ TODO: Others
		* Range query support
		* Switch-driven consistent hashing
		* Distributed extension
- Term routine
	+ Match: just compare without 
	+ Get: load to change packet header field (or metadata field)
	+ Set: store new value without changing packet header field
	+ Set_and_get: store new value and change packet header field with old value
	+ Reset: reset value as 0
	+ Init: initialize value as 1
- NOTES
	+ TODO: we need to verify the liimted bandwidth overhead of switch OS (case1, mirrored evict/case2, backup data)
	+ TODO: we need to verify the limited recirculation rate due to data-plane-based eviction
		* We can clone a pkt for each recirculation to cound the # of recirculations, then divide it with # of pkts
		* Note that one packet has only one recirculation if locked by PUTREQ_POP, while multiple recirculations if by GETRES_POP

## Details 

- Packet format
	+ op_hdr: 1B optype, 2B hashidx, 16B key
	+ val_hdr: 4B vallen, variable-length value
		* If value <= 128B, align with 8B (only when serializing/deserializing to/from packet, invisible to client)
		* If value > 128B, do not need alignment
- Client-side processing
	+ Client side calculates hash into op_hdr.hashidx
	+ Only GET and PUT are related with value length, while DEL does not care
		* GET is processed by server for GETREQ_POP: if value>128B, return GETRES_POP_LARGE that behaves similar as GETRES_NPOP in switch
		* TODO: For PUT, client sends PUTREQ_LARGE if value>128B, which does not update vote and may invalidate cached item in switch
			- If with invalidation (i.e., becoming PUTREQ_LARGE_EVICT and clone a PUTREQ_LARGE), PUTREQ_LARGE_EVICT will be mirrored to switch OS to cope with packet loss
- In-switch processing
	+ NOTE: clone_i2e clones the original packet to egress pipeline; clone_e2e clones the deparsed packet to egress pipeline
	+ Overview
		* Stage 0: keylolo, keylohi, keyhilo, keyhihi, load_backup_tbl
			- For key, we provide two operations: match, set_and_get
		* Stage 1: valid, vote, seq (assign for each PUTREQ/DELREQ/PUTREQ_LARGE), update_iskeymatch_tbl
			- TODO: To avoid seq overflow, we can maintain an array of seq, each corresponds to a value in the hash range of key (i.e. a hash slot)
				+ It guarantees that for each key, seq always increases (server uses 0 as initial value, while switch increases from 1)
				+ Another way is to use more bits and stages for seq (we do not use due to stage limitation)
					* For example, we can use a 64-bit register array for seq (register_hi for highest 32 bits, while register_lo for lowest 32 bits)
					* Due to lack of comparator within each ALU, we must maintain savedseq_hi and savedseq_lo in two 32-bit register arrays, which wastes stage
				+ TODO: both GETRES_POP, PUTREQ_POP, and evicted packets needs to set savedseq correspondingly
					* TODO: Note that for GETRES_POP which carries savedseq from server, if new PUT/DEL arrives at switch after GETRES_POP is applied into switch, the assigned seq must be larger than that carried from server
				+ TODO: server also needs to save the seq number, which should be invisible to client
				+ TODO: For PUTREQ_LARGE, assign seq to meta.seq instead of seq_hdr.seq
				+ TODO: For PUTREQ_LARGE_RECIR, set meta.seq = seq_hdr.seq
		* Stage 2: savedseq, lock 
			- For PUTREQ/DELREQ/PUTREQ_RECIR/DELREQ_RECIR, if iskeymatch=1, try_update_savedseq: if seq<=savedseq, directly send back response; otherwise, update vallen and value before sending back response
			- TODO: For PUTREQ_POP/DELREQ_POP, set_and_get_savedseq: set savedseq as embedded one, get old savedseq for possible eviction
				+ TODO: NOTE: as cloned packet does not need seq, both set and get can happen on seq_hdr
			- TODO: For PUTREQ_LARGE/_RECIR, if iskeymatch=1 and valid=1, get_savedseq_large: get savedseq to seq_hdr 
				+ NOTE: PUTREQ_LARGE/_RECIR always evicts cached record if any even if seq<=savedseq due to stage limitation

		* Stage 3: vallen, vallo1, valhi1, case12
			- For vallen and val, we provide two operations: get, set_and_get
		* Stage 4-10: from val2 to val15
		* Stage 11: vallo16, valhi16, ~~case3~~, port_forward_tbl
		* Egress pipeline
			- process_cloned_packet_tbl for cloned packet 
				+ If PUTREQ/DELREQ/PUTREQ_RECIR/DELREQ_RECIR, update as PUT/DELRES/PUTRES to client
				+ If PUTREQ_LARGE_RECIR, update as PUTREQ_LARGE to server
					* NOTE: for cloned PUTREQ_LARGE, we do not need to update op_hdr.optype
					* TODO: Only PUTREQ_LARGE will aplly the following MATs in egress pipeline
				+ If GETRES_POP (with new key-value), update it as GETRES to client
				+ If PUTREQ_POP (with new key-value), update it as PUTRES to client
				+ TODO: If SCANREQ_SPLIT
					* TODO: If split_idx == 1 (the last 2nd pkt has been sent), set split_idx=0 (send the last pkt) and send to next server (increase dst_port)
					* TODO: Otherwise: decrease split_idx, send to next server, and clone_e2e
				+ TODO: For mirrored EVICT and mirrored EVICT_CASE2, assign a specific udp port as switch OS
			- TODO: switch-driven partition scheme (change udp port)
				+ If no range query, hash parition for normal REQ packets
				+ TODO: If w/ range query, range partition for normal REQ packets
					* TODO: For SCANREQ, update it as SCANREQ_SPLIT to corresponding server (based on beginkey) with split_num (based on beginkey and endkey) and split_idx (always start from split_num-1), and clone_e2e
					* TODO: Server needs to embed snapshot id into SCANRES for client to judge the consistency, and retry if not (snapshot and hence inconsistent SCANRES is rare)
				+ TODO: assign a specific udp port as switch OS for all packets of CASE1
				+ TODO: try to move it after eg_port_forward_tbl to reduce MAT entries and actions?
			- TODO: Access per-server iscase3
			- TODO: Access eg_port_forward_tbl
				- TODO: For PUT/DELREQ, and PUT/DELREQ_RECIR
					+ If iscase3=0, update it as PUT/DELREQ_CASE3 to server
					+ Otherwise, update it as PUT/DELREQ to server
				- TODO: If PUTREQ/GETRES_POP_EVICT/EVICT_CASE2 and PUTREQ_LARGE_EVICT/EVICT_CASE2, forward to server, and clone_e2e for switchOS thread (simulate copy_to_cpu)
					+ For CASE2, switch OS can both cope with packet loss and perform rollback for snapshot
					+ For each evicted packet sent from switch OS, server can compare the carried seq with that saved in server to decide whether overwrite
					+ No matter whether overwrite, server always sends ACK to switch OS
					+ Bandwidth overhead of switch OS is limited, as eviction is rare under skewed workload and packet loss is rare
			- Same stage
				+ Access update_udplen_tbl (default: not change udp_hdr.hdrLen)
					* NOTE: only match vallen <= 128B; udp_hdr.hdrLen = 6 + payloadsize
						- Payloadsize: 1B optype + 2B hashidx + 16B key + 4B vallen + aligned vallen
					* TODO: Including GETRES from server/switch, GETRES_POP_EVICT/_CASE2, PUTRES from server/switch, PUTREQ_POP_EVICT/_CASE2, PUTREQ_CASE1
					* TODO: check why we need parameter 0 when adding entry with range field
				+ Access update_macaddr_tbl
	+ GETREQ
		* Stage 0: match key 
		* Stage 1
			- Get valid
			- Update vote: if key matches, increase vote; otherwise, decrease vote
			- Update iskeymatch
		* Stage 2
			- Access lock: if valid=0 or zerovote=2, try_lock; otherwise, read_lock
		* Stage 3-11: get vallen and value
		* Stage 11: port_forward
			- If (valid=0 or zerovote=2) and lock=0, update GETREQ to GETREQ_POP -> hash_partition_tbl
			- If valid=1 and iskeymatch=1, update GETREQ to GETRES
			- If (valid=0 or iskeymatch=0) and lock=1, recirculate GETREQ
			- Otherwise, forward GETREQ to server -> hash_partition_tbl
	+ GETRES_POP
		* Stage 0: set_and_get key
		* Stage 1: set valid=1, vote=1
		* Stage 2: set savedseq=0, lock=0
		* Stage 3-11:
			- Set_and_get vallen and value
			- Try_case12 isbackup=1; otherwise, read_case12
		* Stage 11: port_forward
			- NOTE: current GETRES_POP has old key-value pair instead of new one, we must send original packet to egress pipeline
			- If isbackup=1 and iscase12=0, update GETRES_POP as GETRES_POP_EVICT_CASE2 to server, clone_i2e for GETRES to client
				+ GETRES_POP_EVICT_CASE2 -> hash_partition_reverse_tbl, update_macaddr_c2s
			- Otherwise
				+ If valid=0, drop original packet, clone_i2e for GETRES to client
				+ If valid=1, update GETRES_POP as GETRES_POP_EVICT to server, clone_i2e for GETRES to client
				+ GETRES_POP_EVICT -> hash_partition_reverse_tbl, update_macaddr_c2s
	+ GETRES_NPOP
		* Stage 2: set lock=0
		* Stage 11: port_forward -> update GETRES_NPOP as GETRES to client
	+ GETRES_POP_LARGE
		* Stage 2: set lock=0
		* Stage 11: port_forward -> update GETRES_LARGE as GETRES to client
	+ PUTREQ
		* Stage 0: match key
		* Stage 1
			- Get valid
			- Update vote: if key matches, increase vote; otherwise, decrease vote
			- Assign seq for each slot (no matter key matches or not)
			- Update iskeymatch
		* Stage 2
			- If valid=1 and iskeymatch=1, try to update savedseq to update meta.canput
			- Access lock: if valid=0 or zerovote=2, try_lock; otherwise, read_lock
		* Stage 3-11: 
			- Set_and_get vallen and value if valid=1 and canput=2 (valid=1, iskeymatch=1, and seq>savedseq)
			- Try_case12 if valid=1 and iskeymatch=1 and canput=2 and isbackup=1; otherwise, read_case12
		* Stage 11: 
			- Access port_forward_tbl
				- If (valid=0 or zerovote=2) and lock=0, update PUTREQ to PUTREQ_POP and recirculate
				- If valid=1 and iskeymatch=1
					+ If canput=2 and isbackup=1 and iscase12=0, update PUTREQ as PUTREQ_CASE1 to server, clone_i2e for PUTRES to client
					+ Otherwise, update PUTREQ to PUTRES
					- NOTE: current PUTREQ has old key-value pair instead of new one, we must send original packet to egress pipeline
				- If (valid=0 or iskeymatch=0) and lock=1, update PUTREQ as PUTREQ_RECIR (with seq_hdr, not need to assign seq again) and recirculate
				- Otherwise
					+ If isbackup=0, forward PUTREQ to server -> hash_partition_tbl
					+ If isbackup=1, update PUTREQ as PUTREQ_MAY_CASE3 (embedded with other_hdr.iscase3)
			- If "otherwise" and isbackup=1, try_case3
	+ PUTREQ_LARGE/RECIR
		* Stage 0: match key
		* Stage 1
			- Reset_and_get valid=0 if key matches
			- Assign seq only for PUTREQ_LARGE not PUTREQ_LARGE_RECIR
			- Update iskeymatch
		* Stage 3-11
			- Get vallen if iskeymatch=1 and isvalid=1; get val if iskeymatch=1
				+ NOTE: if PUTREQ_LARGE/RECIR do not need EVICT, they should not change vallen_hdr which will be deparsed; but they can simply change val_hdr as it will not be deparsed due to vallen > 128
			- Try case12 if isbackup=1, iskeymatch=1, and isvalid=1; otherwise, read case12
		* Stage 11
			- NOTE: PUTREQ_LARGE can trigger both case 2 and case 3
			- Access port_forward_tbl
				+ If iskeymatch=0 and lock=1, TODO: set seq_hdr.seq=meta.seq, update PUTREQ_LARGE/RECIR as PUTREQ_LARGE_RECIR (with seq_hdr, not need to assign seq again) and recirculate
				+ If iskeymatch=1 and isvalid=1
					- If isbackup=1 and iscase12=0, update PUTREQ_LARGE/RECIR as PUTREQ_LARGE_EVICT_CASE2 to server (evict the value of the same key to cope with packet loss), clone_i2e for PUTREQ_LARGE/RECIR to server with meta.seq
						+ PUTREQ_LARGE_EVICT_CASE2 -> hash_partition_tbl
					- Otherwise, update PUTREQ_LARGE/RECIR as PUTREQ_LARGE_EVICT to server, clone_i2e for PUTREQ_LARGE/RECIR to server with meta.seq
				+ Otherwise, TODO: set seq_hdr.seq=meta.seq, update PUTREQ_LARGE/RECIR as PUTREQ_LARGE to server
			- TODO: If lock=0 and isbackup=1, try_case3
	+ PUTREQ_POP
		* Stage 0: set_and_get key
		* Stage 1: set valid=1, vote=1
		* Stage 2: set savedseq=0, lock=0
		* Stage 3-11:
			- Set_and_get vallen and value
			- Try case12 if isbackup=1; otherwise, read_case12
		* Stage 11: port_forward
			- NOTE: current PUTREQ_POP has old key-value pair instead of new one, we must send original packet to egress pipeline
			- If isbackup=1 and iscase12=0, update PUTREQ_POP as PUTREQ_POP_EVICT_CASE2 to server, clone_i2e for PUTRES to client
				+ PUTREQ_POP_EVICT_CASE2 -> hash_partition_tbl
			- If (isbackup=0 or iscase12=1) and valid=0, drop original packet, clone_i2e for PUTRES to client
			- If (isbackup=0 or iscase12=1) and valid=1, update PUTREQ_POP as PUTREQ_POP_EVICT to server, clone_i2e for PUTRES to client
				+ PUTREQ_POP_EVICT -> hash_partition_tbl
	+ PUTREQ_RECIR (carry seq_hdr already)
		* Stage 0: match key
		* Stage 1
			- Get valid
			- Update vote: if key matches, increase vote; otherwise, decrease vote
			- NOTE: do not assign seq 
			- Update iskeymatch
		* Stage 2
			- If valid=1 and iskeymatch=1, try to update savedseq to update meta.canput
			- Access lock: if valid=0 or zerovote=2, try_lock; otherwise, read_lock
		* Stage 3-11: 
			- Set_and_get vallen and value if valid=1 and canput=2 (valid=1, iskeymatch=1, and seq>savedseq)
			- Try_case12 if valid=1 and iskeymatch=1 and canput=2 and isbackup=1; otherwise, read_case12
		* Stage 11: 
			- Access port_forward_tbl
				- If (valid=0 or zerovote=2) and lock=0, update PUTREQ_RECIR to PUTREQ_POP and recirculate
				- If valid=1 and iskeymatch=1
					+ If canput=2 and isbackup=1 and iscase12=0, update PUTREQ as PUTREQ_CASE1 to server, clone_i2e for PUTRES to client
					+ Otherwise, update PUTREQ to PUTRES
					- NOTE: current PUTREQ_RECIR has old key-value pair instead of new one, we must send original packet to egress pipeline
				- If (valid=0 or iskeymatch=0) and lock=1, recirculate PUTREQ_RECIR
				- Otherwise
					+ If isbackup=0, update PUTREQ_RECIR as PUTREQ to server -> hash_partition_tbl
					+ If isbackup=1, update PUTREQ_RECIR as PUTREQ_MAY_CASE3 (embedded with other_hdr.iscase3)
			- If "otherwise" and isbackup=1, try_case3
	+ DELREQ (as a speical PUTREQ)
		* Stage 0: match key
		* Stage 1
			- Get valid (treat DELREQ as a special PUTREQ which does not need to reset valid bit)
			- Not update vote now (TODO: maybe set vote=0 is better if popularity in the slot changes after DEL)
			- Assign seq for each slot (no matter key matches or not)
			- Update iskeymatch
		* Stage 2
			- If valid=1 and iskeymatch=1, try to update savedseq to update meta.canput
			- Not access lock (DEL will not trigger eviction)
		* Stage 3-11:
			- Reset_and_get vallen, and get value if valid=1 and canput=2 (valid=1, iskeymatch=1, and seq>savedseq)
			- Try_case12 if valid=1 and iskeymatch=1 and canput=2 and isbackup=1; otherwise, read_case12
		* Stage 11: 
			- Access port_forward_tbl
				- If valid=1 and iskeymatch=1
					+ If canput=2 and isbackup=1 and iscase12=0, update DELREQ as DELREQ_CASE1 to server, clone_i2e for DELRES to client
					+ Otherwise, update DELREQ to DELRES
					- NOTE: current DELREQ has old key-value pair instead of new one, we must send original packet to egress pipeline
				- If (valid=0 or iskeymatch=0) and lock=1, update DELREQ as DELREQ_RECIR (with seq_hdr, not need to assign seq again) and recirculate
				- Otherwise
					+ If isbackup=0, forward DELREQ to server -> hash_partition_tbl
					+ If isbackup=1, update DELREQ as DELREQ_MAY_CASE3 (embedded with other_hdr.iscase3)
			- If "otherwise" and isbackup=1, try_case3
	+ DELREQ_RECIR
		* Stage 0: match key
		* Stage 1
			- Get valid (treat DELREQ_RECIR as a special PUTREQ which does not need to reset valid bit)
			- Not update vote now (TODO: maybe set vote=0 is better if popularity in the slot changes after DEL)
			- NOTE: do not assign seq 
			- Update iskeymatch
		* Stage 2
			- If valid=1 and iskeymatch=1, try to update savedseq to update meta.canput
			- Not access lock (DEL will not trigger eviction)
		* Stage 3-11:
			- Reset_and_get vallen, and get value if valid=1 and canput=2 (valid=1, iskeymatch=1, and seq>savedseq)
			- Try_case12 if valid=1 and iskeymatch=1 and canput=2 and isbackup=1; otherwise, read_case12
		* Stage 11: 
			- Access port_forward_tbl
				- If valid=1 and iskeymatch=1
					+ If canput=2 and isbackup=1 and iscase12=0, update DELREQ as DELREQ_CASE1 to server, clone_i2e for DELRES to client
					+ Otherwise, update DELREQ to DELRES
					- NOTE: current DELREQ_RECIR has old key-value pair instead of new one, we must send original packet to egress pipeline
				- If (valid=0 or iskeymatch=0) and lock=1, recirculate DELREQ_RECIR
				- Otherwise
					+ If isbackup=0, update DELREQ_RECIR as DELREQ to server -> hash_partition_tbl
					+ If isbackup=1, update DELREQ_RECIR as DELREQ_MAY_CASE3 (embedded with other_hdr.iscase3)
			- If "otherwise" and isbackup=1, try_case3
	+ TODO: Cope with packet loss
		* For GETRES_POP_EVICT/PUTREQ_POP_EVICT: send to server as well as mirroringing to switch OS for timeout and retransmission
		* For case1/case2: send to switch OS for local rollback, switch OS can handle packet loss (send to server and count those requests as bandwidth to switchOS for simulation)
		* For case3: wait for PUT/DELRES_CASE3 (with server id to index case3) from server to set iscase3=1
- Server-side processsing
	+ GETREQ: sendback GETRES
	+ GETREQ_POP:
		* If key exists in KVS
			- If value <= 128B, sendback GETRES_POP
			- TODO: If value > 128B, sendback GETRES_POP_LARGE
		* Otherwise, sendback GETRES_NPOP
	+ GETRES_POP_EVICT:
		* If vallen > 0, put evicted key-value pair into KVS without response
		* Otherwise, delete evicted key from KVS without response
	+ PUTREQ_POP_EVICT:
		* If vallen > 0, put evicted key-value pair into KVS without response
		* Otherwise, delete evicted key from KVS without response
	+ TODOTODO: PUTREQ_LARGE, PUTREQ_LARGE_EVICT, PUTREQ_LARGE_EVICT_CASE2
	+ PUTREQ_CASE1/DELREQ_CASE1:
		* Add into special case
	+ GETRES_POP_EVICT_CASE2/PUTREQ_POP_EVICT_CASE2
		* TODO: make snapshot
		* If vallen > 0, put evicted key-value pair into KVS without response, add into special case (valid)
		* Otherwise, delete evicted key from KVS without response, add into special case (invalid)
	+ PUTREQ/DELREQ/PUTREQ_LARGE_CASE3:
		* TODO: make snapshot
	+ SCANREQ:
		* TODO: Switch: split SCANREQ to the corresponding server based on key range
		* TODO: Server: perform SCANREQ in snapshot of in-memory KVS and that of in-switch cache, and merge results
		* TODO: Client: wait for all responses (each response carries # of splits)
	+ TODO: snapshot for range query
		* Make a snapshot when init or open
		* Ensure that in each period of backup, kv snapshot can only be performed once
			* Use std::atomic_flag: test_and_set & clear
		* Steps of processing backup and making snapshot
			* Set isbackup as true to disable server threads from touching per-thread special cases
			* If is_kvsnapshot is false, mark it as true and make kv snapshot by RCU
			* Use TCP to receive new backup data
			* Rollback per-thread special cases (case1/case2) from new backup data
				* If vallen = 0 delete the entry from backup data; otherwise, overwrite the entry in backup data
			* Replace old backup data with new backup data
			* RCU barrier (no other threads touching per-thread special cases and old backup data)
			* Free old backup data and emptize per-thread special cases, set isbackup as false and is_kvsnapshot as false
	+ TOOD: Remember missed seq2 numbers and largest seq2 of GETRES/PUTREQ_EVICT and PUTREQ_LARGE_INVALIDATE for packet loss
		* If pkt.seq2 <= largest seq2
			* If pkt.seq2 in missed seq2 numbers, remove it from missed set and perform operation
			* Otherwise, ignore
		* If pkt.seq2 > largest seq2
			* Update missed set, update largest seq2, and perform operation
		* When switchOS sends pkt' with seq2 to server
- Controller-side processsing
	+ Crash-consistent backup (two-phase backup)
		* Phase 1
			* Reset registers: case12_reg, case3_reg
			* Set flag
		* Phase 2
			* Read registers
			* TODO: Load case3; if iscase3=0, notify the corresponding server to make snapshot and wait for ACK (need to count into bandwidth overhead of switch OS and servers)
			* Reset flag -> no special optype from now on
			* Optional: reset registers: case1_reg, case2_reg, case3_reg
		* Send backup data by TCP
- TODO: Simulation tricks
	+ Snapshot
		* Case1 should be forwarded to switch OS thread
		* Case2 should be forwarded to both the corresponding server thread and switch OS thread
		* Notification for server-side snapshot should be counted into server (multiply # of servers)
	+ Packet loss issued by switch to server
		* GETRES/PUTREQ_EVICT received by switch OS thread should be counted into server
		* GETRES/PUTREQ_EVICT from switch or from switch OS thread should be counted into server


## Implementation log

- Copy netreach-voting-v3 to netreach-voting-v3-memory and replace rocksdb with xindex
- Merge extended xindex from tofino-xindex-dpdk-memory-varkv into netreach-voting-v3-memory
	* Including: helper.h, original_xindex/\*, extended_xindexplus/\*, localtest.c, Makefile, ycsb_server.c, key.\*, val.\*
- Add CBF-based fast path to get xindex+
- Sync tofino-xindex-dpdk-memory-varkv to extend xindex with variable-length value and snapshot
- Support value > 128B (TODO: apply to baseline?)
	+ Add switch_max_vallen and max_vallen (use # of bytes instead of # of 8B) (config.ini, iniparser/iniparser_wrapper.\*, val.\*, localtest.c, ycsb_server.c, ycsb_local_client.c, ycsb_remote_client.c, ycsb/parser.c)
	+ When serializing/deserializing value, if vallen <= 128B, we need to pad it with 8B alignment (val.\*)
		* TODO: Introduce buflen when serializing/deserializing value for each packet with value (packet_format_impl.h)
	+ Change vallen from 1B to 4B (note that vallen should be used in switch -> convert between little-endian and big-endian) (val.\*, packet_format_impl.h)
	+ Dynamically calculate udp header length according to vallen in switch and update udp_hdr.hdrLen if necessary
- Support normal packets
	+ Suport 9 types of GET: GETREQ, GETREQ_RECIR, GETRES from server, GETREQ_POP, GETRES_POP, GETRES_NPOP, GETRES_POP_LARGE, GETRES from switch, GETRES_POP_EVICT, GETRES_POP_EVICT_CASE2
	+ Support ? types of PUT: PUTREQ, PUTREQ_RECIR, PUTRES from server, PUTREQ_POP, PUTREQ_POP_EVICT, PUTREQ_POP_EVICT_CASE2, PUTRES from switch, PUTREQ_CASE1, PUTREQ_CASE3,
	+ TODO: PUTREQ_LARGE, PUTREQ_LARGE_CASE3, PUTREQ_LARGE_RECIR, PUTREQ_LARGE_EVICT, PUTREQ_LARGE_EVICT_CASE2
+ TODO: Check localtest
+ TODO: Check ycsb
- TODO: range query
- TODO: NetCache
- TODO: switch-driven consistent hashing
- TODO: distributed extension
- TODO: DistCache

## How to run

- Prepare for YCSB
	- Prepare workload for loading or transaction phase
		+ For example:
		+ `./bin/ycsb.sh load basic -P workloads/workloada -P netbuffer.dat > workloada-load.out`
		+ `./bin/ycsb.sh run basic -P workloads/workloada -P netbuffer.dat > workloada-run.out`
		+ `./split_workload load`
		+ `./split_workload run`
- Server
	- `./ycsb_local_client` for loading phase
	- `./ycsb_server` for server-side in transaction phase
- Client
	- `./ycsb_remote_client` for client-side in transaction phase
- Switch
	- Run `cd tofino`
	+ Run `su` to enter root account
	+ Run `bash compile.sh` to compile p4 into binary code
	+ Run `bash start_switch.sh` to launch Tofino
	+ Create a new terminal and run `bash configure.sh` to configure data plane
	+ `bash controller.sh setup`
	+ END: `bash controller.sh cleanup`
- Directory structure
	+ Raw workload file: workloada-load.out, workloada-run.out
	+ Split workload file: e.g., workloada-load-5/2.out
	+ Database directory: e.g., /tmp/netbuffer/workloada/group0.db, /tmp/netbuffer/workloada/buffer0-0.db
	+ RMI model at root node when init key-value store: workloada-root.out

## Simple test

- NOTE: update bucket_num in config.ini as 1 before test
- Test cases of normal operations: See directory of "testcases/normal" (with only 1 bucket in sketch)
	+ Case 1: single read (GET evicts invalid)
		* Read the value of a given key
		* It should read the value from the server and also store it in switch
		* In-switch result: non-zero key, vallen, and val, seq = 0, savedseq = 0, lock = 0, valid = 1, vote = 1
	+ Case 2: single write (PUT evicts invalid)
		* Write new value for a given key
		* It should write the value into switch by recirculation and sendback PUTRES (no PUTREQ_POP)
		* In-switch result: non-zero key, vallen, and val, seq = 1, savedseq = 0, lock = 0, valid = 1, vote = 1
	+ Case 3: read-after-write
		* Write value of k1 and then read k1
		* It should write the value in switch and read the value from switch (not touch server)
		* In-switch result: non-zero key, vallen, and val of k1, seq = 1, savedseq = 0, lock = 0, valid = 1, vote = 2
	+ Case 4: read-after-two-writes
		* Write value of k1 twice, and then read k1
		* It should write the value in switch and read the value from switch (not touch server)
		* In-switch result: non-zero key, vallen, and val of k1, seq = 2, savedseq = 2, lock = 0, valid = 1, vote = 3
	+ Case 5: write-after-read1
		* Read value of k1 and then write k1
		* It reads the value of k1 from server and store it in switch, PUT increases vote, updates vallen & val, and does not touch server
		* In-switch result: non-zero key, vallen, and val of k1, seq = 1, savedseq = 1, lock = 0, valid = 1, vote = 2
	+ Case 6: write-after-read2
		* Read value of k1 and then write k2
		* It should read the value of k1 from server and store it in switch, k2 will decrease vote and be forwarded to server (no cache update)
		* In-switch result: non-zero key, vallen, and val of k1, seq = 1, savedseq = 0, lock = 0, valid = 1, vote = 0
	+ Case 7: two-writes-after-read (PUT evicts GET)
		* Read value of k1 and then write k2 twice
		* It should read the value of k1 from server and store it in switch, k2 will replace k1 finally (PUTs touch server only once)
		* In-switch result: non-zero key, vallen, and val of 2nd k2, seq = 2, savedseq = 0, lock = 0, valid = 1, vote = 1
	+ Case 8: read-after-two-writes-after-write (PUT evicts PUT)
		* Write value of k1, write k2 twice, and then read k2
		* PUT of k1 writes the value in switch and sendback PUTRES, 1st PUT of k2 is forwarded to server, 2nd PUT of k2 evicts k1, GET
		is directly processed by switch
		* In-switch result: non-zero key, vallen, and val of 2nd k2, seq = 3, savedseq = 0, lock = 0, valid = 1, vote = 2
	+ Case 9: two-reads-after-write (GET evicts PUT)
		* Write value of k1, read k2 twice
		* It writes value of k1 in switch, GETs of k2 evicted k1 (the evicted data touches server)
		* In-switch result: non-zero key, vallen, and val of k2, seq = 1, savedseq = 0, lock = 0, valid = 1, vote = 1
	+ Case 10: two-reads-after-read (GET evicts GET)
		* Read value of k1, read k2 twice
		* It first gets value of k1 from server and stores it in switch, the 2nd GET of k2 replaces k1 in switch
		* In-switch result: non-zero key, vallen, and val of k2, seq = 0, savedseq = 0, lock = 0, valid = 1, vote = 1
	+ Case 11: read-delete-read
		* Read value of k1, delete k1, and then read k1 again
		* It first gets value of k1 from server and stores it in switch, then it deletes k1 (set vallen=0 without changing valid and vote), the 2nd GET
		of k1 gets a value of size 0 
		* In-switch result: non-zero key, vallen=0, and val of k1, seq = 0, savedseq = 0, lock = 0, valid = 1, vote = 2
- Test cases of crash-consistent backup and range query: See "testcases/backup" (with only 1 bucket in sketch)
	+ NOTE: remember to set bucket_num in config.ini, otherwise the hashidx will be incorrect sent by phase2 ptf
	+ NOTE: if data in backup is not dirty, it will incur duplicate results for range query -> we leave the deduplication in client-side
		* If data in backup is dirty, it will incur inconsistent results (from KVS and backup) for range query -> we leave it in client-side, i.e., client treats data from backup with higher priority
		* If data in backup with vallen of 0, it is deleted which should not be the result of range query -> we leave the removal of deleted data in client-side
	+ Phase1: reset regs and set flag as 1
	+ Case 1-1: undirty + PUT case1
		* Get <k1, v1> -> Run phase1 -> PUT <k1, v2> -> Run phase2
		* Result: receive PUTREQ_CASE1 with <k1, v1>, receive backup with <k1, v2>, final backup after rollback with <k1, v1>
	+ Case 1-2: dirty + PUT case1
		* PUT <k1, v1> -> Run phase1 -> PUT <k1, v2> -> Run phase2
		* Result: receive PUTREQ_CASE1 with <k1, v1>, receive backup with <k1, v2>, final backup after rollback with <k1, v1>
	+ Case 1-3: undirty + DEL case1
		* Get <k1, v1> -> Run phase1 -> DEL k1 -> Run phase2
		* Result: receive DELREQ_CASE1 with <k1, v1>, receive backup with <k1, vallen=0>, final backup after rollback with <k1, v1>
	+ Case 1-4: dirty + DEL case1
		* PUT <k1, v1> -> Run phase1 -> DEL k1 -> Run phase2
		* Result: receive DELREQ_CASE1 with <k1, v1>, receive backup with <k1, vallen=0>, final backup after rollback with <k1, v1>
	+ Case 2-1: invalid + GETRES_POP case2
		* Run phase1 -> GET <k1, v1> -> Run phase2
		* Result: receive GETRES_POP_EVICT_CASE2 with <0, vallen=0>, receive backup with <k1, v1>, final backup after rollback without k1 
		* NOTE: GETRES_NS will not trigger cache update and hence no special case
	+ Case 2-2: undirty + GETRES_POP case2
		* GET <k1, v1> -> Run phase1 -> GET <k2, v2> -> Get <k3, v3> -> Run phase2
		* Result: receive GETRES_POP_EVICT_CASE2 with <k1, v1>, receive backup with <k3, v3>, final backup after rollback with <k1, v1>
	+ Case 2-3: dirty + GETRES_POP case2
		* PUT <k1, v1> -> Run phase1 -> GET <k2, v2> -> GET <k3, v3> -> Run phase2
		* Result: receive GETRES_POP_EVICT_CASE2 with <k1, v1>, receive backup with <k3, v3>, final backup after rollback with <k1, v1>
	+ Case 2-4: invalid + PUTREQ_POP case2
		* Run phase1 -> PUT <k1, v1> -> Run phase2
		* Result: receive PUTREQ_POP_EVICT_CASE2 with <0, vallen=0>, receive backup with <k1, v1>, final backup after rollback without k1
	+ Case 2-5: undirty + PUTREQ_POP case2 + PUTREQ case3
		* GET <k1, v1> -> Run phase1 -> PUT <k2, v2> -> PUT <k3, v3> -> Run phase2
		* Result: receive PUTREQ_CASE3 with <k2, v2> and PUTREQ_POP_EVICT_CASE2 with <k2, v2>, receive backup with <k3, v3>, final 
		backup after rollback with <k1, v1>
	+ Case 2-6: dirty + PUTPS case2 + PUTREQ case3
		* PUT <k1, v1> -> Run phase1 -> PUT <k2, v2> -> PUT <k3, v3> -> Run phase2
		* Result: receive PUTREQ_CASE3 with <k2, v2> and PUTREQ_POP_EVICT_CASE2 with <k2, v2>, receive backup with <k3, v3>, final 
		backup after rollback with <k1, v1>
	+ Case 3-1: DELREQ case3
		* PUT <k1, v1> -> Run phase1 -> DEL <k2, v2> -> Run phase2
		* Result: receive DELREQ_CASE3 with k2, receive backup with <k1, v1>, final backup after rollback with <k1, v1>
	+ Case 4-1: range query
		* DEL <k1, v1> -> PUT <k1, v2> -> Run phase1 -> PUT <k1, v3> -> Run phase2 -> SCAN
		* Result: receive PUTREQ_CASE1 with <k1, v2>, receive bakup with <k1, v3>, final backup after rollback with <k1, v2>, SCAN
		result with <k1, v2>

## Fixed issues
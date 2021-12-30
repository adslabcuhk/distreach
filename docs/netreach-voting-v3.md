# Tofino-based NetREACH (voting-based) + DPDK-based XIndex with persistence + variable-length key-value pair (netreach-voting-v3)

- Remove hash_tbl, try_res_tbl, using 4B for each unit of key instead of 2B -> reduce 3 stages

## Overview

- Design features
	+ TODO: Parameter-free decision
		* Existing: large parameter -> miss hot keys; small parameter -> too many hot keys -> insufficient cache capacity and switch OS bottleneck
		* Challenge: Slow warmup with incast populations -> data-plane-based cache population
	+ TODO: Data-plane-based cache update
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
	+ TODO: Crash-consistent backup
		* Case 1: change on in-switch cache value;
		* Case 2: change on in-switch cache key-value pair;
		* Case 3: change on server KVS for range query;
	+ TODO: Others
		* Switch-driven consistent hashing
		* CBF-based fast path
		* Range query support
		* Distributed extension
		* Variable-length key-value
- Workflow
- Baselines
- NOTES

## Details 

- Packet format
- In-switch processing
	+ Overview
		* Stage 0: keylolo, keylohi, keyhilo, keyhihi, load_backup_tbl
		* Stage 1: valid, vote, seq (assign only if key matches for PUT/DELREQ), update_iskeymatch_tbl
		* Stage 2: savedseq, lock, 
		* Stage 3: vallen, vallo1, valhi1, case12
		* Stage 4-10: from val2 to val15
		* Stage 11: vallo16, valhi16, case3, port_forward_tbl
			- For case1, if backup=1 and valid=1 and iskeymatch=1 (key is the same) and iscase12=0
				+ Update PUT/DELREQ with old value as PUT/DELREQ_CASE1 to server, clone_i2e for PUT/DELRES
			- For case2, if backup=1 and iscase12=0
				+ Update GETRES_POP with old key-value as GETRES_POP_CASE2 to server, clone_i2e for GETRES with new key-value
				+ Update PUTREQ_POP with old key-value as PUTREQ_POP_CASE2 to server, clone_i2e for PUTRES with new key
			- For case3, if backup=1
				+ Embed other_hdr (valid, case3) at the end of PUT/DELREQ
					+ Update PUTREQ with new key-value as PUTREQ_CASEV
					+ Update DELREQ without key-value as DELREQ_CASENV
		* Egress pipeline
			- For PUTREQ_CASEV/DELREQ_CASENV
				+ If case3=0, update it as PUT/DELREQ_CASE3 to server
				+ Otherwise, update it as PUT/DELREQ to server
			- For cloned packet
				+ If GETRES_POP (with new key-value), update it as GETRES to client
				+ If PUTREQ_POP (with new key-value), update it as PUTRES to client
				+ If PUT/DELREQ, update as PUT/DELRES to client
			- Hash parition for normal REQ pacekts
	+ GETREQ
		* Stage 0: Get key 
		* Stage 1
			- Get valid
			- Update vote: if key matches, increase vote; otherwise, decrease vote
			- Update iskeymatch
		* Stage 2
			- Access lock: if valid=0 or zerovote=2, try_lock; otherwise, read_lock
		* Stage 3-11: read vallen and value
		* Stage 11: port_forward
			- If (valid=0 or zerovote=2) and lock=0, update GETREQ to GETREQ_POP
			- If valid=1 and iskeymatch=1, update GETREQ to GETRES
			- If (valid=0 or iskeymatch=0) and lock=1, recirculate GETREQ
			- If (valid=0 or iskeymatch=0) and lock=0, forward GETREQ to server
- Client-side processsing
- Server-side processsing

## Implementation log

- Copy netreach-voting to netreach-voting-v3
- Embed hashidx into packet op_hdr at client side (ycsb_remote_client.c, config.ini, packet_format.h, packet_format_impl,h, ycsb_server.c)
- Support GETREQ
- TODO: support PUTREQ, DELREQ

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

## Fixed issues
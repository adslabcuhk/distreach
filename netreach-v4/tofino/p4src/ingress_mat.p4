/* Ingress Processing (Normal Operation) */

action nop() {}

// Stage 0

action cached_action(idx) {
	modify_field(inswitch_hdr.idx, idx);
	modify_field(inswitch_hdr.is_cached, 1);
}

action uncached_action() {
	modify_field(inswitch_hdr.is_cached, 0);
}

table cache_lookup_tbl {
	reads {
		op_hdr.optype: exact;
		op_hdr.keylolo: exact;
		op_hdr.keylohi: exact;
		op_hdr.keyhilo: exact;
		op_hdr.keyhihi: exact;
	}
	actions {
		cached_action;
		uncached_action;
	}
	default_action: uncached_action();
	size: LOOKUP_ENTRY_COUNT;
}

field_list hash_fields {
	op_hdr.keylolo;
	op_hdr.keylohi;
	op_hdr.keyhilo;
	op_hdr.keyhihi;
}

field_list_calculation hash_calc {
	input {
		hash_fields;
	}
	algorithm: crc32;
	output_width: 16;
}

field_list_calculation sample_calc {
	input {
		hash_fields;
	}
	algorithm: crc32;
	output_width: 1;
}

action hash() {
	modify_field_with_hash_based_offset(inswitch_hdr.hashval, 0, hash_calc, CM_BUCKET_COUNT);
}

table hash_tbl {
	reads {
		op_hdr.optype: exact;
	}
	actions {
		hash;
		nop;
	}
	default_action: nop();
	size: 0;
}

action sample() {
	modify_field_with_hash_based_offset(inswitch_hdr.is_sampled, 0, sample_calc, 2);
}

table sample_tbl {
	reads {
		op_hdr.optype: exact;
	}
	actions {
		sample;
		nop;
	}
	default_action: nop();
	size: 0;
}

action hash_partition(udpport, serveridx, eport) {
	modify_field(udp_hdr.dstPort, udpport);
	modify_field(serveridx_hdr.serveridx, serveridx);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, eport);
}

table hash_partition_tbl {
	reads {
		op_hdr.optype: exact;
		inswitch_hdr.hashval: range;
	}
	actions {
		hash_partition;
		nop;
	}
	default_action: nop();
	size: 128;
}















// Stage 0

action save_info() {
	modify_field(meta.tmp_sport, udp_hdr.srcPort);
	modify_field(meta.tmp_dport, udp_hdr.dstPort);
}

table save_info_tbl {
	actions {
		save_info;
	}
	default_action: save_info();
	size: 1;
}

action initialize() {
	// NOTE: condition_lo = false -> predicate = 1
	// Make such an initialization to reduce MAT entries
	modify_field(meta.zerovote, 1);
	modify_field(meta.canput, 1);
}

table initialize_tbl {
	actions {
		initialize;
	}
	default_action: initialize();
	size: 1;
}

action load_backup_flag(flag) {
	modify_field(meta.isbackup, flag);
}

table load_backup_flag_tbl {
	actions {
		load_backup_flag;
	}
	default_action: load_backup_flag(0);
	size: 1;
}

// Stage 1

action update_iskeymatch(iskeymatch) {
	modify_field(meta.iskeymatch, iskeymatch);
}

table update_iskeymatch_tbl {
	reads {
		meta.ismatch_keylolo: exact;
		meta.ismatch_keylohi: exact;
		meta.ismatch_keyhilo: exact;
		meta.ismatch_keyhihi: exact;
	}
	actions {
		update_iskeymatch;
	}
	default_action: update_iskeymatch(0);
	size: 1;
}

// Stage 11

action update_getreq_to_getreq_pop(port) {
	modify_field(op_hdr.optype, GETREQ_POP_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
}

action update_getreq_to_getres() {
	// Swap udp port
	modify_field(udp_hdr.dstPort, meta.tmp_sport);
	modify_field(udp_hdr.srcPort, meta.tmp_dport);
	//add_to_field(udp_hdr.hdrlen, VAL_PKTLEN); // Leave to update_udplen_tbl in egress

	modify_field(ig_intr_md_for_tm.ucast_egress_port, ig_intr_md.ingress_port);

	modify_field(op_hdr.optype, GETRES_TYPE);
	add_header(vallen_hdr);
	add_header(val1_hdr);
	add_header(val2_hdr);
	add_header(val3_hdr);
	add_header(val4_hdr);
	add_header(val5_hdr);
	add_header(val6_hdr);
	add_header(val7_hdr);
	add_header(val8_hdr);
	add_header(val9_hdr);
	add_header(val10_hdr);
	add_header(val11_hdr);
	add_header(val12_hdr);
	add_header(val13_hdr);
	add_header(val14_hdr);
	add_header(val15_hdr);
	add_header(val16_hdr);
}

action recirculate_getreq(port) {
	// It is equivalent to ig_intro_md_for_tm.ucast_egress_port = (port & 0x7f) | (ingress_port & ~0x7f)
	recirculate(port);
}

// NOTE: clone field list cannot exceed 32 bytes
// Only clone for RES needs original sport and dport
field_list clone_field_list {
	meta.tmp_sport;
	meta.tmp_dport;
}

// Only for PUTREQ_LARGE/_RECIR
field_list clone_field_list_large {
	meta.seq_large;
}

action drop_getres_pop_clone_for_getres(sid) {
	modify_field(ig_intr_md_for_tm.drop_ctl, 1); // Disable unicast, but enable mirroring
	clone_ingress_pkt_to_egress(sid, clone_field_list);
}

action update_getres_pop_to_evict_clone_for_getres(sid) {
	modify_field(op_hdr.optype, GETRES_POP_EVICT_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, ig_intr_md.ingress_port);
	clone_ingress_pkt_to_egress(sid, clone_field_list);
}

action update_getres_npop_to_getres(port) {
	modify_field(op_hdr.optype, GETRES_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
}

action update_getres_pop_large_to_getres(port) {
	modify_field(op_hdr.optype, GETRES_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
}

action update_putreq_to_putreq_seq(port) {
	// NOTE: PUTREQ_SEQ has seq_hdr for linearizability
	modify_field(op_hdr.optype, PUTREQ_SEQ_TYPE);
	add_to_field(udp_hdr.hdrlen, SEQ_PKTLEN);
	add_header(seq_hdr);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
}

action update_putreq_to_putreq_pop(port) {
	// NOTE: PUTREQ_POP has seq_hdr for linearizability
	modify_field(op_hdr.optype, PUTREQ_POP_TYPE);
	add_to_field(udp_hdr.hdrlen, SEQ_PKTLEN);
	add_header(seq_hdr);

	// It is equivalent to ig_intro_md_for_tm.ucast_egress_port = (port & 0x7f) | (ingress_port & ~0x7f)
	recirculate(port);
}

action update_putreq_to_putres() {
	// Swap udp port
	modify_field(udp_hdr.dstPort, meta.tmp_sport);
	modify_field(udp_hdr.srcPort, meta.tmp_dport);
	subtract_from_field(udp_hdr.hdrlen, VAL_PKTLEN_MINUS_STAT);

	modify_field(op_hdr.optype, PUTRES_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, ig_intr_md.ingress_port);

	remove_header(vallen_hdr);
	remove_header(val1_hdr);
	remove_header(val2_hdr);
	remove_header(val3_hdr);
	remove_header(val4_hdr);
	remove_header(val5_hdr);
	remove_header(val6_hdr);
	remove_header(val7_hdr);
	remove_header(val8_hdr);
	remove_header(val9_hdr);
	remove_header(val10_hdr);
	remove_header(val11_hdr);
	remove_header(val12_hdr);
	remove_header(val13_hdr);
	remove_header(val14_hdr);
	remove_header(val15_hdr);
	remove_header(val16_hdr);
	modify_field(res_hdr.stat, 1);
	add_header(res_hdr);
}

action update_putreq_to_putreq_recir(port) {
	// NOTE: PUTREQ_RECIR has seq_hdr for linearizability
	modify_field(op_hdr.optype, PUTREQ_RECIR_TYPE);
	add_to_field(udp_hdr.hdrlen, SEQ_PKTLEN);
	add_header(seq_hdr);

	// It is equivalent to ig_intro_md_for_tm.ucast_egress_port = (port & 0x7f) | (ingress_port & ~0x7f)
	recirculate(port);
}

action drop_putreq_pop_clone_for_putres(sid) {
	modify_field(ig_intr_md_for_tm.drop_ctl, 1); // Disable unicast, but enable mirroring
	clone_ingress_pkt_to_egress(sid, clone_field_list);
}

action update_putreq_pop_to_evict_clone_for_putres(sid, port) {
	// NOTE: PUTREQ_POP_EVICT has seq_hdr for linearizability
	modify_field(op_hdr.optype, PUTREQ_POP_EVICT_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
	clone_ingress_pkt_to_egress(sid, clone_field_list);
}

action update_putreq_recir_to_putreq_pop(port) {
	// NOTE: PUTREQ_POP has seq_hdr for linearizability
	modify_field(op_hdr.optype, PUTREQ_POP_TYPE);

	// It is equivalent to ig_intro_md_for_tm.ucast_egress_port = (port & 0x7f) | (ingress_port & ~0x7f)
	recirculate(port);
}

action update_putreq_recir_to_putres() {
	// Swap udp port
	modify_field(udp_hdr.dstPort, meta.tmp_sport);
	modify_field(udp_hdr.srcPort, meta.tmp_dport);
	subtract_from_field(udp_hdr.hdrlen, VAL_PKTLEN_MINUS_STAT_PLUS_SEQ);

	modify_field(op_hdr.optype, PUTRES_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, ig_intr_md.ingress_port);

	remove_header(vallen_hdr);
	remove_header(val1_hdr);
	remove_header(val2_hdr);
	remove_header(val3_hdr);
	remove_header(val4_hdr);
	remove_header(val5_hdr);
	remove_header(val6_hdr);
	remove_header(val7_hdr);
	remove_header(val8_hdr);
	remove_header(val9_hdr);
	remove_header(val10_hdr);
	remove_header(val11_hdr);
	remove_header(val12_hdr);
	remove_header(val13_hdr);
	remove_header(val14_hdr);
	remove_header(val15_hdr);
	remove_header(val16_hdr);
	remove_header(seq_hdr);
	modify_field(res_hdr.stat, 1);
	add_header(res_hdr);
}

action recirculate_putreq_recir(port) {
	// It is equivalent to ig_intro_md_for_tm.ucast_egress_port = (port & 0x7f) | (ingress_port & ~0x7f)
	recirculate(port);
}

action update_putreq_recir_to_putreq_seq(port) {
	// NOTE: PUTREQ_SEQ has seq_hdr for linearizability
	modify_field(op_hdr.optype, PUTREQ_SEQ_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
}

action update_putreq_large_to_putreq_large_seq(port) {
	modify_field(op_hdr.optype, PUTREQ_LARGE_SEQ_TYPE);
	add_to_field(udp_hdr.hdrlen, SEQ_PKTLEN);
	add_header(seq_hdr);
	modify_field(seq_hdr.seq, meta.seq_large);

	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
}

action update_putreq_large_to_putreq_large_recir(port) {
	modify_field(op_hdr.optype, PUTREQ_LARGE_RECIR_TYPE);
	add_to_field(udp_hdr.hdrlen, SEQ_PKTLEN);
	add_header(seq_hdr);
	modify_field(seq_hdr.seq, meta.seq_large);

	// It is equivalent to ig_intro_md_for_tm.ucast_egress_port = (port & 0x7f) | (ingress_port & ~0x7f)
	recirculate(port);
}

action recirculate_putreq_large_recir(port) {
	modify_field(seq_hdr.seq, meta.seq_large);
	// It is equivalent to ig_intro_md_for_tm.ucast_egress_port = (port & 0x7f) | (ingress_port & ~0x7f)
	recirculate(port);
}

action update_putreq_large_to_evict_clone_for_putreq_large_seq(sid, port) {
	modify_field(op_hdr.optype, PUTREQ_LARGE_EVICT_TYPE);
	add_to_field(udp_hdr.hdrlen, SEQ_PKTLEN);
	add_header(seq_hdr);

	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
	clone_ingress_pkt_to_egress(sid, clone_field_list_large);
}

action update_putreq_large_recir_to_evict_clone_for_putreq_large_seq(sid, port) {
	modify_field(op_hdr.optype, PUTREQ_LARGE_EVICT_TYPE);

	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
	clone_ingress_pkt_to_egress(sid, clone_field_list_large);
}

action update_putreq_large_recir_to_putreq_large_seq(port) {
	modify_field(op_hdr.optype, PUTREQ_LARGE_TYPE);

	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
}

action update_delreq_to_delres() {
	// Swap udp port
	modify_field(udp_hdr.dstPort, meta.tmp_sport);
	modify_field(udp_hdr.srcPort, meta.tmp_dport);
	add_to_field(udp_hdr.hdrlen, STAT_PKTLEN);

	modify_field(op_hdr.optype, DELRES_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, ig_intr_md.ingress_port);

	modify_field(res_hdr.stat, 1);
	add_header(res_hdr);
}

action update_delreq_to_delreq_recir(port) {
	modify_field(op_hdr.optype, DELREQ_RECIR_TYPE);
	add_to_field(udp_hdr.hdrlen, SEQ_PKTLEN);
	add_header(seq_hdr);

	// It is equivalent to ig_intro_md_for_tm.ucast_egress_port = (port & 0x7f) | (ingress_port & ~0x7f)
	recirculate(port);
}

action update_delreq_to_delreq_seq(port) {
	modify_field(op_hdr.optype, DELREQ_SEQ_TYPE);
	add_to_field(udp_hdr.hdrlen, SEQ_PKTLEN);
	add_header(seq_hdr);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
}

action update_delreq_recir_to_delres() {
	// Swap udp port
	modify_field(udp_hdr.dstPort, meta.tmp_sport);
	modify_field(udp_hdr.srcPort, meta.tmp_dport);
	subtract_from_field(udp_hdr.hdrlen, SEQ_PKTLEN_MINUS_STAT);

	modify_field(op_hdr.optype, DELRES_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, ig_intr_md.ingress_port);

	remove_header(seq_hdr);
	modify_field(res_hdr.stat, 1);
	add_header(res_hdr);
}

action recirculate_delreq_recir(port) {
	// It is equivalent to ig_intro_md_for_tm.ucast_egress_port = (port & 0x7f) | (ingress_port & ~0x7f)
	recirculate(port);
}

action update_delreq_recir_to_delreq_seq(port) {
	modify_field(op_hdr.optype, DELREQ_SEQ_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
}

action update_putreq_to_case1_clone_for_putres(sid, port) {
	modify_field(op_hdr.optype, PUTREQ_CASE1_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
	clone_ingress_pkt_to_egress(sid, clone_field_list);
}

action update_putreq_recir_to_case1_clone_for_putres(sid, port) {
	modify_field(op_hdr.optype, PUTREQ_CASE1_TYPE);
	subtract_from_field(udp_hdr.hdrlen, SEQ_PKTLEN);
	remove_header(seq_hdr);

	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
	clone_ingress_pkt_to_egress(sid, clone_field_list);
}

action update_delreq_to_case1_clone_for_delres(sid, port) {
	modify_field(op_hdr.optype, DELREQ_CASE1_TYPE);

	//add_to_field(udp_hdr.hdrlen, VAL_PKTLEN);
	add_header(vallen_hdr);
	add_header(val1_hdr);
	add_header(val2_hdr);
	add_header(val3_hdr);
	add_header(val4_hdr);
	add_header(val5_hdr);
	add_header(val6_hdr);
	add_header(val7_hdr);
	add_header(val8_hdr);
	add_header(val9_hdr);
	add_header(val10_hdr);
	add_header(val11_hdr);
	add_header(val12_hdr);
	add_header(val13_hdr);
	add_header(val14_hdr);
	add_header(val15_hdr);
	add_header(val16_hdr);

	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
	clone_ingress_pkt_to_egress(sid, clone_field_list);
}

action update_delreq_recir_to_case1_clone_for_delres(sid, port) {
	modify_field(op_hdr.optype, DELREQ_CASE1_TYPE);

	//add_to_field(udp_hdr.hdrlen, VAL_PKTLEN_MINUS_SEQ);
	subtract_from_field(udp_hdr.hdrlen, SEQ_PKTLEN);
	add_header(vallen_hdr);
	add_header(val1_hdr);
	add_header(val2_hdr);
	add_header(val3_hdr);
	add_header(val4_hdr);
	add_header(val5_hdr);
	add_header(val6_hdr);
	add_header(val7_hdr);
	add_header(val8_hdr);
	add_header(val9_hdr);
	add_header(val10_hdr);
	add_header(val11_hdr);
	add_header(val12_hdr);
	add_header(val13_hdr);
	add_header(val14_hdr);
	add_header(val15_hdr);
	add_header(val16_hdr);
	remove_header(seq_hdr);

	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
	clone_ingress_pkt_to_egress(sid, clone_field_list);
}

action update_getres_pop_to_case2_clone_for_getres(sid) {
	modify_field(op_hdr.optype, GETRES_POP_EVICT_CASE2_TYPE);
	add_header(other_hdr);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, ig_intr_md.ingress_port);
	clone_ingress_pkt_to_egress(sid, clone_field_list);
}

action update_putreq_pop_to_case2_clone_for_putres(sid, port) {
	modify_field(op_hdr.optype, PUTREQ_POP_EVICT_CASE2_TYPE);
	add_header(other_hdr);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
	clone_ingress_pkt_to_egress(sid, clone_field_list);
}

action update_putreq_large_to_case2_clone_for_putreq_large_seq(sid, port) {
	modify_field(op_hdr.optype, PUTREQ_LARGE_EVICT_CASE2_TYPE);
	add_to_field(udp_hdr.hdrlen, SEQ_PKTLEN);
	add_header(seq_hdr);

	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
	clone_ingress_pkt_to_egress(sid, clone_field_list_large);
}

action update_putreq_large_recir_to_case2_clone_for_putreq_large_seq(sid, port) {
	modify_field(op_hdr.optype, PUTREQ_LARGE_EVICT_CASE2_TYPE);

	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
	clone_ingress_pkt_to_egress(sid, clone_field_list_large);
}

action update_putreq_to_may_case3(port) {
	modify_field(op_hdr.optype, PUTREQ_MAY_CASE3_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);

	add_to_field(udp_hdr.hdrlen, OTHER_PKTLEN);
	add_header(other_hdr);
}

action update_putreq_recir_to_may_case3(port) {
	modify_field(op_hdr.optype, PUTREQ_MAY_CASE3_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);

	subtract_from_field(udp_hdr.hdrlen, SEQ_PKTLEN_MINUS_OTHER);
	remove_header(seq_hdr);
	add_header(other_hdr);
}

action update_delreq_to_may_case3(port) {
	modify_field(op_hdr.optype, DELREQ_MAY_CASE3_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);

	add_to_field(udp_hdr.hdrlen, OTHER_PKTLEN);
	add_header(other_hdr);
}

action update_delreq_recir_to_may_case3(port) {
	modify_field(op_hdr.optype, DELREQ_MAY_CASE3_TYPE);
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);

	subtract_from_field(udp_hdr.hdrlen, SEQ_PKTLEN_MINUS_OTHER);
	remove_header(seq_hdr);
	add_header(other_hdr);
}

action port_forward(port) {
	modify_field(ig_intr_md_for_tm.ucast_egress_port, port);
}

@pragma stage 11
table port_forward_tbl {
	reads {
		op_hdr.optype: exact;
		other_hdr.isvalid: exact;
		meta.zerovote: exact;
		meta.iskeymatch: exact;
		meta.islock: exact;
		meta.canput: exact;
		meta.isbackup: exact;
		meta.iscase12: exact;
	}
	actions {
		update_getreq_to_getreq_pop;
		update_getreq_to_getres;
		recirculate_getreq;
		drop_getres_pop_clone_for_getres;
		update_getres_pop_to_evict_clone_for_getres;
		update_getres_npop_to_getres;
		update_putreq_to_putreq_seq;
		update_putreq_to_putreq_pop;
		update_putreq_to_putres;
		update_putreq_to_putreq_recir;
		drop_putreq_pop_clone_for_putres;
		update_putreq_pop_to_evict_clone_for_putres;
		update_putreq_recir_to_putreq_pop;
		update_putreq_recir_to_putres;
		recirculate_putreq_recir;
		update_putreq_recir_to_putreq_seq;
		update_putreq_large_to_putreq_large_seq;
		update_putreq_large_to_putreq_large_recir;
		recirculate_putreq_large_recir;
		update_putreq_large_to_evict_clone_for_putreq_large_seq;
		update_putreq_large_recir_to_evict_clone_for_putreq_large_seq;
		update_putreq_large_recir_to_putreq_large_seq;
		update_delreq_to_delres;
		update_delreq_to_delreq_recir;
		update_delreq_to_delreq_seq;
		update_delreq_recir_to_delres;
		recirculate_delreq_recir;
		update_delreq_recir_to_delreq_seq;
		update_putreq_to_case1_clone_for_putres;
		update_putreq_recir_to_case1_clone_for_putres;
		update_delreq_to_case1_clone_for_delres;
		update_delreq_recir_to_case1_clone_for_delres;
		update_getres_pop_to_case2_clone_for_getres;
		update_putreq_pop_to_case2_clone_for_putres;
		update_putreq_large_to_case2_clone_for_putreq_large_seq;
		update_putreq_large_recir_to_case2_clone_for_putreq_large_seq;
		update_putreq_to_may_case3;
		update_putreq_recir_to_may_case3;
		update_delreq_to_may_case3;
		update_delreq_recir_to_may_case3;
		port_forward;
		nop;
	}
	default_action: nop();
	size: 2048;  
}

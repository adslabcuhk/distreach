#ifndef REFLECTOR_IMPL_H
#define REFLECTOR_IMPL_H

// shared by reflector.cp2dpserver and reflector.dp2cpserver, but no contention
//bool volatile reflector_with_switchos_popworker_popclient_for_reflector_addr = false;
std::mutex mutex_for_popclient_addr; // cope with recovery mode
struct sockaddr_in reflector_switchos_popworker_popclient_for_reflector_addr;
unsigned int reflector_switchos_popworker_popclient_for_reflector_addr_len = sizeof(struct sockaddr);
bool volatile reflector_with_switchos_snapshotserver_snapshotclient_for_reflector_addr = false;
struct sockaddr_in reflector_switchos_snapshotserver_snapshotclient_for_reflector_addr;
unsigned int reflector_switchos_snapshotserver_snapshotclient_for_reflector_addr_len = sizeof(struct sockaddr);

// switchos.popworker/snapshotserver.snapshotclient -> (udp channel) -> one reflector.cp2dpserver -> data plane
int reflector_cp2dpserver_udpsock = -1;

// data plane -> reflector.dp2cpserver
int reflector_dp2cpserver_udpsock = -1;

// data plane -> receiver -> (message) -> reflector.dp2cpserver -> switchos.popworker
// for cache population ack and special cases of snapshot
/*struct rte_mbuf* volatile * reflector_pkts_for_popack_snapshot; // pkts from receiver to reflector.dp2cpserver
uint32_t volatile reflector_head_for_popack_snapshot;
uint32_t volatile reflector_tail_for_popack_snapshot;*/

// reflector.dp2cpserver -> switchos.popworker/snapshotserver.snapshotclient
int reflector_dp2cpserver_popclient_udpsock = -1;

// reflector.dp2cpserver -> switchos.specialcaseserver
int reflector_dp2cpserver_specialcaseclient_udpsock = -1;

void prepare_reflector();
void *run_reflector_cp2dpserver(void *param);
void *run_reflector_dp2cpserver(void *param);
void close_reflector();

void prepare_reflector() {
	printf("[reflector] prepare start\n");

	// prepare worker socket
	prepare_udpserver(reflector_dp2cpserver_udpsock, true, reflector_dp2cpserver_port, "reflector.dp2cpserver", SOCKET_TIMEOUT, 0, 2*UDP_LARGE_RCVBUFSIZE);

	// prepare popserver socket
	prepare_udpserver(reflector_cp2dpserver_udpsock, true, reflector_cp2dpserver_port, "reflector.cp2dpserver", SOCKET_TIMEOUT, 0, UDP_LARGE_RCVBUFSIZE);

	// From receiver to reflector.dp2cpserver
	create_udpsock(reflector_dp2cpserver_popclient_udpsock, false, "reflector.dp2cpserver.popclient");

	create_udpsock(reflector_dp2cpserver_specialcaseclient_udpsock, false, "reflector.dp2cpserver.specialcaseclient");

	memory_fence();

	printf("[reflector] prepare end\n");
}

// forward pkt from switchos to switch
void *run_reflector_cp2dpserver(void *param) {
	// client address (switch will not hide CACHE_POP_INSWITCH from clients)
	struct sockaddr_in client_addr;
	set_sockaddr(client_addr, inet_addr(client_ips[0]), 123); // client ip and client port are not important
	socklen_t client_addrlen = sizeof(struct sockaddr_in);

	struct sockaddr_in tmp_addr;
	socklen_t tmp_addrlen = sizeof(struct sockaddr_in);

	char buf[MAX_BUFSIZE];
	printf("[reflector.cp2dpserver] ready\n");
	transaction_ready_threads++;

	while (!transaction_running) {}

	while (transaction_running) {
		int recvsize = -1;
		bool is_timeout = true;
		is_timeout = udprecvfrom(reflector_cp2dpserver_udpsock, buf, MAX_BUFSIZE, 0, &tmp_addr, &tmp_addrlen, recvsize, "reflector.cp2dpserver");

		if (is_timeout) {
			tmp_addrlen = sizeof(struct sockaddr_in);
			continue;
		}

		INVARIANT(recvsize >= 0);

		packet_type_t tmp_optype = packet_type_t(get_packet_type(buf, recvsize));
		// printf("[debug] reflector_cp2dpserver recv %d\n",tmp_optype);fflush(stdout);
		switch (tmp_optype) {
			case packet_type_t::CACHE_POP_INSWITCH:
			case packet_type_t::CACHE_EVICT_LOADFREQ_INSWITCH:
			case packet_type_t::CACHE_EVICT_LOADDATA_INSWITCH:
			case packet_type_t::SETVALID_INSWITCH:
				{

					// Update each time to cope with recovery mode
					mutex_for_popclient_addr.lock();
					memcpy(&reflector_switchos_popworker_popclient_for_reflector_addr, &tmp_addr, sizeof(struct sockaddr_in));
					reflector_switchos_popworker_popclient_for_reflector_addr_len = tmp_addrlen;
					mutex_for_popclient_addr.unlock();
					break;
				}
#ifdef SNAPSHOT_DIST
			case packet_type_t::LOADSNAPSHOTDATA_INSWITCH:
				{
					if (!reflector_with_switchos_snapshotserver_snapshotclient_for_reflector_addr) {
						memcpy(&reflector_switchos_snapshotserver_snapshotclient_for_reflector_addr, &tmp_addr, sizeof(struct sockaddr_in));
						reflector_switchos_snapshotserver_snapshotclient_for_reflector_addr_len = tmp_addrlen;
						reflector_with_switchos_snapshotserver_snapshotclient_for_reflector_addr = true;
					}
					break;
				}
#endif
			default:
				{
					printf("[reflector.cp2dpserver] invalid optype %d\n", int(tmp_optype));
					exit(-1);
				}
		}

		udpsendto(reflector_cp2dpserver_udpsock, buf, recvsize, 0, &client_addr, client_addrlen, "reflector.cp2dpserver");
	}

	close(reflector_cp2dpserver_udpsock);
	pthread_exit(nullptr);
}

// forward pkt from switch to switchos
void *run_reflector_dp2cpserver(void *param) {
	char buf[MAX_BUFSIZE];
	int recvsize = 0;

	struct sockaddr_in reflector_switchos_specialcaseserver_addr;
	set_sockaddr(reflector_switchos_specialcaseserver_addr, inet_addr(switchos_ip), switchos_specialcaseserver_port);
	unsigned int reflector_switchos_specialcaseserver_addr_len = sizeof(struct sockaddr);

	printf("[reflector.dp2cpserver] ready\n");
	transaction_ready_threads++;

	while (!transaction_running) {}

	while (transaction_running) {

		bool is_timeout = udprecvfrom(reflector_dp2cpserver_udpsock, buf, MAX_BUFSIZE, 0, NULL, NULL, recvsize, "reflector.dp2cpserver");
		if (is_timeout) {
			continue;
		}
		INVARIANT(recvsize > 0);

		packet_type_t pkt_type = get_packet_type(buf, recvsize);
		switch (pkt_type) {
			case packet_type_t::CACHE_POP_INSWITCH_ACK:
			case packet_type_t::CACHE_EVICT_LOADFREQ_INSWITCH_ACK:
			case packet_type_t::CACHE_EVICT_LOADDATA_INSWITCH_ACK:
			case packet_type_t::SETVALID_INSWITCH_ACK:
				{
					//INVARIANT(reflector_with_switchos_popworker_popclient_for_reflector_addr == true);
					mutex_for_popclient_addr.lock();
					// NOTE: not use popserver.popclient due to duplicate packets for packet loss issued by switch
					udpsendto(reflector_dp2cpserver_popclient_udpsock, buf, recvsize, 0, &reflector_switchos_popworker_popclient_for_reflector_addr, reflector_switchos_popworker_popclient_for_reflector_addr_len, "reflector.dp2cpserver.popclient");
					mutex_for_popclient_addr.unlock();
					break;
				}
#ifdef SNAPSHOT_DIST
			case packet_type_t::LOADSNAPSHOTDATA_INSWITCH_ACK:
				{
					INVARIANT(reflector_with_switchos_snapshotserver_snapshotclient_for_reflector_addr == true);
					udpsendto(reflector_dp2cpserver_popclient_udpsock, buf, recvsize, 0, &reflector_switchos_snapshotserver_snapshotclient_for_reflector_addr, reflector_switchos_snapshotserver_snapshotclient_for_reflector_addr_len, "reflector.dp2cpserver.popclient");
					break;
				}
#endif
			case packet_type_t::GETRES_LATEST_SEQ_INSWITCH_CASE1:
			case packet_type_t::GETRES_DELETED_SEQ_INSWITCH_CASE1:
			case packet_type_t::PUTREQ_SEQ_INSWITCH_CASE1:
			case packet_type_t::DELREQ_SEQ_INSWITCH_CASE1:
				{
					// send CASE1 to switchos.specialcaseserver
					udpsendto(reflector_dp2cpserver_specialcaseclient_udpsock, buf, recvsize, 0, &reflector_switchos_specialcaseserver_addr, reflector_switchos_specialcaseserver_addr_len, "reflector.dp2cpserver.specialcaseclient");
					break;
				}
			default:
				{
					printf("[reflector.dp2cpserver] invalid packet type: %d\n", int(pkt_type));
					break;
				}
		}
	}

	close(reflector_dp2cpserver_udpsock);
	close(reflector_dp2cpserver_popclient_udpsock);
	pthread_exit(nullptr);
}

void close_reflector() {

}

#endif

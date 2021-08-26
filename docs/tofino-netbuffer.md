## Tofino-based NetBuffer (tofino-netbuffer)

### Implementation Log

- Add tofino-netbuffer
- Add update/register_update.py, controller.cpp, and test_controller.cpp
- Add tofino-netbuffer/controller
	+ Avoid the dependency on cmake 3.5 and gcc/g++ 7.0 (uncompatible in Tofino Debian OS)
- Modify tofino-netbuffer/tofino/basic.p4
	+ Due to outputing up to 32-bit metadata, we must store key/val_lo/hi independently
	+ Add KV component (key_lo/hi, value_lo/hi, and valid bit)
		* Support get
		* Support put (fix the bug with 3 days)
- Modify basic.p4
	+ Update IP length and UDP length when sending back packet from switch to client
- Modify client.cpp, server,cpp, raw_socket.cpp
	+ Use raw socket for sendto and recvfrom in client and server
- Update raw_socket.cpp
	+ Update IP checksum alg and UDP checksum alg
	+ Fix IP checksum error (ihl must be 4 for IPv4)
	+ Fix UDP length error (udp length must be in big endian)
- Update prepare.cpp
	+ To make sure the consistency of existing keys and non-existing keys
- Test correctness under one socket in server
	+ Get pass
		* KV hit pass
		* KV miss pass
	+ Put pass
		* Without eviction pass
		* With eviction pass
			* Modify original packet as put response and send it back
			* Clone a packet as put request for eviction (use PUT_REQ_S to notify servers that it does not require a response)
	+ Del pass
		* STRANGE: client/server gets all requests/responses successfully, while tcpdump cannot capture all packets
			- Reason: raw packet socket listens on a specific interface, which receives all packets
			- Solution: use UDP socket and disable checkusm calculation
- Use MAT to swap MAC address and pass GET/PUT/DEL for multiple threads

### Run

- Prepare randomly-generated keys
	+ `cmake . -DCMAKE_BUILD_TYPE=Release`
	+ `make all`
	+ `./prepare`
	+ NOTE: We must keep the same exist_keys.out and nonexist_keys.out for client/server
- Prepare interface
	+ Use `arp -s <if-ip> <if-mac>` to prepare arp table
	+ Use `ifconfig <if> promisc` to enable promisc mode
		* NOTE: promisc mode only works for packet socket (raw), since it only avoids the drop by interface device but not the ethernet protocol in kernel
- Toifno: run `cd tofino`
	+ Run `su` to enter root account
	+ Run `bash compile.sh` to compile p4 into binary code
	+ Run `bash start_switch.sh` to launch Tofino
	+ Create a new terminal and run `bash configure.sh` to configure data plane
- Run `bash start_server.sh` in server host
- Run `bash start_controller.sh` in Tofino OS
- Run `bash start_client.sh` in client host

## How to debug

- Use `tcpdump -i <if> -e -vvv -X udp` to listen udp packets

## NOTES

- Ports usage
	+ Client: send requests out the ports from 8888 to 8888+fg_n
	+ Storage server: listen client requests from 1111 to 1111+fg_n
	+ NetBuffer controller: listen storage server notification on 2222 (unused)
	+ Ptf: listen controller msg on 3333 (unused)

## Fixed Issues

- XIndex
	+ Run `make microbench` -> Cannot support compile option `-faligned-new`
		* Update g++ from g++-5 to g++-7
			- `sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 10`
			- `sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 10`
			- `sudo add-apt-repository ppa:ubuntu-toolchain-r/test`
			- `sudo apt-get update`
			- `sudo apt-get install g++-7`
			- `sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 20`
			- `sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-7 20`
- OVS + XIndex
	+ NOTE: To support RCU mechanism, each frontend worker must send request one-by-one
	+ NOTE: For template programming, you must implement template class in header file to compile in one time, or explicitly tell the compiler which kinds of specified classes you will use for compiling individually as a library
	+ NOTE: SIGKILL cannot be catched or ignored by custom handler
- Test connectness of tofino by ping
	+ Must set correct IP and mask for each interface
	+ Must enable ports in tofino
	+ Must set correct ARP in hosts
	+ Must keep correct IP checksum, otherwise no ICMP reply
- Tofino + NetBuffer
	+ Get error of "free invalid pointer" after running ptf -> bf_switchd_pd_lib_init: Assertion failed
		* Cannot upgrade g++/gcc to version 7. Rollback to g++/gcc version 4.9 is ok.
	+ Change egress_port as ingress_port to send the packet back: [Error] tofino increases TX packet, but the host cannot receive the packet (RX packet does not rise, and no error packet like CRC, frame, overrun, and dropped)
		* Swap MAC address: still fail
		* Enable PROMISC by `ifconfig <if> promisc`: still fail
		* Use raw socket: still fail
		* Use DPDK instead of kernel: TODO
		* SOLVED!
			- (1) Ethernet's src mac address cannot be the mac address of the receiver's interface (promisc mode only tolerates dst mac address)
			- (2) Tofino hardware error?: Cannot swap src/dst mac address -> no packet will be sent back with a large possibility
				+ Reason: Sequential operation in the same action, i.e., read and write src mac addr
				+ Solution: use MAT to modify mac addr directly; NOTE: give two 48-bit mac addr and two 32-bit ip addr exceeds the parameter limitation of one action in tofino
	+ Errno 22 of bind/sendto: wrong parameter
		* NOTE: sizeof(struct sockaddr) != sizeof(struct sockaddr_ll), you must give precise length
	+ Sth adds two extra bytes (both of zero) at the end of packet: unkown reason
	+ Invalid status of 128
		* Small/big endian is based on byte not bit
	+ If key 0 is evicted by key 1, get key 0 will not arrive storage server
		* Reason: if you do not assign value to condition_hi, it could be either 0 or 1
		* NOTE: predicate is 4-bit instead of 2-bit, which can only be 1/2/4/8
	+ No cloned packet and meta fields are zero
		* Must bind dev port and mirror id in control plane
		* All meta fields will be reset unless those in field list (packet fields are copied)
			- NOTE: clone function just marks a flag without any data dependency; it copies the packet fields at the stage when being invoked; it copies the field list at the end of ingress/egress
	+ Implementation choices
		* UDP socket: Tofino does not support UDP checksum calculation -> UDP checksum error after changing payload -> dropped by UDP socket
			- Solution: disable UDP checksum checking in kernel, or use dpdk and change its code
		* IP socket (raw): can only bind IP address -> must use IP addresses to distinguish different client threads; need to heavily configure ARP table; cannot modify ethernet information and must swap mac address yet it will be dropped by ehternet protocol in kernel stack
		* Packet socket (raw): can only bind a specific interface -> misbehavior under multiple client threads due to receving all packets
	+ NOTE: If reg exceeds the SRAM limitation of tofino, it tries to arrange each table into an individual stage and could cause invalid placement
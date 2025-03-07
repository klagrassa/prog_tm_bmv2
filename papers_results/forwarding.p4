/* -*- P4_16 -*- */
#include <core.p4>
#include <v1model.p4>
// Do a better include
#include "../targets/simple_switch/extern/interface_tm.p4"

/*************************************************************************
This P4 program implements a basic L3 forwarding pipeline with VLAN support.
It includes the following components:

1. Headers: Defines Ethernet, VLAN, and IPv4 headers.
2. Parser: Extracts Ethernet, VLAN, and IPv4 headers from incoming packets.
3. Checksum Verification: Placeholder for checksum verification logic.
4. Ingress Processing: Implements forwarding logic, including actions for
    dropping packets, forwarding IPv4 packets, and interacting with a traffic
    manager interface for rank calculation and predicate evaluation.
5. Egress Processing: Decreases the TTL of IPv4 packets.
6. Checksum Computation: Computes the IPv4 header checksum.
7. Deparser: Reconstructs the packet by emitting the Ethernet and IPv4 headers.

The program uses the V1Switch architecture and includes an external traffic
manager interface for advanced scheduling and queue management.
*************************************************************************/

const bit<16> TYPE_VLAN = 0x8100;
const bit<16> TYPE_IPV4 = 0x800;

/*************************************************************************
*********************** H E A D E R S  ***********************************
*************************************************************************/

typedef bit<9>  egressSpec_t;
typedef bit<48> macAddr_t;
typedef bit<32> ip4Addr_t;

header vlan_t {
    bit<3>  pcp;
    bit<1>  dei;
    bit<12> vid;
    bit<16> etherType;
}

header ethernet_t {
    macAddr_t dstAddr;
    macAddr_t srcAddr;
    bit<16>   etherType;
}

header ipv4_t {
    bit<4>    version;
    bit<4>    ihl;
    bit<8>    diffserv;
    bit<16>   totalLen;
    bit<16>   identification;
    bit<3>    flags;
    bit<13>   fragOffset;
    bit<8>    ttl;
    bit<8>    protocol;
    bit<16>   hdrChecksum;
    ip4Addr_t srcAddr;
    ip4Addr_t dstAddr;
}

struct metadata {
    bit<3>  queue_id;
    bit<3>  color;
}

struct headers {
    ethernet_t   ethernet;
    vlan_t       vlan;
    ipv4_t       ipv4;
}


/*************************************************************************
*********************** P A R S E R  ***********************************
*************************************************************************/

parser MyParser(packet_in packet,
                out headers hdr,
                inout metadata meta,
                inout standard_metadata_t standard_metadata) {
    
    // Start with Ethernet
    state start {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            TYPE_VLAN: parse_vlan;
            TYPE_IPV4: parse_ipv4;
            default: accept;
        }
    }

    state parse_vlan {
        packet.extract(hdr.vlan);
        transition select(hdr.vlan.etherType) {
            TYPE_IPV4: parse_ipv4;
            default: accept;
        }
    }

    state parse_ipv4 {

        packet.extract(hdr.ipv4);
        transition accept;
    }

}


/*************************************************************************
************   C H E C K S U M    V E R I F I C A T I O N   *************
*************************************************************************/

control MyVerifyChecksum(inout headers hdr, inout metadata meta) {
    apply {  }
}


/*************************************************************************
**************  I N G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control MyIngress(inout headers hdr,
                  inout metadata meta,
                  inout standard_metadata_t standard_metadata) {

    // Extern init
    @userextern @name("tm_interface")
    TrafficManagerInterface<bit<32>>() tm_interface;

    // Constants
    bit<32> MAX_DAY = 3;

    // Field indexes
    bit<32> rank_field_index = 0;
    bit<32> id_field_index = 1;
    bit<32> size_field_index = 2;

    // Scheduler variables (alternative to tm_table for prototyping)
    register<bit<32>>(3) rank; // Set to 0 by default in BMv2 target
    register<bit<32>>(1) current_day;

    action drop() {
        mark_to_drop(standard_metadata);
    }

    action ipv4_forward(macAddr_t dstAddr, egressSpec_t port) {

        //set the src mac address as the previous dst, this is not correct right?
        hdr.ethernet.srcAddr = hdr.ethernet.dstAddr;

       //set the destination mac address that we got from the match in the table
        hdr.ethernet.dstAddr = dstAddr;

        //set the output port that we also get from the table
        standard_metadata.egress_spec = port;

        //decrease ttl by 1
        hdr.ipv4.ttl = hdr.ipv4.ttl -1;

    }

    /** FIFO PRIMITIVES **/
    action FIFO_calculate_rank() {
        // Test code for the interface
        bit<32> day; // Find a way to implement calendar queues correctly
        bit<32> temp_rank;

        rank.read(temp_rank, 0);
        current_day.read(day, 0);
        // tm_interface.get_rank(day, rank);
        temp_rank = temp_rank + 1;
        rank.write(0, temp_rank);
        tm_interface.set_rank(day, temp_rank);
    }
    action FIFO_evaluate_predicate() {
        bit<32> day = 0; // Find a way to implement calendar queues correctly
        bit<32> predicate_rank;
 
        tm_interface.get_lowest_priority_for_day(day, predicate_rank);
        // tm_interface.get_lowest_priority(day, predicate_rank);
        tm_interface.set_predicate(day, predicate_rank);
    }
    action FIFO_dequeued() {}
    action FIFO_periodic_timeout() {}

    /** SP COLOR PRIMITIVES **/
    action SP_calculate_rank() {
        bit<32> temp_rank;

        rank.read(temp_rank, (bit<32>)meta.color);
        temp_rank = temp_rank + 1;
        tm_interface.set_rank((bit<32>)meta.color, temp_rank);
        rank.write((bit<32>)meta.color, temp_rank);
    }

    action SP_evaluate_predicate() {
        bit<32> predicate_rank;
        bit<32> temp_day; // Temp variable holding actual day
        bit<32> pred_set; // Keep track of when the pred is set

        /* Polls the high prio Q first then the others */
        tm_interface.find_non_empty_day(0, MAX_DAY, temp_day);

        tm_interface.get_lowest_priority_for_day(temp_day, predicate_rank);
        tm_interface.set_predicate(temp_day, predicate_rank);
    }

    action SP_dequeued() {}

 
    table ipv4_lpm {
        key = {
            hdr.ipv4.dstAddr: lpm;
        }
        actions = {
            ipv4_forward;
            drop;
            NoAction;
            FIFO_calculate_rank;
            FIFO_evaluate_predicate;
            FIFO_dequeued;
            FIFO_periodic_timeout;
            SP_calculate_rank;
            SP_evaluate_predicate;
            SP_dequeued;
        }
        size = 1024;
        default_action = NoAction();
    }

    apply {


        if (hdr.vlan.isValid()) {
            // Extract PCP from VLAN header and set it to meta.queue_id
            meta.queue_id = hdr.vlan.pcp;
            if (hdr.ipv4.diffserv == 0x00 || ((hdr.ipv4.diffserv >= 0x0A) && (hdr.ipv4.diffserv <= 0x0E)))
            {
                meta.color = 2; // Green, low priority
            }
            else if (((hdr.ipv4.diffserv >= 0x1A) && (hdr.ipv4.diffserv <= 0x1E)) || ((hdr.ipv4.diffserv >= 0x12) && (hdr.ipv4.diffserv <= 0x16)))
            {
                meta.color = 1; // Yellow, medium priority
            }
            else if (hdr.ipv4.diffserv == 0x2E || ((hdr.ipv4.diffserv >= 0x22) && (hdr.ipv4.diffserv <= 0x26)))
            {
                meta.color = 0; // Red, high priority
            }
        }

        //only if IPV4 the rule is applied. Therefore other packets will not be forwarded.
        if (hdr.ipv4.isValid()){
            ipv4_lpm.apply();
        }
    }
}

/*************************************************************************
****************  E G R E S S   P R O C E S S I N G   *******************
*************************************************************************/

control MyEgress(inout headers hdr,
                 inout metadata meta,
                 inout standard_metadata_t standard_metadata) {
    action decrease_ttl() {
        hdr.ipv4.ttl = hdr.ipv4.ttl -1;
    }
    apply {  
        if (hdr.ipv4.isValid()){
            decrease_ttl();
        }
    }
}

/*************************************************************************
*************   C H E C K S U M    C O M P U T A T I O N   **************
*************************************************************************/

control MyComputeChecksum(inout headers hdr, inout metadata meta) {
     apply {
	update_checksum(
	    hdr.ipv4.isValid(),
            { hdr.ipv4.version,
	      hdr.ipv4.ihl,
              hdr.ipv4.diffserv,
              hdr.ipv4.totalLen,
              hdr.ipv4.identification,
              hdr.ipv4.flags,
              hdr.ipv4.fragOffset,
              hdr.ipv4.ttl,
              hdr.ipv4.protocol,
              hdr.ipv4.srcAddr,
              hdr.ipv4.dstAddr },
            hdr.ipv4.hdrChecksum,
            HashAlgorithm.csum16);
    }
}


/*************************************************************************
***********************  D E P A R S E R  *******************************
*************************************************************************/

control MyDeparser(packet_out packet, in headers hdr) {
    apply {

        //parsed headers have to be added again into the packet.
        packet.emit(hdr.ethernet);
        packet.emit(hdr.ipv4);

    }
}

/*************************************************************************
***********************  S W I T C H  *******************************
*************************************************************************/

//switch architecture
V1Switch(
MyParser(),
MyVerifyChecksum(),
MyIngress(),
MyEgress(),
MyComputeChecksum(),
MyDeparser()
) main;
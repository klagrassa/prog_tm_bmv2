from scapy.all import *
import subprocess
import random

dscp_values = [0, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 46, 48, 56]
pcp_values = [0, 1, 2, 3, 4, 5, 6, 7]  # Valid PCP values

def create_vlan_packet(dscp, pcp):
    # Randomly choose a DSCP value
    dscp = random.choice(dscp_values)
    # Randomly choose a PCP value
    pcp = random.choice(pcp_values)
    
    # Create an Ethernet frame
    eth = Ether(src="00:00:0a:00:01:01", dst="00:00:0a:00:01:02")
    
    # Create a VLAN tag with the chosen PCP value
    vlan = Dot1Q(prio=pcp, id=0, vlan=100)
    
    # Create an IP packet with the chosen DSCP value
    ip = IP(
        dst="10.0.1.2",  # Destination IP address
        src="10.0.1.1",  # Source IP address
        ttl=64,          # Time to live
        id=random.randint(1, 65535),  # Identification field
        flags="DF",      # Don't Fragment flag
        proto="tcp",     # Protocol (TCP)
        tos=dscp    # DSCP value shifted to the correct position in the TOS field
    )
    
    # Create a TCP segment
    tcp = TCP(
        sport=random.randint(1024, 65535),  # Source port
        dport=80,  # Destination port
        flags="S",  # SYN flag
        seq=random.randint(1, 4294967295)  # Sequence number
    )
    
    # Combine Ethernet, VLAN, IP, and TCP layers to create the full packet
    packet = eth / vlan / ip / tcp
    
    return packet

def main():
    nb_packets = 40
    for i in range(nb_packets):
        # Create a new VLAN packet with random DSCP and PCP values
        packet = create_vlan_packet(dscp_values, pcp_values)
        # packet.show()
        # Send the packet
        sendp(packet, iface="s1-eth1", inter=0.001)
        if i == 19:
            subprocess.run(["nc", "localhost", "41200"], stdin=open("conf_swap_node.json", "r"))
        # sendp(packet, iface="veth0", inter=0.001)

if __name__ == "__main__":
    main()
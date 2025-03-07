# P4 Network Simulation Workspace

This workspace contains files for compiling and simulating a P4 program, configuring a Mininet network topology, and generating test packets with Scapy.

## Project Structure

- **P4 Program and Artifacts**
  - [extern/interface_tm.p4](extern/interface_tm.p4): A P4 extern definition that provides traffic management functions.
  - [extern/interface_tm.so](extern/interface_tm.so): Compiled shared object for the Traffic Manager extern.

- **Network Configuration**
  - [mininet_env.py](mininet_env.py): Python script to set up and run a Mininet network with P4Runtime and BMv2.
  - [switch_commands/s1-commands.txt](switch_commands/s1-commands.txt): CLI commands for the P4 switch, setting default actions and table entries.
  - [topology.json](topology.json): JSON file defining the network topology and node configurations.

- **Packet Capture and Logging**
  - `pcap/`: Directory containing packet capture files (in/out on switch ports).
  - `log/`: Directory with log files from the switch and CLI output. For example, [log/p4s.s1.log](log/p4s.s1.log) and [log/s1_cli_output.log](log/s1_cli_output.log).

- **Test Packet Generation**
  - [pkt_gen.py](pkt_gen.py): A Scapy script to generate and send test packets through a designated interface.
  - `packet_log_in0.csv` and `packet_log_out0.csv`: CSV files for logging packet information.

## How to compile

1. **Configure the project**

  Our prototype were using this configuration : 
  ```sh
  ./configure --enable-tm-debug --with-pi --with-thrift --enable-debugger 'CXXFLAGS=-O0 -g' --with-pdfixed --enable-Werror
  ```
  Enable-tm-debug enables the logs from the traffic manager and the output logs of the nodes.

2. **Execute the build script**
  ```sh
  ./build.sh
  ```
  It requires sudo privileges to execute (it builds AND installs it)

## How to add policies

1. **Define actions primitives**
  Using the example of SP and FIFO, write calculate_rank(), evaluate_predicate(), dequeued() and periodic_timeout().

2. **Add the actions to the forwarding table**
  The prototype is built to fetch actions from here.

3. **You can consult the interface.p4 and targets/simple_switch/extern/ to do additions to the TM's interface if needed**
  If modifying the extern, add the functions declarations to interface.p4 and interface.h.
  Then use ```make clean && make``` to compile the extern.

## How to Run

For reconfiguration :

1. **Export PYTHONPATH to include thrift.py**
```sh
export PYTHONPATH="/home/p4/Documents/p4dev-python-venv/lib/python3.8/site-packages:$PYTHONPATH"
```

2. **Compile the P4 Program**  
   Use the P4 compiler provided in the folder "papers_results". For example:  
   ```sh
   ./p4c-bm2-ss forwarding.p4 --emit-externs
   ```
   or 
   sudo python3 mininet_env.py

   Note: the proposed compiler is new enough to accept extern calls in nested actions' if statement.

3. **Load Switch CLI Commands**

The switch is pre-configured with commands from s1-commands.txt. These commands set default table entries and add rules for IPv4 forwarding.

4. **Generate Test Packets**

Use the Scapy script to send test packets:

This script crafts Ethernet frames with VLAN and IPv4 headers and sends them on the specified interface.

There is the option to reconfigure using a subprocess (with nc, you must suply the port you chose in the traffic_manager.cpp constructor) to execute the reconfiguration.
It can be done after sending n packets.
Also check logs of the switch to know what configuration port is used (you chan change it enqueue()'s traffic_manager.cpp

For validation of scheduling policies

1. **Switch to branch "result_scheduler"**
  ```sh
  git switch result_scheduler
  ```
  Then build the project using "How to build" section.

2. **Use instructions from reconfiguration**
  Artificial backpressure is applied on this branch, you can choose the threshold in "enqueue()" of the node.cpp, line 232.

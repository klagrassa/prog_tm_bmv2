/**
 * @brief TrafficManagerInterface extern.
 * 
 * DISCLAIMER : For prototyping purposes only. Once the langage will support
 * a programmable traffic manager, this extern shouldn't be used anymore.
 * 
 * The TrafficManagerInterface extern provides an interface for managing traffic 
 * through a P4 program. It allows for interaction with registers and scheduler 
 * parameters, enabling the user to read from and write to general purpose registers, 
 * set and get scheduler parameters, and manage packet information.
 * 
 * It includes methods for setting and 
 * getting the rank and predicate registers, which are used for scheduling and 
 * prioritizing packets.
 * 
 * The class also supports adding scheduler parameters, resetting the state, and 
 * setting and getting packet fields. It maintains internal data structures for 
 * general purpose registers, scheduler parameters, and packet information.
 * 
 * The TrafficManagerInterface class is registered as an extern in the BMv2 
 * behavioral model, allowing it to be used in P4 programs.
 * @param <T1> Size of the values put in registers. 32 by default.
 */
extern TrafficManagerInterface <T1> {
    TrafficManagerInterface(); // Always declare the constructor

    // Registered methods
    // Get the number of paramaters registers for the given parameter index
    // Useful to get the number of elements in an array of a parameter
    void get_size_of_parameter(in bit<8> param_index, inout T1 value);

    // Returns the value of the parameter register at the given index
    void get_scheduler_parameter(in bit<8> param_index, in bit<8> reg_index, 
                                inout T1 value);

    // "General Purpose" register of the node
    void write_to_reg(in bit<8> reg_num, in bit<8> index, in T1 value);
    void read_from_reg(in bit<8> reg_num, in bit<8> index, inout T1 value);

    // Set the rank of the actual packet in the form of day,time
    void set_rank(in bit<32> day, in bit<32> time);

    // MIGHT CHANGE/REMOVE
    // Get the last rank of the packet for that day
    void get_rank(in bit<32> day, inout bit<32> value);

    // Set the rank of the packet that is predicate
    void set_predicate(in bit<32> day, in bit<32> time); 

    // Get the lowest priority packet in the queue
    void get_lowest_priority_for_day(in bit<32> day, inout bit<32> value);
    void get_lowest_priority(inout bit<32> day, inout bit<32> value);

    // Set a field for the packet
    void set_field(in bit<32> field_index, in bit<32> value);

    // Retrieve a field for the packet
    void get_field(in bit<32> field_index, inout bit<32> value);

    void has_packets(in bit<32> day, inout bit<1> is_empty);

    void find_next_non_empty_day(in bit<32> day,in bit<32> max_search_day, inout bit<32> next_day);
    
    void find_non_empty_day(in bit<32> day,in bit<32> max_search_day, inout bit<32> next_day);
}
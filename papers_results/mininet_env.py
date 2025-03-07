from p4utils.mininetlib.network_API import NetworkAPI
from p4utils.utils.compiler import P4C

# Compiler part
p4c_bin = './p4c-bm2-ss'
p4c_src = 'forwarding.p4'
additional_opts = '--emit-externs'

# Network part
net = NetworkAPI()

net.setLogLevel('info')
net.enableCli()

# Topology
net.addP4RuntimeSwitch('s1', load_modules='../targets/simple_switch/extern/interface_tm.so')
# net.setP4CliInput('s1', './switch_commands/s1-commands.txt')
net.addHost('h1')
net.addHost('h2')
net.addHost('h3')
net.addHost('h4')

net.setP4Source('s1', p4c_src)
net.setCompiler(P4C, p4c_bin=p4c_bin, p4c_src=p4c_src, opts=f'{additional_opts}')

# Connections
net.addLink('s1', 'h1')
net.addLink('s1', 'h2')
net.addLink('s1', 'h3')
net.addLink('s1', 'h4')

net.setIntfPort('s1', 'h1', 1)  # Set the number of the port on s1 facing h1
net.setIntfPort('h1', 's1', 0)  # Set the number of the port on h1 facing s1
net.setIntfPort('s1', 'h2', 2)  # Set the number of the port on s1 facing h2
net.setIntfPort('h2', 's1', 0)  # Set the number of the port on h2 facing s1
net.setIntfPort('s1', 'h3', 3)  # Set the number of the port on s1 facing h3
net.setIntfPort('h3', 's1', 0)  # Set the number of the port on h3 facing s1
net.setIntfPort('s1', 'h4', 4)  # Set the number of the port on s1 facing h4
net.setIntfPort('h4', 's1', 0)  # Set the number of the port on h4 facing s1


# Address assignment strategy
net.mixed()

# Logging
net.enablePcapDumpAll()
net.enableLogAll()


net.startNetwork()


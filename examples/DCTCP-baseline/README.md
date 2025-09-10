# DCTCP Baseline - Leaf-Spine Topology

This directory contains the implementation of a leaf-spine data center topology for DCTCP baseline experiments.

## Topology Overview

- **Architecture**: Leaf-Spine (Clos Network)
- **Leaf Switches**: 12 nodes
- **Spine Switches**: 6 nodes
- **Servers**: 288 nodes (24 servers per leaf switch)

## Network Structure

```
    Spine Layer (6 switches)
         |  |  |  |  |  |
    ┌────┴──┴──┴──┴──┴────┐
    │                     │
    Leaf Layer (12 switches)
    │  │  │  │  │  │  │  │
  ┌─┴┬─┴┬─┴┬─┴┬─┴┬─┴┬─┴┬─┴┐
  └─┬┴─┬┴─┬┴─┬┴─┬┴─┬┴─┬┴─┬┘
    │  │  │  │  │  │  │  │
   Server Layer (288 servers, 24 per leaf)
```

## Features

- **Full Mesh Connectivity**: Each leaf switch connects to all spine switches
- **Scalable Design**: Easy to modify number of switches and servers
- **IP Addressing**: Automatic IP assignment with proper subnetting
- **Mobility Model**: Positions nodes for NetAnim visualization
- **Example Applications**: UDP echo server/client for testing
- **Animation Support**: Generates XML file for NetAnim visualization

## Files

- `leaf-spine-topology.cc`: Main simulation script
- `CMakeLists.txt`: Build configuration
- `README.md`: This documentation

## Building and Running

1. From the ns-3 root directory:
```bash
./ns3 configure --enable-examples
./ns3 build
```

2. Run the simulation:
```bash
./ns3 run "leaf-spine-topology"
```

## Command Line Options

- `--simulationTime`: Simulation duration in seconds (default: 10.0)
- `--linkBandwidth`: Link bandwidth (default: "10Gbps")
- `--linkDelay`: Link delay (default: "1ms")

Example:
```bash
./ns3 run "leaf-spine-topology --simulationTime=20.0 --linkBandwidth=40Gbps"
```

## Visualization

The simulation generates `leaf-spine-topology.xml` for NetAnim visualization:
- **Red nodes**: Spine switches
- **Green nodes**: Leaf switches  
- **Blue nodes**: Servers

## Network Addressing

- **Leaf-Spine Links**: 10.x.y.0/24 networks
- **Server-Leaf Links**: 192.168.x.y/30 networks

## Customization

To modify the topology:
- Change `numLeafSwitches`, `numSpineSwitches`, or `serversPerLeaf` variables
- Adjust link parameters (bandwidth, delay)
- Add custom applications for specific traffic patterns
- Modify positioning for different visualization layouts

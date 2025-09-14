/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/internet-apps-module.h"
#include "datacenter-workload-generator.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LeafSpineTopology");

/**
 * \brief Leaf-Spine Data Center Topology with Configurable Static ECN Thresholds
 * 
 * This script creates a leaf-spine topology with:
 * - 12 leaf switches
 * - 6 spine switches
 * - 288 servers (24 servers per leaf switch)
 * - Configurable static ECN thresholds (SECN1 and SECN2)
 * 
 * Topology structure:
 * - Each leaf switch connects to all spine switches
 * - Each server connects to one leaf switch
 * - Full bisection bandwidth between leaf and spine layers
 * 
 * ECN Configuration Options:
 * - SECN1: MinTh=5KB, MaxTh=200KB (25Gbps server-leaf links)
 *          MinTh=20KB, MaxTh=800KB (100Gbps leaf-spine links)
 * - SECN2: MinTh=100KB, MaxTh=400KB (25Gbps server-leaf links)
 *          MinTh=400KB, MaxTh=1600KB (100Gbps leaf-spine links)
 * 
 * Usage:
 *   ./ns3 run "leaf-spine-topology --ecnConfig=SECN1"
 *   ./ns3 run "leaf-spine-topology --ecnConfig=SECN2"
 */

int
main (int argc, char *argv[])
{
  // Network configuration parameters
  std::string leafSpineBandwidth = "100Gbps"; // higher bandwidth to support aggregated traffic
  std::string leafSpineDelay = "5us";         // higher delay due to longer physical distance
  std::string serverLeafBandwidth = "25Gbps"; // lower bandwidth for server-leaf links
  std::string serverLeafDelay = "1us";        // lower delay due to shorter physical distance

  // Simulation parameters
  double simulationTime = 10.0; // seconds - longer for traffic analysis
  uint32_t numSpineSwitches = 6;
  uint32_t numLeafSwitches = 12;
  uint32_t serversPerLeaf = 24;
  uint32_t totalServers = numLeafSwitches * serversPerLeaf; // 288 servers
  
  // ECN threshold configuration
  std::string ecnConfig = "SECN1"; // Default to SECN1
  
  // Traffic workload parameters
  std::string workloadType = "WEB_SEARCH"; // WEB_SEARCH or DATA_MINING
  double networkLoad = 0.7; // 70% load by default
  double flowArrivalRate = 200.0; // flows per second
  bool enableRealisticTraffic = true; // Enable realistic traffic patterns
  
  // Parse command line arguments
  CommandLine cmd (__FILE__);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("ecnConfig", "ECN threshold configuration (SECN1 or SECN2)", ecnConfig);
  cmd.AddValue ("workloadType", "Traffic workload type (WEB_SEARCH or DATA_MINING)", workloadType);
  cmd.AddValue ("networkLoad", "Target network load (0.6, 0.7, 0.8, 0.9)", networkLoad);
  cmd.AddValue ("flowArrivalRate", "Average flows per second", flowArrivalRate);
  cmd.AddValue ("enableRealisticTraffic", "Enable realistic traffic patterns", enableRealisticTraffic);
  cmd.Parse (argc, argv);

  // Enable logging
  LogComponentEnable ("LeafSpineTopology", LOG_LEVEL_INFO);
  // LogComponentEnable ("DataCenterWorkloadGenerator", LOG_LEVEL_DEBUG);
  // LogComponentEnable ("TcpSocketBase", LOG_LEVEL_WARN); // Show connection issues
  // LogComponentEnable ("PacketSink", LOG_LEVEL_INFO); // Show sink activity
  
  // Set DCTCP as the default TCP congestion control algorithm
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (ns3::TcpDctcp::GetTypeId ()));

  // TCP socket configuration for better DCTCP performance
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));    // Standard MSS
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));       // Immediate ACKs
  Config::SetDefault ("ns3::TcpSocket::TcpNoDelay", BooleanValue (true));      // Disable Nagle's algorithm
  
  NS_LOG_INFO ("Creating Leaf-Spine Topology");
  NS_LOG_INFO ("Spine switches: " << numSpineSwitches);
  NS_LOG_INFO ("Leaf switches: " << numLeafSwitches);
  NS_LOG_INFO ("Total servers: " << totalServers);
  NS_LOG_INFO ("ECN Configuration: " << ecnConfig);
  NS_LOG_INFO ("TCP congestion control: DCTCP (Data Center TCP)");
  NS_LOG_INFO ("Traffic Configuration:");
  NS_LOG_INFO ("- Workload Type: " << workloadType);
  NS_LOG_INFO ("- Network Load: " << (networkLoad * 100) << "%");
  NS_LOG_INFO ("- Flow Arrival Rate: " << flowArrivalRate << " flows/sec");
  NS_LOG_INFO ("- Realistic Traffic: " << (enableRealisticTraffic ? "Enabled" : "Disabled"));
  NS_LOG_INFO ("- Simulation Time: " << simulationTime << " seconds");

  // Create node containers
  NodeContainer spineSwitches;
  NodeContainer leafSwitches;
  NodeContainer servers;

  // Create nodes
  spineSwitches.Create (numSpineSwitches);
  leafSwitches.Create (numLeafSwitches);
  servers.Create (totalServers);

  NS_LOG_INFO ("Created " << spineSwitches.GetN () << " spine switches");
  NS_LOG_INFO ("Created " << leafSwitches.GetN () << " leaf switches");
  NS_LOG_INFO ("Created " << servers.GetN () << " servers");

  // Configure point-to-point helpers for different link types
  PointToPointHelper leafSpineP2P;
  leafSpineP2P.SetDeviceAttribute ("DataRate", StringValue (leafSpineBandwidth));
  leafSpineP2P.SetChannelAttribute ("Delay", StringValue (leafSpineDelay));
  leafSpineP2P.DisableFlowControl (); // Disable default flow control

  PointToPointHelper serverLeafP2P;
  serverLeafP2P.SetDeviceAttribute ("DataRate", StringValue (serverLeafBandwidth));
  serverLeafP2P.SetChannelAttribute ("Delay", StringValue (serverLeafDelay));
  serverLeafP2P.DisableFlowControl (); // Disable default flow control

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (spineSwitches);
  stack.Install (leafSwitches);
  stack.Install (servers);

  // Configure traffic control and queuing based on ECN configuration
  // ECN threshold calculations:
  // SECN1: MinTh = 5KB, MaxTh = 200KB (for 25Gbps server-leaf links)
  // SECN2: MinTh = 100KB, MaxTh = 400KB (for 25Gbps server-leaf links)
  // For 100Gbps leaf-spine links, scale proportionally (4x bandwidth ratio)
  
  uint32_t serverLeafMinTh_KB, serverLeafMaxTh_KB;
  uint32_t leafSpineMinTh_KB, leafSpineMaxTh_KB;

  uint32_t queueSize_MB = 8; // Set queue size to 8MB for all links
  
  if (ecnConfig == "SECN1")
    {
      // SECN1 configuration
      serverLeafMinTh_KB = 5;    // 5KB
      serverLeafMaxTh_KB = 200;  // 200KB
      // Scale for 100Gbps links (4x the 25Gbps bandwidth)
      leafSpineMinTh_KB = serverLeafMinTh_KB * 4;  // 20KB
      leafSpineMaxTh_KB = serverLeafMaxTh_KB * 4;  // 800KB
    }
  else if (ecnConfig == "SECN2")
    {
      // SECN2 configuration
      serverLeafMinTh_KB = 100;   // 100KB
      serverLeafMaxTh_KB = 400;   // 400KB
      // Scale for 100Gbps links (4x the 25Gbps bandwidth)
      leafSpineMinTh_KB = serverLeafMinTh_KB * 4;  // 400KB
      leafSpineMaxTh_KB = serverLeafMaxTh_KB * 4;  // 1600KB
    }
  else
    {
      NS_FATAL_ERROR ("Invalid ECN configuration. Use SECN1 or SECN2");
    }
  
  // Convert KB to bytes for RED queue configuration
  uint32_t serverLeafMinTh_bytes = serverLeafMinTh_KB * 1024;
  uint32_t serverLeafMaxTh_bytes = serverLeafMaxTh_KB * 1024;
  uint32_t leafSpineMinTh_bytes = leafSpineMinTh_KB * 1024;
  uint32_t leafSpineMaxTh_bytes = leafSpineMaxTh_KB * 1024;
  
  // Set queue size to 8MB for all links
  uint32_t queueSize_bytes = queueSize_MB * 1024 * 1024;  // 8MB in bytes
  
  NS_LOG_INFO ("ECN Threshold Configuration (" << ecnConfig << "):");
  NS_LOG_INFO ("Server-Leaf links (" << serverLeafBandwidth << "):");
  NS_LOG_INFO ("  MinTh: " << serverLeafMinTh_KB << "KB (" << serverLeafMinTh_bytes << " bytes)");
  NS_LOG_INFO ("  MaxTh: " << serverLeafMaxTh_KB << "KB (" << serverLeafMaxTh_bytes << " bytes)");
  NS_LOG_INFO ("  Queue size: " << queueSize_MB << "MB (" << queueSize_bytes << " bytes)");
  NS_LOG_INFO ("Leaf-Spine links (" << leafSpineBandwidth << "):");
  NS_LOG_INFO ("  MinTh: " << leafSpineMinTh_KB << "KB (" << leafSpineMinTh_bytes << " bytes)");
  NS_LOG_INFO ("  MaxTh: " << leafSpineMaxTh_KB << "KB (" << leafSpineMaxTh_bytes << " bytes)");
  NS_LOG_INFO ("  Queue size: " << queueSize_MB << "MB (" << queueSize_bytes << " bytes)");
  
  // Configure RED queue for server-leaf links - Byte mode
  TrafficControlHelper tchServerLeaf;
  tchServerLeaf.SetRootQueueDisc ("ns3::RedQueueDisc",
                                  "MaxSize", StringValue (std::to_string(queueSize_bytes) + "B"),
                                  "MinTh", DoubleValue (serverLeafMinTh_bytes),
                                  "MaxTh", DoubleValue (serverLeafMaxTh_bytes),
                                  "LinkBandwidth", DataRateValue (DataRate (serverLeafBandwidth)),
                                  "LinkDelay", TimeValue (Time (serverLeafDelay)),
                                  "UseEcn", BooleanValue (true));
  
  // Configure RED queue for leaf-spine links - Byte mode
  TrafficControlHelper tchLeafSpine;
  tchLeafSpine.SetRootQueueDisc ("ns3::RedQueueDisc",
                                 "MaxSize", StringValue (std::to_string(queueSize_bytes) + "B"),
                                 "MinTh", DoubleValue (leafSpineMinTh_bytes),
                                 "MaxTh", DoubleValue (leafSpineMaxTh_bytes),
                                 "LinkBandwidth", DataRateValue (DataRate (leafSpineBandwidth)),
                                 "LinkDelay", TimeValue (Time (leafSpineDelay)),
                                 "UseEcn", BooleanValue (true));

  // IP address helper
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");

  // Container to store network devices for different link types
  NetDeviceContainer allDevices;
  NetDeviceContainer leafSpineSwitchDevices; // For leaf-spine link switch devices
  NetDeviceContainer serverLeafSwitchDevices; // For server-leaf link switch devices

  // Connect leaf switches to spine switches (full mesh between layers)
  NS_LOG_INFO ("Creating leaf-to-spine connections...");
  for (uint32_t leafIdx = 0; leafIdx < numLeafSwitches; ++leafIdx)
    {
      for (uint32_t spineIdx = 0; spineIdx < numSpineSwitches; ++spineIdx)
        {
          NodeContainer leafSpineLink;
          leafSpineLink.Add (leafSwitches.Get (leafIdx));
          leafSpineLink.Add (spineSwitches.Get (spineIdx));
          
          NetDeviceContainer devices = leafSpineP2P.Install (leafSpineLink);
          allDevices.Add (devices);
          
          // Add switch devices for leaf-spine links to appropriate container
          leafSpineSwitchDevices.Add (devices.Get (0)); // Leaf switch device
          leafSpineSwitchDevices.Add (devices.Get (1)); // Spine switch device
          
          // Assign IP addresses
          std::ostringstream subnet;
          subnet << "10." << (leafIdx + 1) << "." << (spineIdx + 1) << ".0";
          ipv4.SetBase (subnet.str ().c_str (), "255.255.255.0");
          Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);
          
          NS_LOG_INFO ("Connected Leaf " << leafIdx << " to Spine " << spineIdx 
                      << " with subnet " << subnet.str ());
        }
    }

  // Connect servers to leaf switches
  NS_LOG_INFO ("Creating server-to-leaf connections...");
  for (uint32_t leafIdx = 0; leafIdx < numLeafSwitches; ++leafIdx)
    {
      for (uint32_t serverIdx = 0; serverIdx < serversPerLeaf; ++serverIdx)
        {
          uint32_t globalServerIdx = leafIdx * serversPerLeaf + serverIdx;
          
          NodeContainer serverLeafLink;
          serverLeafLink.Add (servers.Get (globalServerIdx));
          serverLeafLink.Add (leafSwitches.Get (leafIdx));
          
          NetDeviceContainer devices = serverLeafP2P.Install (serverLeafLink);
          allDevices.Add (devices);
          
          // Add only the leaf switch device for server-leaf links
          serverLeafSwitchDevices.Add (devices.Get (1)); // Leaf switch device (server is Get(0))
          
          // Assign IP addresses for server connections
          std::ostringstream subnet;
          subnet << "192.168." << leafIdx << "." << (serverIdx * 4);
          ipv4.SetBase (subnet.str ().c_str (), "255.255.255.252");
          Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);
          
          NS_LOG_INFO ("Connected Server " << globalServerIdx 
                      << " to Leaf " << leafIdx 
                      << " with subnet " << subnet.str ());
        }
    }

  // Install RED queues on switch devices with appropriate configurations
  NS_LOG_INFO ("Installing RED queues on switch devices...");
  
  // Install RED queues on leaf-spine switch devices (100Gbps links)
  QueueDiscContainer leafSpineQueueDiscs = tchLeafSpine.Install (leafSpineSwitchDevices);
  
  // Install RED queues on server-leaf switch devices (25Gbps links)
  QueueDiscContainer serverLeafQueueDiscs = tchServerLeaf.Install (serverLeafSwitchDevices);
  
  NS_LOG_INFO ("RED queue installation summary:");
  NS_LOG_INFO ("- Leaf-spine switch devices with RED queues: " << leafSpineSwitchDevices.GetN ());
  NS_LOG_INFO ("- Server-leaf switch devices with RED queues: " << serverLeafSwitchDevices.GetN ());
  NS_LOG_INFO ("- Total queue disciplines created: " << (leafSpineQueueDiscs.GetN () + serverLeafQueueDiscs.GetN ()));
  NS_LOG_INFO ("ECN marking enabled for " << ecnConfig << " configuration");

  // Populate routing tables
  NS_LOG_INFO ("Populating routing tables...");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Set up mobility model for visualization (optional)
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  
  // Position spine switches at the top
  for (uint32_t i = 0; i < numSpineSwitches; ++i)
    {
      Ptr<ConstantPositionMobilityModel> pos = CreateObject<ConstantPositionMobilityModel> ();
      pos->SetPosition (Vector (i * 50.0, 100.0, 0.0));
      spineSwitches.Get (i)->AggregateObject (pos);
    }
  
  // Position leaf switches in the middle
  for (uint32_t i = 0; i < numLeafSwitches; ++i)
    {
      Ptr<ConstantPositionMobilityModel> pos = CreateObject<ConstantPositionMobilityModel> ();
      pos->SetPosition (Vector (i * 30.0, 50.0, 0.0));
      leafSwitches.Get (i)->AggregateObject (pos);
    }
  
  // Position servers at the bottom
  for (uint32_t i = 0; i < totalServers; ++i)
    {
      uint32_t leafIdx = i / serversPerLeaf;
      uint32_t serverInLeaf = i % serversPerLeaf;
      Ptr<ConstantPositionMobilityModel> pos = CreateObject<ConstantPositionMobilityModel> ();
      pos->SetPosition (Vector (leafIdx * 30.0 + (serverInLeaf % 6) * 5.0, 
                               (serverInLeaf / 6) * 5.0, 0.0));
      servers.Get (i)->AggregateObject (pos);
    }

  // Set up realistic data center traffic workloads
  NS_LOG_INFO ("Setting up realistic data center traffic workloads...");
  
  if (enableRealisticTraffic)
    {
      // Install TCP sink applications on all servers to receive traffic
      uint16_t sinkPort = 8080;
      Address sinkAddress (InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
      PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkAddress);
      ApplicationContainer sinkApps = packetSinkHelper.Install (servers);
      sinkApps.Start (Seconds (0.0));
      sinkApps.Stop (Seconds (simulationTime));
      
      NS_LOG_INFO ("Installed TCP sinks on " << servers.GetN () << " servers");
      
      // Create and configure the realistic workload generator
      Ptr<DataCenterWorkloadGenerator> workloadGenerator = CreateObject<DataCenterWorkloadGenerator> ();
      
      // Set workload type
      WorkloadType wType = WEB_SEARCH;
      if (workloadType == "DATA_MINING")
        {
          wType = DATA_MINING;
        }
      
      workloadGenerator->SetServerNodes (servers);
      workloadGenerator->SetWorkloadType (wType);
      workloadGenerator->SetNetworkLoad (networkLoad);
      workloadGenerator->SetFlowArrivalRate (flowArrivalRate);
      
      // Install the workload generator on the first server (it will generate traffic to all servers)
      servers.Get (0)->AddApplication (workloadGenerator);
      workloadGenerator->SetStartTime (Seconds (1.0));
      workloadGenerator->SetStopTime (Seconds (simulationTime - 1.0));
      
      NS_LOG_INFO ("Configured realistic workload generator:");
      NS_LOG_INFO ("- Type: " << workloadType);
      NS_LOG_INFO ("- Load: " << (networkLoad * 100) << "%");
      NS_LOG_INFO ("- Rate: " << flowArrivalRate << " flows/sec");
      NS_LOG_INFO ("- Expected total flows: ~" << static_cast<uint32_t>(flowArrivalRate * (simulationTime - 2.0)));
    }
  else
    {
      // Fallback to simple UDP echo for basic connectivity testing
      NS_LOG_INFO ("Using simple UDP echo applications for basic testing...");
      
      // Install echo server on first server
      UdpEchoServerHelper echoServer (9);
      ApplicationContainer serverApps = echoServer.Install (servers.Get (0));
      serverApps.Start (Seconds (1.0));
      serverApps.Stop (Seconds (simulationTime));

      // Install echo client on last server
      UdpEchoClientHelper echoClient (servers.Get (0)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), 9);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
      echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

      ApplicationContainer clientApps = echoClient.Install (servers.Get (totalServers - 1));
      clientApps.Start (Seconds (2.0));
      clientApps.Stop (Seconds (simulationTime));
    }

  // Enable tracing (optional)
  // leafSpineP2P.EnablePcapAll ("leaf-spine-links");
  // serverLeafP2P.EnablePcapAll ("server-leaf-links");

  // Create animation file for NetAnim (optional)
  AnimationInterface anim ("leaf-spine-topology.xml");
  anim.SetConstantPosition (servers.Get (0), 0, 0);
  
  // Set node descriptions for better visualization
  for (uint32_t i = 0; i < numSpineSwitches; ++i)
    {
      anim.UpdateNodeDescription (spineSwitches.Get (i), "Spine" + std::to_string (i));
      anim.UpdateNodeColor (spineSwitches.Get (i), 255, 0, 0); // Red for spine
    }
  
  for (uint32_t i = 0; i < numLeafSwitches; ++i)
    {
      anim.UpdateNodeDescription (leafSwitches.Get (i), "Leaf" + std::to_string (i));
      anim.UpdateNodeColor (leafSwitches.Get (i), 0, 255, 0); // Green for leaf
    }
  
  for (uint32_t i = 0; i < totalServers; ++i)
    {
      anim.UpdateNodeDescription (servers.Get (i), "Server" + std::to_string (i));
      anim.UpdateNodeColor (servers.Get (i), 0, 0, 255); // Blue for servers
    }

  NS_LOG_INFO ("Starting simulation for " << simulationTime << " seconds");
  
  // Run simulation
  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_INFO ("Simulation completed successfully");
  NS_LOG_INFO ("Total nodes created: " << numSpineSwitches + numLeafSwitches + totalServers);
  NS_LOG_INFO ("- Spine switches: " << numSpineSwitches);
  NS_LOG_INFO ("- Leaf switches: " << numLeafSwitches);
  NS_LOG_INFO ("- Servers: " << totalServers);
  NS_LOG_INFO ("DCTCP with Realistic Traffic Configuration Complete:");
  NS_LOG_INFO ("- ECN Configuration: " << ecnConfig);
  NS_LOG_INFO ("- RED queues with ECN marking on all switch interfaces");
  NS_LOG_INFO ("- Server-leaf links: " << serverLeafMinTh_KB << "KB-" << serverLeafMaxTh_KB << "KB thresholds");
  NS_LOG_INFO ("- Leaf-spine links: " << leafSpineMinTh_KB << "KB-" << leafSpineMaxTh_KB << "KB thresholds");
  NS_LOG_INFO ("- DCTCP congestion control algorithm set as default");
  NS_LOG_INFO ("- Workload Type: " << workloadType << " at " << (networkLoad * 100) << "% load");
  NS_LOG_INFO ("- High-speed data center network topology with realistic traffic patterns");

  return 0;
}

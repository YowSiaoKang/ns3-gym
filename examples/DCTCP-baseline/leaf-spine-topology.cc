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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LeafSpineTopology");

/**
 * \brief Leaf-Spine Data Center Topology
 * 
 * This script creates a leaf-spine topology with:
 * - 12 leaf switches
 * - 6 spine switches
 * - 288 servers (24 servers per leaf switch)
 * 
 * Topology structure:
 * - Each leaf switch connects to all spine switches
 * - Each server connects to one leaf switch
 * - Full bisection bandwidth between leaf and spine layers
 */

int
main (int argc, char *argv[])
{
  // Simulation parameters
  double simulationTime = 10.0; // seconds
  std::string linkBandwidth = "10Gbps";
  std::string linkDelay = "1ms";
  uint32_t numLeafSwitches = 12;
  uint32_t numSpineSwitches = 6;
  uint32_t serversPerLeaf = 24;
  uint32_t totalServers = numLeafSwitches * serversPerLeaf; // 288 servers

  // Parse command line arguments
  CommandLine cmd (__FILE__);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("linkBandwidth", "Link bandwidth", linkBandwidth);
  cmd.AddValue ("linkDelay", "Link delay", linkDelay);
  cmd.Parse (argc, argv);

  // Enable logging
  LogComponentEnable ("LeafSpineTopology", LOG_LEVEL_INFO);
  
  // Set DCTCP as the default TCP congestion control algorithm
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpDctcp"));
  
  // TCP socket configuration for better DCTCP performance
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));    // Standard MSS
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));       // Immediate ACKs
  Config::SetDefault ("ns3::TcpSocket::TcpNoDelay", BooleanValue (true));      // Disable Nagle's algorithm
  
  NS_LOG_INFO ("Creating Leaf-Spine Topology");
  NS_LOG_INFO ("Leaf switches: " << numLeafSwitches);
  NS_LOG_INFO ("Spine switches: " << numSpineSwitches);
  NS_LOG_INFO ("Total servers: " << totalServers);
  NS_LOG_INFO ("Queue configuration: RED with ECN, 8MB (~5461 packets) buffer size");
  NS_LOG_INFO ("TCP congestion control: DCTCP (Data Center TCP)");

  // Create node containers
  NodeContainer leafSwitches;
  NodeContainer spineSwitches;
  NodeContainer servers;

  // Create nodes
  leafSwitches.Create (numLeafSwitches);
  spineSwitches.Create (numSpineSwitches);
  servers.Create (totalServers);

  NS_LOG_INFO ("Created " << leafSwitches.GetN () << " leaf switches");
  NS_LOG_INFO ("Created " << spineSwitches.GetN () << " spine switches");
  NS_LOG_INFO ("Created " << servers.GetN () << " servers");

  // Configure point-to-point helpers for different link types
  PointToPointHelper leafSpineP2P;
  leafSpineP2P.SetDeviceAttribute ("DataRate", StringValue ("100Gbps"));
  leafSpineP2P.SetChannelAttribute ("Delay", StringValue ("5us"));

  PointToPointHelper serverLeafP2P;
  serverLeafP2P.SetDeviceAttribute ("DataRate", StringValue ("25Gbps"));
  serverLeafP2P.SetChannelAttribute ("Delay", StringValue ("1us"));

  // Configure traffic control and queuing
  // Calculate queue size: 8MB / 1500 bytes ≈ 5461 packets
  uint32_t queueSizePackets = 5461; // 8MB / 1500 bytes MTU
  
  // Configure RED queue for ECN marking on switch interfaces
  TrafficControlHelper tchRed;
  tchRed.SetRootQueueDisc ("ns3::RedQueueDisc",
                           "MaxSize", StringValue (std::to_string(queueSizePackets) + "p"),
                           "MinTh", DoubleValue (queueSizePackets * 0.3),    // 30% threshold
                           "MaxTh", DoubleValue (queueSizePackets * 0.8),    // 80% threshold
                           "LinkBandwidth", DataRateValue (DataRate ("100Gbps")), // Max link rate
                           "LinkDelay", TimeValue (MicroSeconds (5)),
                           "UseEcn", BooleanValue (true));                   // Enable ECN marking

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (leafSwitches);
  stack.Install (spineSwitches);
  stack.Install (servers);

  // IP address helper
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");

  // Container to store all network devices for later reference
  NetDeviceContainer allDevices;

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

  // Install RED queues on all switch interfaces (leaf and spine switches)
  NS_LOG_INFO ("Installing RED queues on switch interfaces...");
  
  // Install RED queues on leaf switches
  for (uint32_t leafIdx = 0; leafIdx < numLeafSwitches; ++leafIdx)
    {
      Ptr<Node> leafNode = leafSwitches.Get (leafIdx);
      for (uint32_t deviceIdx = 0; deviceIdx < leafNode->GetNDevices (); ++deviceIdx)
        {
          Ptr<NetDevice> device = leafNode->GetDevice (deviceIdx);
          if (device->GetTypeId ().GetName () == "ns3::PointToPointNetDevice")
            {
              tchRed.Install (device);
              NS_LOG_INFO ("Installed RED queue on Leaf " << leafIdx << " device " << deviceIdx);
            }
        }
    }
  
  // Install RED queues on spine switches
  for (uint32_t spineIdx = 0; spineIdx < numSpineSwitches; ++spineIdx)
    {
      Ptr<Node> spineNode = spineSwitches.Get (spineIdx);
      for (uint32_t deviceIdx = 0; deviceIdx < spineNode->GetNDevices (); ++deviceIdx)
        {
          Ptr<NetDevice> device = spineNode->GetDevice (deviceIdx);
          if (device->GetTypeId ().GetName () == "ns3::PointToPointNetDevice")
            {
              tchRed.Install (device);
              NS_LOG_INFO ("Installed RED queue on Spine " << spineIdx << " device " << deviceIdx);
            }
        }
    }

  NS_LOG_INFO ("RED queue configuration completed");
  NS_LOG_INFO ("Queue size: " << queueSizePackets << " packets (~8MB with 1500 byte MTU)");
  NS_LOG_INFO ("ECN marking enabled for congestion control");

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

  // Example application setup (echo server/client for testing)
  // Note: Current applications are UDP-based for basic connectivity testing
  // For DCTCP evaluation, replace with TCP-based applications (BulkSendApplication, etc.)
  NS_LOG_INFO ("Setting up test applications...");
  
  // Install echo server on first server
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (servers.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  // Install echo client on last server
  UdpEchoClientHelper echoClient (servers.Get (0)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (servers.Get (totalServers - 1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

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
  NS_LOG_INFO ("Total nodes created: " << NodeList::GetNNodes ());
  NS_LOG_INFO ("- Spine switches: " << numSpineSwitches);
  NS_LOG_INFO ("- Leaf switches: " << numLeafSwitches);
  NS_LOG_INFO ("- Servers: " << totalServers);
  NS_LOG_INFO ("DCTCP baseline configuration complete:");
  NS_LOG_INFO ("- RED queues with ECN marking on all switch interfaces");
  NS_LOG_INFO ("- DCTCP congestion control algorithm set as default");
  NS_LOG_INFO ("- High-speed data center network topology established");

  return 0;
}

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

#include "datacenter-workload-generator.h"
#include "ns3/log.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/simulator.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-address.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("DataCenterWorkloadGenerator");
NS_OBJECT_ENSURE_REGISTERED (DataCenterWorkloadGenerator);

TypeId
DataCenterWorkloadGenerator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DataCenterWorkloadGenerator")
    .SetParent<Application> ()
    .SetGroupName ("Applications")
    .AddConstructor<DataCenterWorkloadGenerator> ()
    .AddAttribute ("NetworkLoad",
                   "Target network load as fraction (0.6 = 60%)",
                   DoubleValue (0.7),
                   MakeDoubleAccessor (&DataCenterWorkloadGenerator::m_networkLoad),
                   MakeDoubleChecker<double> (0.0, 1.0))
    .AddAttribute ("FlowArrivalRate",
                   "Average number of flows per second",
                   DoubleValue (100.0),
                   MakeDoubleAccessor (&DataCenterWorkloadGenerator::m_flowArrivalRate),
                   MakeDoubleChecker<double> (0.0))
    .AddAttribute ("BasePort",
                   "Base port number for connections",
                   UintegerValue (8080),
                   MakeUintegerAccessor (&DataCenterWorkloadGenerator::m_basePort),
                   MakeUintegerChecker<uint16_t> ())
    .AddTraceSource ("FlowStarted",
                     "A new flow has started",
                     MakeTraceSourceAccessor (&DataCenterWorkloadGenerator::m_flowStarted),
                     "ns3::DataCenterWorkloadGenerator::FlowStartedCallback")
    .AddTraceSource ("FlowCompleted",
                     "A flow has completed",
                     MakeTraceSourceAccessor (&DataCenterWorkloadGenerator::m_flowCompleted),
                     "ns3::DataCenterWorkloadGenerator::FlowCompletedCallback")
    .AddTraceSource ("BytesSent",
                     "Bytes sent by the application",
                     MakeTraceSourceAccessor (&DataCenterWorkloadGenerator::m_bytesSent),
                     "ns3::DataCenterWorkloadGenerator::BytesSentCallback")
  ;
  return tid;
}

DataCenterWorkloadGenerator::DataCenterWorkloadGenerator ()
  : m_workloadType (WEB_SEARCH),
    m_networkLoad (0.7),
    m_flowArrivalRate (100.0),
    m_basePort (8080),
    m_nextFlowId (1),
    m_completedFlows (0),
    m_totalBytesSent (0),
    m_running (false)
{
  NS_LOG_FUNCTION (this);
  
  // Initialize random variables
  m_flowArrivalRv = CreateObject<ExponentialRandomVariable> ();
  m_serverSelectRv = CreateObject<UniformRandomVariable> ();
  m_portSelectRv = CreateObject<UniformRandomVariable> ();
  
  // Web Search workload: Heavy-tailed with small average flow size
  // Pareto distribution with shape=1.2, scale=6KB (empirical data center measurements)
  m_webSearchFlowSizeRv = CreateObject<ParetoRandomVariable> ();
  m_webSearchFlowSizeRv->SetAttribute ("Shape", DoubleValue (1.2));
  m_webSearchFlowSizeRv->SetAttribute ("Scale", DoubleValue (6144)); // 6KB
  
  // Data Mining workload: Heavy-tailed with larger average flow size
  // Pareto distribution with shape=1.1, scale=1MB
  m_dataMiningFlowSizeRv = CreateObject<ParetoRandomVariable> ();
  m_dataMiningFlowSizeRv->SetAttribute ("Shape", DoubleValue (1.1));
  m_dataMiningFlowSizeRv->SetAttribute ("Scale", DoubleValue (1048576)); // 1MB
  
  // Mixed workload using LogNormal distribution
  m_mixedFlowSizeRv = CreateObject<LogNormalRandomVariable> ();
  m_mixedFlowSizeRv->SetAttribute ("Mu", DoubleValue (10.0));    // Mean of log
  m_mixedFlowSizeRv->SetAttribute ("Sigma", DoubleValue (2.0));  // Std dev of log
}

DataCenterWorkloadGenerator::~DataCenterWorkloadGenerator ()
{
  NS_LOG_FUNCTION (this);
}

void
DataCenterWorkloadGenerator::SetServerNodes (NodeContainer servers)
{
  NS_LOG_FUNCTION (this);
  m_servers = servers;
  
  // Configure server selection random variable
  m_serverSelectRv->SetAttribute ("Min", DoubleValue (0.0));
  m_serverSelectRv->SetAttribute ("Max", DoubleValue (servers.GetN ()));
}

void
DataCenterWorkloadGenerator::SetWorkloadType (WorkloadType workloadType)
{
  NS_LOG_FUNCTION (this << workloadType);
  m_workloadType = workloadType;
}

void
DataCenterWorkloadGenerator::SetNetworkLoad (double load)
{
  NS_LOG_FUNCTION (this << load);
  m_networkLoad = load;
}

void
DataCenterWorkloadGenerator::SetFlowArrivalRate (double rate)
{
  NS_LOG_FUNCTION (this << rate);
  m_flowArrivalRate = rate;
  
  // Update the exponential random variable for flow arrivals
  // Rate parameter for exponential distribution (lambda)
  m_flowArrivalRv->SetAttribute ("Mean", DoubleValue (1.0 / rate));
}

uint32_t
DataCenterWorkloadGenerator::GetActiveFlows () const
{
  return m_activeFlows.size ();
}

uint32_t
DataCenterWorkloadGenerator::GetCompletedFlows () const
{
  return m_completedFlows;
}

uint64_t
DataCenterWorkloadGenerator::GetTotalBytesSent () const
{
  return m_totalBytesSent;
}

void
DataCenterWorkloadGenerator::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  
  // Cancel any pending events
  if (m_nextFlowEvent.IsRunning ())
    {
      Simulator::Cancel (m_nextFlowEvent);
    }
  
  // Close all active sockets
  for (auto& pair : m_activeFlows)
    {
      if (pair.second.socket)
        {
          pair.second.socket->Close ();
        }
    }
  
  m_activeFlows.clear ();
  m_socketToFlow.clear ();
  
  Application::DoDispose ();
}

void
DataCenterWorkloadGenerator::StartApplication (void)
{
  NS_LOG_FUNCTION (this);
  
  if (m_servers.GetN () == 0)
    {
      NS_FATAL_ERROR ("No server nodes configured");
    }
  
  m_running = true;
  
  // Configure port selection range
  m_portSelectRv->SetAttribute ("Min", DoubleValue (m_basePort));
  m_portSelectRv->SetAttribute ("Max", DoubleValue (m_basePort + 1000));
  
  NS_LOG_INFO ("Starting DataCenter Workload Generator");
  NS_LOG_INFO ("- Workload Type: " << (m_workloadType == WEB_SEARCH ? "Web Search" : "Data Mining"));
  NS_LOG_INFO ("- Network Load: " << (m_networkLoad * 100) << "%");
  NS_LOG_INFO ("- Flow Arrival Rate: " << m_flowArrivalRate << " flows/sec");
  NS_LOG_INFO ("- Server Count: " << m_servers.GetN ());
  
  // Schedule first flow
  ScheduleNextFlow ();
}

void
DataCenterWorkloadGenerator::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  
  m_running = false;
  
  // Cancel pending flow generation
  if (m_nextFlowEvent.IsRunning ())
    {
      Simulator::Cancel (m_nextFlowEvent);
    }
  
  // Close all active flows
  for (auto& pair : m_activeFlows)
    {
      if (pair.second.socket)
        {
          pair.second.socket->Close ();
        }
    }
  
  NS_LOG_INFO ("DataCenter Workload Generator stopped");
  NS_LOG_INFO ("- Total flows completed: " << m_completedFlows);
  NS_LOG_INFO ("- Total bytes sent: " << m_totalBytesSent);
  NS_LOG_INFO ("- Active flows at stop: " << m_activeFlows.size ());
}

void
DataCenterWorkloadGenerator::ScheduleNextFlow ()
{
  NS_LOG_FUNCTION (this);
  
  if (!m_running)
    {
      return;
    }
  
  Time nextTime = CalculateNextArrivalTime ();
  m_nextFlowEvent = Simulator::Schedule (nextTime, &DataCenterWorkloadGenerator::GenerateFlow, this);
  
  NS_LOG_DEBUG ("Scheduled next flow in " << nextTime.GetSeconds () << " seconds");
}

void
DataCenterWorkloadGenerator::GenerateFlow ()
{
  NS_LOG_FUNCTION (this);
  
  if (!m_running)
    {
      return;
    }
  
  // Create new flow
  FlowInfo flowInfo;
  flowInfo.flowId = m_nextFlowId++;
  flowInfo.flowSize = GenerateFlowSize ();
  flowInfo.startTime = Simulator::Now ();
  flowInfo.bytesSent = 0;
  flowInfo.isActive = true;
  
  // Select random source and destination
  SelectRandomEndpoints (flowInfo.sourceNodeId, flowInfo.destNodeId);
  
  // Configure port selection range - use only the base port for now
  // All sinks are listening on the same port, so use that port only
  flowInfo.port = m_basePort; // Use the base port directly instead of random selection
  
  NS_LOG_INFO ("Generating flow " << flowInfo.flowId 
               << " from server " << flowInfo.sourceNodeId 
               << " to server " << flowInfo.destNodeId
               << ", size: " << flowInfo.flowSize << " bytes"
               << ", port: " << flowInfo.port);
  
  // Start the flow
  StartFlow (flowInfo);
  
  // Schedule next flow
  ScheduleNextFlow ();
}

uint64_t
DataCenterWorkloadGenerator::GenerateFlowSize ()
{
  uint64_t flowSize = 0;
  
  switch (m_workloadType)
    {
    case WEB_SEARCH:
      {
        // Web search: Mostly small flows with heavy tail
        // 90% small flows (< 100KB), 10% large flows (can be several MB)
        double prob = m_serverSelectRv->GetValue ();
        
        if (prob < 0.9) // 90% small flows
          {
            flowSize = static_cast<uint64_t> (m_webSearchFlowSizeRv->GetValue ());
            // Cap small flows at 100KB
            flowSize = std::min (flowSize, static_cast<uint64_t> (102400));
          }
        else // 10% large flows
          {
            flowSize = static_cast<uint64_t> (m_webSearchFlowSizeRv->GetValue ()) * 100;
            // Large flows can be 1-10MB
            flowSize = std::max (flowSize, static_cast<uint64_t> (1048576));  // Min 1MB
            flowSize = std::min (flowSize, static_cast<uint64_t> (10485760)); // Max 10MB
          }
        break;
      }
    case DATA_MINING:
      {
        // Data mining: Larger flows on average, still heavy-tailed
        flowSize = static_cast<uint64_t> (m_dataMiningFlowSizeRv->GetValue ());
        // Data mining flows typically 100KB - 100MB
        flowSize = std::max (flowSize, static_cast<uint64_t> (102400));    // Min 100KB
        flowSize = std::min (flowSize, static_cast<uint64_t> (104857600)); // Max 100MB
        break;
      }
    default:
      flowSize = 65536; // Default 64KB
      break;
    }
  
  // Ensure minimum flow size (1KB)
  flowSize = std::max (flowSize, static_cast<uint64_t> (1024));
  
  return flowSize;
}

void
DataCenterWorkloadGenerator::SelectRandomEndpoints (uint32_t &sourceId, uint32_t &destId)
{
  // Select random source
  sourceId = static_cast<uint32_t> (m_serverSelectRv->GetValue ());
  if (sourceId >= m_servers.GetN ())
    {
      sourceId = m_servers.GetN () - 1;
    }
  
  // Select random destination (different from source)
  do {
    destId = static_cast<uint32_t> (m_serverSelectRv->GetValue ());
    if (destId >= m_servers.GetN ())
      {
        destId = m_servers.GetN () - 1;
      }
  } while (destId == sourceId && m_servers.GetN () > 1);
}

void
DataCenterWorkloadGenerator::StartFlow (FlowInfo &flowInfo)
{
  NS_LOG_FUNCTION (this << flowInfo.flowId);
  
  // Get source and destination nodes
  Ptr<Node> sourceNode = m_servers.Get (flowInfo.sourceNodeId);
  Ptr<Node> destNode = m_servers.Get (flowInfo.destNodeId);
  
  // Get destination IP address - find the first non-loopback interface
  Ptr<Ipv4> destIpv4 = destNode->GetObject<Ipv4> ();
  Ipv4Address destAddr;
  bool foundValidAddress = false;
  
  // Search through all interfaces to find a valid one (skip interface 0 which is loopback)
  for (uint32_t i = 1; i < destIpv4->GetNInterfaces (); ++i)
    {
      if (destIpv4->IsUp (i) && destIpv4->GetNAddresses (i) > 0)
        {
          destAddr = destIpv4->GetAddress (i, 0).GetLocal ();
          foundValidAddress = true;
          NS_LOG_DEBUG ("Found valid destination address " << destAddr << " on interface " << i);
          break;
        }
    }
  
  if (!foundValidAddress)
    {
      NS_LOG_ERROR ("No valid IP address found for destination node " << flowInfo.destNodeId);
      return;
    }
  
  // Create TCP socket
  Ptr<Socket> socket = Socket::CreateSocket (sourceNode, TcpSocketFactory::GetTypeId ());
  
  // Set socket callbacks
  socket->SetConnectCallback (
    MakeCallback (&DataCenterWorkloadGenerator::ConnectionSucceeded, this),
    MakeCallback (&DataCenterWorkloadGenerator::ConnectionFailed, this));
  
  socket->SetSendCallback (
    MakeCallback (&DataCenterWorkloadGenerator::SendData, this));
  
  // Store flow information
  flowInfo.socket = socket;
  m_activeFlows[flowInfo.flowId] = flowInfo;
  m_socketToFlow[socket] = flowInfo.flowId;
  
  // Connect to destination
  InetSocketAddress destAddress (destAddr, flowInfo.port);
  socket->Connect (destAddress);
  
  // Fire trace
  m_flowStarted (flowInfo.flowId, flowInfo.flowSize);
  
  NS_LOG_DEBUG ("Started flow " << flowInfo.flowId 
                << " connecting to " << destAddr << ":" << flowInfo.port);
}

void
DataCenterWorkloadGenerator::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  
  auto it = m_socketToFlow.find (socket);
  if (it != m_socketToFlow.end ())
    {
      uint32_t flowId = it->second;
      NS_LOG_DEBUG ("Connection succeeded for flow " << flowId);
      
      // Start sending data immediately
      SendData (socket, socket->GetTxAvailable ());
    }
}

void
DataCenterWorkloadGenerator::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  
  auto it = m_socketToFlow.find (socket);
  if (it != m_socketToFlow.end ())
    {
      uint32_t flowId = it->second;
      NS_LOG_WARN ("Connection failed for flow " << flowId);
      
      // Clean up failed flow
      FlowCompleted (flowId);
    }
}

void
DataCenterWorkloadGenerator::SendData (Ptr<Socket> socket, uint32_t availableBufferSize)
{
  NS_LOG_FUNCTION (this << socket << availableBufferSize);
  
  auto it = m_socketToFlow.find (socket);
  if (it == m_socketToFlow.end ())
    {
      return;
    }
  
  uint32_t flowId = it->second;
  auto flowIt = m_activeFlows.find (flowId);
  if (flowIt == m_activeFlows.end ())
    {
      return;
    }
  
  FlowInfo &flowInfo = flowIt->second;
  
  // Calculate how much data to send
  uint64_t remainingBytes = flowInfo.flowSize - flowInfo.bytesSent;
  uint32_t toSend = std::min (static_cast<uint64_t> (availableBufferSize), remainingBytes);
  
  if (toSend > 0)
    {
      // Create and send packet
      Ptr<Packet> packet = Create<Packet> (toSend);
      int actual = socket->Send (packet);
      
      if (actual > 0)
        {
          flowInfo.bytesSent += actual;
          m_totalBytesSent += actual;
          
          // Fire trace
          m_bytesSent (actual);
          
          NS_LOG_DEBUG ("Flow " << flowId << " sent " << actual 
                        << " bytes, total: " << flowInfo.bytesSent 
                        << "/" << flowInfo.flowSize);
        }
    }
  
  // Check if flow is complete
  if (flowInfo.bytesSent >= flowInfo.flowSize)
    {
      NS_LOG_DEBUG ("Flow " << flowId << " completed");
      FlowCompleted (flowId);
    }
}

void
DataCenterWorkloadGenerator::FlowCompleted (uint32_t flowId)
{
  NS_LOG_FUNCTION (this << flowId);
  
  auto flowIt = m_activeFlows.find (flowId);
  if (flowIt == m_activeFlows.end ())
    {
      return;
    }
  
  FlowInfo &flowInfo = flowIt->second;
  
  // Close socket
  if (flowInfo.socket)
    {
      flowInfo.socket->Close ();
      m_socketToFlow.erase (flowInfo.socket);
    }
  
  // Update statistics
  m_completedFlows++;
  
  // Fire trace
  m_flowCompleted (flowId, flowInfo.bytesSent);
  
  NS_LOG_INFO ("Flow " << flowId << " completed, sent " 
               << flowInfo.bytesSent << "/" << flowInfo.flowSize << " bytes");
  
  // Remove from active flows
  m_activeFlows.erase (flowIt);
}

Time
DataCenterWorkloadGenerator::CalculateNextArrivalTime ()
{
  // Exponential inter-arrival times (Poisson process)
  double intervalSeconds = m_flowArrivalRv->GetValue ();
  
  // Apply load factor to adjust arrival rate
  // Higher load means shorter intervals between flows
  intervalSeconds /= m_networkLoad;
  
  return Seconds (intervalSeconds);
}

} // namespace ns3

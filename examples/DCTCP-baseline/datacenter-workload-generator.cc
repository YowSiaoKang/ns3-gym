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
                   "Average number of connections per second",
                   DoubleValue (100.0),
                   MakeDoubleAccessor (&DataCenterWorkloadGenerator::m_flowArrivalRate),
                   MakeDoubleChecker<double> (0.0))
    .AddAttribute ("BasePort",
                   "Base port number for connections",
                   UintegerValue (8080),
                   MakeUintegerAccessor (&DataCenterWorkloadGenerator::m_basePort),
                   MakeUintegerChecker<uint16_t> ())
  ;
  return tid;
}

DataCenterWorkloadGenerator::DataCenterWorkloadGenerator ()
  : m_workloadType (WEB_SEARCH),
    m_networkLoad (0.7),
    m_flowArrivalRate (100.0),
    m_basePort (8080),
    m_nextConnectionId (1),
    m_running (false)
{
  NS_LOG_FUNCTION (this);
  
  // Initialize random variables
  m_connectionArrivalRv = CreateObject<ExponentialRandomVariable> ();
  m_serverSelectRv = CreateObject<UniformRandomVariable> ();
  
  // Web Search workload: Heavy-tailed with small average transfer size
  // Pareto distribution with shape=1.2, scale=6KB (empirical data center measurements)
  m_webSearchSizeRv = CreateObject<ParetoRandomVariable> ();
  m_webSearchSizeRv->SetAttribute ("Shape", DoubleValue (1.2));
  m_webSearchSizeRv->SetAttribute ("Scale", DoubleValue (6144)); // 6KB
  
  // Data Mining workload: Heavy-tailed with larger average transfer size
  // Pareto distribution with shape=1.1, scale=1MB
  m_dataMiningSizeRv = CreateObject<ParetoRandomVariable> ();
  m_dataMiningSizeRv->SetAttribute ("Shape", DoubleValue (1.1));
  m_dataMiningSizeRv->SetAttribute ("Scale", DoubleValue (1048576)); // 1MB
  
  // Mixed workload using LogNormal distribution
  m_mixedSizeRv = CreateObject<LogNormalRandomVariable> ();
  m_mixedSizeRv->SetAttribute ("Mu", DoubleValue (10.0));    // Mean of log
  m_mixedSizeRv->SetAttribute ("Sigma", DoubleValue (2.0));  // Std dev of log
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
  
  // Update the exponential random variable for connection arrivals
  // Rate parameter for exponential distribution (lambda)
  m_connectionArrivalRv->SetAttribute ("Mean", DoubleValue (1.0 / rate));
}

void
DataCenterWorkloadGenerator::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  
  // Cancel any pending events
  if (m_nextConnectionEvent.IsRunning ())
    {
      Simulator::Cancel (m_nextConnectionEvent);
    }
  
  // Close all active sockets
  for (auto& pair : m_activeConnections)
    {
      if (pair.second.socket)
        {
          pair.second.socket->Close ();
        }
    }
  
  m_activeConnections.clear ();
  m_socketToConnection.clear ();
  
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
  
  NS_LOG_INFO ("Starting DataCenter Workload Generator");
  NS_LOG_INFO ("- Workload Type: " << (m_workloadType == WEB_SEARCH ? "Web Search" : "Data Mining"));
  NS_LOG_INFO ("- Network Load: " << (m_networkLoad * 100) << "%");
  NS_LOG_INFO ("- Flow Arrival Rate: " << m_flowArrivalRate << " connections/sec");
  NS_LOG_INFO ("- Server Count: " << m_servers.GetN ());
  NS_LOG_INFO ("- Base Port: " << m_basePort << " (all connections use this port)");
  
  // Schedule first connection
  ScheduleNextConnection ();
}

void
DataCenterWorkloadGenerator::StopApplication (void)
{
  NS_LOG_FUNCTION (this);
  
  m_running = false;
  
  // Cancel pending connection generation
  if (m_nextConnectionEvent.IsRunning ())
    {
      Simulator::Cancel (m_nextConnectionEvent);
    }
  
  // Close all active connections
  for (auto& pair : m_activeConnections)
    {
      if (pair.second.socket)
        {
          pair.second.socket->Close ();
        }
    }
  
  NS_LOG_INFO ("DataCenter Workload Generator stopped");
}

void
DataCenterWorkloadGenerator::ScheduleNextConnection ()
{
  NS_LOG_FUNCTION (this);
  
  if (!m_running)
    {
      return;
    }
  
  Time nextTime = CalculateNextArrivalTime ();
  m_nextConnectionEvent = Simulator::Schedule (nextTime, &DataCenterWorkloadGenerator::GenerateConnection, this);
  
  NS_LOG_DEBUG ("Scheduled next connection in " << nextTime.GetSeconds () << " seconds");
}

void
DataCenterWorkloadGenerator::GenerateConnection ()
{
  NS_LOG_FUNCTION (this);
  
  if (!m_running)
    {
      return;
    }
  
  // Create new connection
  ConnectionInfo connectionInfo;
  connectionInfo.connectionId = m_nextConnectionId++;
  connectionInfo.targetBytes = GenerateTransferSize ();
  connectionInfo.bytesSent = 0;
  
  // Select random source and destination
  uint32_t sourceNodeId, destNodeId;
  SelectRandomEndpoints (sourceNodeId, destNodeId);
  
  NS_LOG_INFO ("Generating connection " << connectionInfo.connectionId 
               << " from server " << sourceNodeId 
               << " to server " << destNodeId
               << ", size: " << connectionInfo.targetBytes << " bytes"
               << ", port: " << m_basePort);
  
  // Start the connection
  StartConnection (connectionInfo, sourceNodeId, destNodeId);
  
  // Schedule next connection
  ScheduleNextConnection ();
}

uint64_t
DataCenterWorkloadGenerator::GenerateTransferSize ()
{
  uint64_t transferSize = 0;
  
  switch (m_workloadType)
    {
    case WEB_SEARCH:
      {
        // Web search: Mostly small transfers with heavy tail
        // 90% small transfers (< 100KB), 10% large transfers (can be several MB)
        double prob = m_serverSelectRv->GetValue ();
        
        if (prob < 0.9) // 90% small transfers
          {
            transferSize = static_cast<uint64_t> (m_webSearchSizeRv->GetValue ());
            // Cap small transfers at 100KB
            transferSize = std::min (transferSize, static_cast<uint64_t> (102400));
          }
        else // 10% large transfers
          {
            transferSize = static_cast<uint64_t> (m_webSearchSizeRv->GetValue ()) * 100;
            // Large transfers can be 1-10MB
            transferSize = std::max (transferSize, static_cast<uint64_t> (1048576));  // Min 1MB
            transferSize = std::min (transferSize, static_cast<uint64_t> (10485760)); // Max 10MB
          }
        break;
      }
    case DATA_MINING:
      {
        // Data mining: Larger transfers on average, still heavy-tailed
        transferSize = static_cast<uint64_t> (m_dataMiningSizeRv->GetValue ());
        // Data mining transfers typically 100KB - 100MB
        transferSize = std::max (transferSize, static_cast<uint64_t> (102400));    // Min 100KB
        transferSize = std::min (transferSize, static_cast<uint64_t> (104857600)); // Max 100MB
        break;
      }
    default:
      transferSize = 65536; // Default 64KB
      break;
    }
  
  // Ensure minimum transfer size (1KB)
  transferSize = std::max (transferSize, static_cast<uint64_t> (1024));
  
  return transferSize;
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
DataCenterWorkloadGenerator::StartConnection (ConnectionInfo &connectionInfo, uint32_t sourceNodeId, uint32_t destNodeId)
{
  NS_LOG_FUNCTION (this << connectionInfo.connectionId);
  
  // Get source and destination nodes
  Ptr<Node> sourceNode = m_servers.Get (sourceNodeId);
  Ptr<Node> destNode = m_servers.Get (destNodeId);
  
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
      NS_LOG_ERROR ("No valid IP address found for destination node " << destNodeId);
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
  
  // Store connection information
  connectionInfo.socket = socket;
  m_activeConnections[connectionInfo.connectionId] = connectionInfo;
  m_socketToConnection[socket] = connectionInfo.connectionId;
  
  // Connect to destination
  InetSocketAddress destAddress (destAddr, m_basePort);
  socket->Connect (destAddress);
  
  NS_LOG_DEBUG ("Started connection " << connectionInfo.connectionId 
                << " connecting to " << destAddr << ":" << m_basePort);
}

void
DataCenterWorkloadGenerator::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  
  auto it = m_socketToConnection.find (socket);
  if (it != m_socketToConnection.end ())
    {
      uint32_t connectionId = it->second;
      NS_LOG_DEBUG ("Connection succeeded for connection " << connectionId);
      
      // Start sending data immediately
      SendData (socket, socket->GetTxAvailable ());
    }
}

void
DataCenterWorkloadGenerator::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  
  auto it = m_socketToConnection.find (socket);
  if (it != m_socketToConnection.end ())
    {
      uint32_t connectionId = it->second;
      NS_LOG_WARN ("Connection failed for connection " << connectionId);
      
      // Clean up failed connection
      ConnectionCompleted (connectionId);
    }
}

void
DataCenterWorkloadGenerator::SendData (Ptr<Socket> socket, uint32_t availableBufferSize)
{
  NS_LOG_FUNCTION (this << socket << availableBufferSize);
  
  auto it = m_socketToConnection.find (socket);
  if (it == m_socketToConnection.end ())
    {
      return;
    }
  
  uint32_t connectionId = it->second;
  auto connIt = m_activeConnections.find (connectionId);
  if (connIt == m_activeConnections.end ())
    {
      return;
    }
  
  ConnectionInfo &connectionInfo = connIt->second;
  
  // Calculate how much data to send
  uint64_t remainingBytes = connectionInfo.targetBytes - connectionInfo.bytesSent;
  uint32_t toSend = std::min (static_cast<uint64_t> (availableBufferSize), remainingBytes);
  
  if (toSend > 0)
    {
      // Create and send packet
      Ptr<Packet> packet = Create<Packet> (toSend);
      int actual = socket->Send (packet);
      
      if (actual > 0)
        {
          connectionInfo.bytesSent += actual;
          
          NS_LOG_DEBUG ("Connection " << connectionId << " sent " << actual 
                        << " bytes, total: " << connectionInfo.bytesSent 
                        << "/" << connectionInfo.targetBytes);
        }
    }
  
  // Check if connection is complete
  if (connectionInfo.bytesSent >= connectionInfo.targetBytes)
    {
      NS_LOG_DEBUG ("Connection " << connectionId << " completed");
      ConnectionCompleted (connectionId);
    }
}

void
DataCenterWorkloadGenerator::ConnectionCompleted (uint32_t connectionId)
{
  NS_LOG_FUNCTION (this << connectionId);
  
  auto connIt = m_activeConnections.find (connectionId);
  if (connIt == m_activeConnections.end ())
    {
      return;
    }
  
  ConnectionInfo &connectionInfo = connIt->second;
  
  // Close socket
  if (connectionInfo.socket)
    {
      connectionInfo.socket->Close ();
      m_socketToConnection.erase (connectionInfo.socket);
    }
  
  NS_LOG_INFO ("Connection " << connectionId << " completed, sent " 
               << connectionInfo.bytesSent << "/" << connectionInfo.targetBytes << " bytes");
  
  // Remove from active connections
  m_activeConnections.erase (connIt);
}

Time
DataCenterWorkloadGenerator::CalculateNextArrivalTime ()
{
  // Exponential inter-arrival times (Poisson process)
  double intervalSeconds = m_connectionArrivalRv->GetValue ();
  
  // Apply load factor to adjust arrival rate
  // Higher load means shorter intervals between connections
  intervalSeconds /= m_networkLoad;
  
  return Seconds (intervalSeconds);
}

} // namespace ns3

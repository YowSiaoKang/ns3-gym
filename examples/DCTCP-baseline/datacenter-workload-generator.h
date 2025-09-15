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

#ifndef DATACENTER_WORKLOAD_GENERATOR_H
#define DATACENTER_WORKLOAD_GENERATOR_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-container.h"
#include <vector>
#include <map>

namespace ns3 {

/**
 * \brief Enumeration for different data center workload types
 */
enum WorkloadType
{
  WEB_SEARCH,    // Web search workload with heavy-tailed flow sizes
  DATA_MINING    // Data mining workload with larger flows
};

/**
 * \brief Connection information structure (simplified)
 */
struct ConnectionInfo
{
  uint64_t targetBytes;       // Target bytes to send
  uint64_t bytesSent;         // Bytes sent so far  
  Ptr<Socket> socket;         // Socket for this connection
  uint32_t connectionId;      // Unique connection identifier
};

/**
 * \brief Data Center Workload Generator Application
 * 
 * This application generates realistic data center traffic patterns with:
 * - Heavy-tailed flow size distributions
 * - Configurable network load (60%, 70%, 80%, 90%)
 * - Support for Web Search and Data Mining workloads
 * - Random source-destination server selection
 */
class DataCenterWorkloadGenerator : public Application
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Constructor
   */
  DataCenterWorkloadGenerator ();

  /**
   * \brief Destructor
   */
  virtual ~DataCenterWorkloadGenerator ();

  /**
   * \brief Set the list of server nodes
   * \param servers The container of server nodes
   */
  void SetServerNodes (NodeContainer servers);

  /**
   * \brief Set the workload type
   * \param workloadType The type of workload (WEB_SEARCH or DATA_MINING)
   */
  void SetWorkloadType (WorkloadType workloadType);

  /**
   * \brief Set the target network load
   * \param load The target load as a percentage (0.6 for 60%, 0.7 for 70%, etc.)
   */
  void SetNetworkLoad (double load);

  /**
   * \brief Set the average flow arrival rate
   * \param rate The average number of flows per second
   */
  void SetFlowArrivalRate (double rate);

protected:
  virtual void DoDispose (void);

private:
  // Inherited from Application base class
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /**
   * \brief Schedule the next connection
   */
  void ScheduleNextConnection ();

  /**
   * \brief Generate and start a new connection
   */
  void GenerateConnection ();

  /**
   * \brief Generate transfer size based on workload type
   * \return Transfer size in bytes
   */
  uint64_t GenerateTransferSize ();

  /**
   * \brief Select random source and destination servers
   * \param sourceId Reference to store source node ID
   * \param destId Reference to store destination node ID
   */
  void SelectRandomEndpoints (uint32_t &sourceId, uint32_t &destId);

  /**
   * \brief Start a specific connection
   * \param connectionInfo The connection information structure
   * \param sourceNodeId Source node ID
   * \param destNodeId Destination node ID
   */
  void StartConnection (ConnectionInfo &connectionInfo, uint32_t sourceNodeId, uint32_t destNodeId);

  /**
   * \brief Handle successful socket connection
   * \param socket The connected socket
   */
  void ConnectionSucceeded (Ptr<Socket> socket);

  /**
   * \brief Handle failed socket connection
   * \param socket The socket that failed to connect
   */
  void ConnectionFailed (Ptr<Socket> socket);

  /**
   * \brief Send data for a specific flow
   * \param socket The socket to send data on
   * \param availableBufferSize Available buffer size
   */
  void SendData (Ptr<Socket> socket, uint32_t availableBufferSize);

  /**
   * \brief Handle connection completion
   * \param connectionId The ID of the completed connection
   */
  void ConnectionCompleted (uint32_t connectionId);

  /**
   * \brief Calculate connection inter-arrival time based on Poisson process
   * \return Time until next connection arrival
   */
  Time CalculateNextArrivalTime ();

  // Configuration parameters
  NodeContainer m_servers;                    //!< Server nodes container
  WorkloadType m_workloadType;                //!< Type of workload
  double m_networkLoad;                       //!< Target network load (0.0-1.0)
  double m_flowArrivalRate;                   //!< Average flows per second
  uint16_t m_basePort;                        //!< Base port for connections

  // Random variables for different distributions
  Ptr<ExponentialRandomVariable> m_connectionArrivalRv;  //!< Connection arrival times (Poisson process)
  Ptr<UniformRandomVariable> m_serverSelectRv;          //!< Server selection
  
  // Transfer size random variables (heavy-tailed distributions)
  Ptr<ParetoRandomVariable> m_webSearchSizeRv;     //!< Web search transfer sizes
  Ptr<ParetoRandomVariable> m_dataMiningSizeRv;          //!< Data mining transfer sizes
  Ptr<LogNormalRandomVariable> m_mixedSizeRv;      //!< Mixed workload transfer sizes

  // State tracking
  std::map<uint32_t, ConnectionInfo> m_activeConnections;      //!< Currently active connections
  std::map<Ptr<Socket>, uint32_t> m_socketToConnection;       //!< Socket to connection ID mapping
  uint32_t m_nextConnectionId;                                //!< Next connection ID to assign
  bool m_running;                                             //!< Whether generator is running

  // Events
  EventId m_nextConnectionEvent;                              //!< Next connection generation event
};

} // namespace ns3

#endif /* DATACENTER_WORKLOAD_GENERATOR_H */

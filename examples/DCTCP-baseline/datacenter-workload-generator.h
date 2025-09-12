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
 * \brief Flow information structure
 */
struct FlowInfo
{
  uint32_t sourceNodeId;      // Source server node ID
  uint32_t destNodeId;        // Destination server node ID
  uint64_t flowSize;          // Flow size in bytes
  Time startTime;             // Flow start time
  Time duration;              // Flow duration
  uint16_t port;              // Destination port
  bool isActive;              // Whether flow is currently active
  Ptr<Socket> socket;         // Socket for this flow
  uint64_t bytesSent;         // Bytes sent so far
  uint32_t flowId;            // Unique flow identifier
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

  /**
   * \brief Get the total number of active flows
   * \return Number of currently active flows
   */
  uint32_t GetActiveFlows () const;

  /**
   * \brief Get the total number of completed flows
   * \return Number of completed flows
   */
  uint32_t GetCompletedFlows () const;

  /**
   * \brief Get the total bytes sent
   * \return Total bytes sent across all flows
   */
  uint64_t GetTotalBytesSent () const;

protected:
  virtual void DoDispose (void);

private:
  // Inherited from Application base class
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  /**
   * \brief Schedule the next flow arrival
   */
  void ScheduleNextFlow ();

  /**
   * \brief Generate and start a new flow
   */
  void GenerateFlow ();

  /**
   * \brief Generate flow size based on workload type
   * \return Flow size in bytes
   */
  uint64_t GenerateFlowSize ();

  /**
   * \brief Select random source and destination servers
   * \param sourceId Reference to store source node ID
   * \param destId Reference to store destination node ID
   */
  void SelectRandomEndpoints (uint32_t &sourceId, uint32_t &destId);

  /**
   * \brief Start a specific flow
   * \param flowInfo The flow information structure
   */
  void StartFlow (FlowInfo &flowInfo);

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
   * \brief Handle flow completion
   * \param flowId The ID of the completed flow
   */
  void FlowCompleted (uint32_t flowId);

  /**
   * \brief Calculate flow inter-arrival time based on Poisson process
   * \return Time until next flow arrival
   */
  Time CalculateNextArrivalTime ();

  // Configuration parameters
  NodeContainer m_servers;                    //!< Server nodes container
  WorkloadType m_workloadType;                //!< Type of workload
  double m_networkLoad;                       //!< Target network load (0.0-1.0)
  double m_flowArrivalRate;                   //!< Average flows per second
  uint16_t m_basePort;                        //!< Base port for connections

  // Random variables for different distributions
  Ptr<ExponentialRandomVariable> m_flowArrivalRv;  //!< Flow arrival times (Poisson process)
  Ptr<UniformRandomVariable> m_serverSelectRv;     //!< Server selection
  Ptr<UniformRandomVariable> m_portSelectRv;       //!< Port selection
  
  // Flow size random variables (heavy-tailed distributions)
  Ptr<ParetoRandomVariable> m_webSearchFlowSizeRv;  //!< Web search flow sizes
  Ptr<ParetoRandomVariable> m_dataMiningFlowSizeRv; //!< Data mining flow sizes
  Ptr<LogNormalRandomVariable> m_mixedFlowSizeRv;   //!< Mixed workload flow sizes

  // State tracking
  std::map<uint32_t, FlowInfo> m_activeFlows;      //!< Currently active flows
  std::map<Ptr<Socket>, uint32_t> m_socketToFlow;  //!< Socket to flow ID mapping
  uint32_t m_nextFlowId;                           //!< Next flow ID to assign
  uint32_t m_completedFlows;                       //!< Number of completed flows
  uint64_t m_totalBytesSent;                       //!< Total bytes sent
  bool m_running;                                  //!< Whether generator is running

  // Events
  EventId m_nextFlowEvent;                         //!< Next flow generation event

  // Traced callbacks
  TracedCallback<uint32_t, uint64_t> m_flowStarted;    //!< Flow started trace
  TracedCallback<uint32_t, uint64_t> m_flowCompleted;  //!< Flow completed trace
  TracedCallback<uint64_t> m_bytesSent;                //!< Bytes sent trace
};

} // namespace ns3

#endif /* DATACENTER_WORKLOAD_GENERATOR_H */

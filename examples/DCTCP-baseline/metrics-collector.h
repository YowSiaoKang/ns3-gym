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
 */

#ifndef METRICS_COLLECTOR_H
#define METRICS_COLLECTOR_H

#include "ns3/object.h"
#include "ns3/simulator.h"
#include "ns3/queue.h"
#include "ns3/packet.h"
#include <fstream>
#include <map>
#include <vector>

namespace ns3 {

/**
 * \brief Metrics Collection Helper for Data Center Network Analysis
 * 
 * This class collects various network performance metrics including:
 * - Queue length and queuing delay statistics
 * - Flow completion time tracking
 * - Network-wide performance summaries
 */
class MetricsCollector : public Object
{
public:
  static TypeId GetTypeId (void);
  
  MetricsCollector ();
  virtual ~MetricsCollector ();
  
  /**
   * \brief Set base filename prefix for output files
   * \param prefix Filename prefix for all output files
   */
  void SetOutputPrefix (const std::string &prefix);
  
  /**
   * \brief Enable queue monitoring on a specific queue
   * \param queue Pointer to the queue to monitor
   * \param queueId Unique identifier for this queue
   */
  void MonitorQueue (Ptr<Queue<Packet>> queue, uint32_t queueId);
  
  /**
   * \brief Callback for packet enqueue events
   * \param queueId Queue identifier
   * \param packet The enqueued packet
   */
  void PacketEnqueue (uint32_t queueId, Ptr<const Packet> packet);
  
  /**
   * \brief Callback for packet dequeue events
   * \param queueId Queue identifier
   * \param packet The dequeued packet
   */
  void PacketDequeue (uint32_t queueId, Ptr<const Packet> packet);
  
  /**
   * \brief Callback for packet drop events
   * \param queueId Queue identifier
   * \param packet The dropped packet
   */
  void PacketDrop (uint32_t queueId, Ptr<const Packet> packet);
  
  /**
   * \brief Generate final metrics report
   */
  void GenerateReport ();
  
  /**
   * \brief Export queue statistics to file
   */
  void ExportQueueStats ();

private:
  struct QueueMetrics
  {
    uint32_t queueId;
    uint32_t enqueueCount;
    uint32_t dequeueCount;
    uint32_t dropCount;
    uint64_t totalBytes;
    double totalDelay;
    uint32_t maxQueueLength;
    std::vector<uint32_t> queueLengthSamples;
    std::vector<double> delaySamples;
  };
  
  struct PacketInQueue
  {
    Time arrivalTime;
    uint32_t packetSize;
  };
  
  std::map<uint32_t, QueueMetrics> m_queueStats;
  std::map<uint32_t, std::map<uint32_t, PacketInQueue>> m_packetsInQueue; // queueId -> packetId -> info
  
  std::string m_outputPrefix;
  std::ofstream m_queueFile;
  
  uint32_t m_nextPacketId;
  Time m_lastStatsTime;
  
  /**
   * \brief Get unique packet ID for tracking
   */
  uint32_t GetPacketId (Ptr<const Packet> packet);
  
  /**
   * \brief Update queue length statistics
   */
  void UpdateQueueLength (uint32_t queueId);
  
  /**
   * \brief Periodic statistics collection
   */
  void PeriodicStatsCollection ();
};

} // namespace ns3

#endif /* METRICS_COLLECTOR_H */
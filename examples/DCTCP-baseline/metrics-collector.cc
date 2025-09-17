/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 */

#include "metrics-collector.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("MetricsCollector");
NS_OBJECT_ENSURE_REGISTERED (MetricsCollector);

TypeId
MetricsCollector::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MetricsCollector")
    .SetParent<Object> ()
    .SetGroupName ("Applications")
    .AddConstructor<MetricsCollector> ()
  ;
  return tid;
}

MetricsCollector::MetricsCollector ()
  : m_outputPrefix ("metrics"),
    m_nextPacketId (1),
    m_lastStatsTime (Seconds (0))
{
  NS_LOG_FUNCTION (this);
  
  // Schedule periodic statistics collection every 100ms
  Simulator::Schedule (MilliSeconds (100), &MetricsCollector::PeriodicStatsCollection, this);
}

MetricsCollector::~MetricsCollector ()
{
  NS_LOG_FUNCTION (this);
  if (m_queueFile.is_open ())
    {
      m_queueFile.close ();
    }
}

void
MetricsCollector::SetOutputPrefix (const std::string &prefix)
{
  m_outputPrefix = prefix;
  
  // Open output files
  std::string queueFileName = m_outputPrefix + "_queues.txt";
  
  m_queueFile.open (queueFileName);
  if (m_queueFile.is_open ())
    {
      m_queueFile << "# Time(s), QueueID, Event, QueueLength, PacketSize, Delay(ms)\n";
    }
}

void
MetricsCollector::MonitorQueue (Ptr<Queue<Packet>> queue, uint32_t queueId)
{
  NS_LOG_FUNCTION (this << queueId);
  
  // Initialize queue metrics
  QueueMetrics metrics;
  metrics.queueId = queueId;
  metrics.enqueueCount = 0;
  metrics.dequeueCount = 0;
  metrics.dropCount = 0;
  metrics.totalBytes = 0;
  metrics.totalDelay = 0.0;
  metrics.maxQueueLength = 0;
  
  m_queueStats[queueId] = metrics;
  
  // Connect trace sources (note: these may need adjustment based on actual queue type)
  // For RED queues, we'll need to connect to the internal queue
  NS_LOG_INFO ("Queue monitoring enabled for queue " << queueId);
}

void
MetricsCollector::PacketEnqueue (uint32_t queueId, Ptr<const Packet> packet)
{
  NS_LOG_FUNCTION (this << queueId << packet->GetSize ());
  
  auto it = m_queueStats.find (queueId);
  if (it == m_queueStats.end ())
    {
      return;
    }
  
  QueueMetrics &metrics = it->second;
  metrics.enqueueCount++;
  metrics.totalBytes += packet->GetSize ();
  
  // Track packet for delay calculation
  uint32_t packetId = GetPacketId (packet);
  PacketInQueue packetInfo;
  packetInfo.arrivalTime = Simulator::Now ();
  packetInfo.packetSize = packet->GetSize ();
  
  m_packetsInQueue[queueId][packetId] = packetInfo;
  
  // Update queue length
  UpdateQueueLength (queueId);
  
  // Log to file
  if (m_queueFile.is_open ())
    {
      m_queueFile << Simulator::Now ().GetSeconds () << ", "
                  << queueId << ", ENQUEUE, "
                  << m_packetsInQueue[queueId].size () << ", "
                  << packet->GetSize () << ", 0\n";
    }
}

void
MetricsCollector::PacketDequeue (uint32_t queueId, Ptr<const Packet> packet)
{
  NS_LOG_FUNCTION (this << queueId << packet->GetSize ());
  
  auto it = m_queueStats.find (queueId);
  if (it == m_queueStats.end ())
    {
      return;
    }
  
  QueueMetrics &metrics = it->second;
  metrics.dequeueCount++;
  
  // Calculate queuing delay
  uint32_t packetId = GetPacketId (packet);
  auto queueIt = m_packetsInQueue.find (queueId);
  if (queueIt != m_packetsInQueue.end ())
    {
      auto packetIt = queueIt->second.find (packetId);
      if (packetIt != queueIt->second.end ())
        {
          double delay = (Simulator::Now () - packetIt->second.arrivalTime).GetSeconds ();
          metrics.totalDelay += delay;
          metrics.delaySamples.push_back (delay);
          
          // Log to file
          if (m_queueFile.is_open ())
            {
              m_queueFile << Simulator::Now ().GetSeconds () << ", "
                          << queueId << ", DEQUEUE, "
                          << (queueIt->second.size () - 1) << ", "
                          << packet->GetSize () << ", "
                          << (delay * 1000) << "\n";
            }
          
          // Remove packet from tracking
          queueIt->second.erase (packetIt);
        }
    }
  
  // Update queue length
  UpdateQueueLength (queueId);
}

void
MetricsCollector::PacketDrop (uint32_t queueId, Ptr<const Packet> packet)
{
  NS_LOG_FUNCTION (this << queueId << packet->GetSize ());
  
  auto it = m_queueStats.find (queueId);
  if (it == m_queueStats.end ())
    {
      return;
    }
  
  QueueMetrics &metrics = it->second;
  metrics.dropCount++;
  
  // Log to file
  if (m_queueFile.is_open ())
    {
      m_queueFile << Simulator::Now ().GetSeconds () << ", "
                  << queueId << ", DROP, "
                  << m_packetsInQueue[queueId].size () << ", "
                  << packet->GetSize () << ", 0\n";
    }
  
  // Remove packet from tracking if it was being tracked
  uint32_t packetId = GetPacketId (packet);
  auto queueIt = m_packetsInQueue.find (queueId);
  if (queueIt != m_packetsInQueue.end ())
    {
      queueIt->second.erase (packetId);
    }
}

void
MetricsCollector::GenerateReport ()
{
  NS_LOG_FUNCTION (this);
  
  std::cout << "\n=== METRICS COLLECTION REPORT ===\n";
  std::cout << std::fixed << std::setprecision (2);
  
  // Aggregate statistics across all queues
  uint64_t totalEnqueued = 0;
  uint64_t totalDequeued = 0;
  uint64_t totalDropped = 0;
  uint64_t totalBytes = 0;
  double totalDelaySum = 0.0;
  uint32_t totalDelaySamples = 0;
  uint32_t maxQueueLengthOverall = 0;
  uint32_t activeQueues = 0;
  std::vector<double> allDelaySamples;
  std::vector<double> allQueueLengthSamples;
  
  for (auto& queuePair : m_queueStats)
    {
      QueueMetrics &metrics = queuePair.second;
      if (metrics.enqueueCount > 0) // Only count active queues
        {
          activeQueues++;
          totalEnqueued += metrics.enqueueCount;
          totalDequeued += metrics.dequeueCount;
          totalDropped += metrics.dropCount;
          totalBytes += metrics.totalBytes;
          totalDelaySum += metrics.totalDelay;
          totalDelaySamples += metrics.delaySamples.size();
          
          if (metrics.maxQueueLength > maxQueueLengthOverall)
            {
              maxQueueLengthOverall = metrics.maxQueueLength;
            }
          
          // Collect all delay samples for percentile calculation
          allDelaySamples.insert(allDelaySamples.end(), 
                                metrics.delaySamples.begin(), 
                                metrics.delaySamples.end());
          
          // Collect all queue length samples
          allQueueLengthSamples.insert(allQueueLengthSamples.end(), 
                                      metrics.queueLengthSamples.begin(), 
                                      metrics.queueLengthSamples.end());
        }
    }
  
  // Calculate aggregate metrics
  std::cout << "\nAggregate Queue Statistics (across " << activeQueues << " active queues):\n";
  std::cout << "Total Queues Monitored: " << m_queueStats.size() << "\n";
  std::cout << "Active Queues (with traffic): " << activeQueues << "\n";
  std::cout << "Total Packets Enqueued: " << totalEnqueued << "\n";
  std::cout << "Total Packets Dequeued: " << totalDequeued << "\n";
  std::cout << "Total Packets Dropped: " << totalDropped << "\n";
  
  if (totalEnqueued > 0)
    {
      std::cout << "Overall Drop Rate: " << (100.0 * totalDropped / totalEnqueued) << "%\n";
    }
  
  std::cout << "Total Data Processed: " << (totalBytes / (1024.0 * 1024.0)) << " MB\n";
  
  if (!allDelaySamples.empty())
    {
      double avgDelay = totalDelaySum / totalDelaySamples;
      std::sort(allDelaySamples.begin(), allDelaySamples.end());
      double p50Delay = allDelaySamples[allDelaySamples.size() / 2];
      double p95Delay = allDelaySamples[static_cast<size_t>(allDelaySamples.size() * 0.95)];
      double p99Delay = allDelaySamples[static_cast<size_t>(allDelaySamples.size() * 0.99)];
      
      std::cout << "Average Queuing Delay: " << (avgDelay * 1000) << " ms\n";
      std::cout << "Median (50th percentile) Delay: " << (p50Delay * 1000) << " ms\n";
      std::cout << "95th percentile Delay: " << (p95Delay * 1000) << " ms\n";
      std::cout << "99th percentile Delay: " << (p99Delay * 1000) << " ms\n";
    }
  
  if (!allQueueLengthSamples.empty())
    {
      double avgQueueLength = std::accumulate(allQueueLengthSamples.begin(), 
                                             allQueueLengthSamples.end(), 0.0) / 
                             allQueueLengthSamples.size();
      std::cout << "Average Queue Length: " << avgQueueLength << " packets\n";
      std::cout << "Maximum Queue Length: " << maxQueueLengthOverall << " packets\n";
    }
  
  std::cout << "\n=== END METRICS REPORT ===\n\n";
}

void
MetricsCollector::ExportQueueStats ()
{
  // Export detailed queue statistics to a separate file
  std::string statsFile = m_outputPrefix + "_queue_summary.txt";
  std::ofstream outFile (statsFile);
  
  if (outFile.is_open ())
    {
      outFile << "# Queue Statistics Summary\n";
      outFile << "# QueueID, Enqueued, Dequeued, Dropped, DropRate%, TotalBytes, AvgDelay(ms), P95Delay(ms), AvgQueueLength, MaxQueueLength\n";
      
      for (auto& queuePair : m_queueStats)
        {
          QueueMetrics &metrics = queuePair.second;
          double dropRate = (metrics.enqueueCount > 0) ? (100.0 * metrics.dropCount / metrics.enqueueCount) : 0.0;
          double avgDelay = (!metrics.delaySamples.empty ()) ? (metrics.totalDelay / metrics.delaySamples.size () * 1000) : 0.0;
          
          double p95Delay = 0.0;
          if (!metrics.delaySamples.empty ())
            {
              std::vector<double> sorted = metrics.delaySamples;
              std::sort (sorted.begin (), sorted.end ());
              p95Delay = sorted[static_cast<size_t> (sorted.size () * 0.95)] * 1000;
            }
          
          double avgQueueLength = (!metrics.queueLengthSamples.empty ()) ? 
                                  (std::accumulate (metrics.queueLengthSamples.begin (), 
                                                    metrics.queueLengthSamples.end (), 0.0) / 
                                   metrics.queueLengthSamples.size ()) : 0.0;
          
          outFile << metrics.queueId << ", "
                  << metrics.enqueueCount << ", "
                  << metrics.dequeueCount << ", "
                  << metrics.dropCount << ", "
                  << dropRate << ", "
                  << metrics.totalBytes << ", "
                  << avgDelay << ", "
                  << p95Delay << ", "
                  << avgQueueLength << ", "
                  << metrics.maxQueueLength << "\n";
        }
      
      outFile.close ();
    }
}

uint32_t
MetricsCollector::GetPacketId (Ptr<const Packet> packet)
{
  // Simple packet ID based on packet pointer and size
  // In a real implementation, you might want to use packet tags
  return reinterpret_cast<uintptr_t> (PeekPointer (packet)) % 1000000 + packet->GetSize ();
}

void
MetricsCollector::UpdateQueueLength (uint32_t queueId)
{
  auto queueIt = m_packetsInQueue.find (queueId);
  if (queueIt != m_packetsInQueue.end ())
    {
      uint32_t currentLength = queueIt->second.size ();
      
      auto statsIt = m_queueStats.find (queueId);
      if (statsIt != m_queueStats.end ())
        {
          QueueMetrics &metrics = statsIt->second;
          metrics.queueLengthSamples.push_back (currentLength);
          if (currentLength > metrics.maxQueueLength)
            {
              metrics.maxQueueLength = currentLength;
            }
        }
    }
}

void
MetricsCollector::PeriodicStatsCollection ()
{
  // Collect periodic statistics (queue lengths, etc.)
  for (auto& queuePair : m_queueStats)
    {
      UpdateQueueLength (queuePair.first);
    }
  
  // Schedule next collection
  if (Simulator::Now () < Simulator::GetMaximumSimulationTime ())
    {
      Simulator::Schedule (MilliSeconds (100), &MetricsCollector::PeriodicStatsCollection, this);
    }
}

} // namespace ns3
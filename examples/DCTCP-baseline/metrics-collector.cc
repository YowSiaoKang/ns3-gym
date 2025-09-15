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
  if (m_flowFile.is_open ())
    {
      m_flowFile.close ();
    }
}

void
MetricsCollector::SetOutputPrefix (const std::string &prefix)
{
  m_outputPrefix = prefix;
  
  // Open output files
  std::string queueFileName = m_outputPrefix + "_queues.txt";
  std::string flowFileName = m_outputPrefix + "_flows.txt";
  
  m_queueFile.open (queueFileName);
  if (m_queueFile.is_open ())
    {
      m_queueFile << "# Time(s), QueueID, Event, QueueLength, PacketSize, Delay(ms)\n";
    }
  
  m_flowFile.open (flowFileName);
  if (m_flowFile.is_open ())
    {
      m_flowFile << "# FlowID, ExpectedSize, ActualSize, StartTime, CompletionTime, FCT(ms)\n";
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
MetricsCollector::FlowStart (uint32_t flowId, uint64_t flowSize)
{
  NS_LOG_FUNCTION (this << flowId << flowSize);
  
  FlowMetrics metrics;
  metrics.flowId = flowId;
  metrics.expectedSize = flowSize;
  metrics.actualSize = 0;
  metrics.startTime = Simulator::Now ();
  metrics.fct = 0.0;
  
  m_flowStats[flowId] = metrics;
}

void
MetricsCollector::FlowComplete (uint32_t flowId, uint64_t actualBytes)
{
  NS_LOG_FUNCTION (this << flowId << actualBytes);
  
  auto it = m_flowStats.find (flowId);
  if (it == m_flowStats.end ())
    {
      return;
    }
  
  FlowMetrics &metrics = it->second;
  metrics.actualSize = actualBytes;
  metrics.completionTime = Simulator::Now ();
  metrics.fct = (metrics.completionTime - metrics.startTime).GetSeconds ();
  
  // Log to file
  if (m_flowFile.is_open ())
    {
      m_flowFile << flowId << ", "
                 << metrics.expectedSize << ", "
                 << metrics.actualSize << ", "
                 << metrics.startTime.GetSeconds () << ", "
                 << metrics.completionTime.GetSeconds () << ", "
                 << (metrics.fct * 1000) << "\n";
    }
}

void
MetricsCollector::GenerateReport ()
{
  NS_LOG_FUNCTION (this);
  
  std::cout << "\n=== METRICS COLLECTION REPORT ===\n";
  std::cout << std::fixed << std::setprecision (2);
  
  // Queue statistics
  std::cout << "\nQueue Statistics:\n";
  for (auto& queuePair : m_queueStats)
    {
      QueueMetrics &metrics = queuePair.second;
      std::cout << "Queue " << metrics.queueId << ":\n";
      std::cout << "  Enqueued: " << metrics.enqueueCount << " packets\n";
      std::cout << "  Dequeued: " << metrics.dequeueCount << " packets\n";
      std::cout << "  Dropped: " << metrics.dropCount << " packets\n";
      std::cout << "  Drop Rate: " << (100.0 * metrics.dropCount / metrics.enqueueCount) << "%\n";
      std::cout << "  Total Bytes: " << (metrics.totalBytes / 1024.0) << " KB\n";
      
      if (!metrics.delaySamples.empty ())
        {
          double avgDelay = metrics.totalDelay / metrics.delaySamples.size ();
          std::sort (metrics.delaySamples.begin (), metrics.delaySamples.end ());
          double p95Delay = metrics.delaySamples[static_cast<size_t> (metrics.delaySamples.size () * 0.95)];
          
          std::cout << "  Avg Queuing Delay: " << (avgDelay * 1000) << " ms\n";
          std::cout << "  95th percentile Delay: " << (p95Delay * 1000) << " ms\n";
        }
      
      if (!metrics.queueLengthSamples.empty ())
        {
          double avgQueueLength = std::accumulate (metrics.queueLengthSamples.begin (), 
                                                   metrics.queueLengthSamples.end (), 0.0) / 
                                  metrics.queueLengthSamples.size ();
          std::cout << "  Avg Queue Length: " << avgQueueLength << " packets\n";
          std::cout << "  Max Queue Length: " << metrics.maxQueueLength << " packets\n";
        }
    }
  
  // Flow statistics
  std::cout << "\nFlow Completion Time Statistics:\n";
  if (!m_flowStats.empty ())
    {
      std::vector<double> fctSamples;
      uint32_t completedFlows = 0;
      
      for (auto& flowPair : m_flowStats)
        {
          FlowMetrics &metrics = flowPair.second;
          if (metrics.fct > 0)
            {
              fctSamples.push_back (metrics.fct);
              completedFlows++;
            }
        }
      
      if (!fctSamples.empty ())
        {
          std::sort (fctSamples.begin (), fctSamples.end ());
          double avgFct = std::accumulate (fctSamples.begin (), fctSamples.end (), 0.0) / fctSamples.size ();
          double medianFct = fctSamples[fctSamples.size () / 2];
          double p95Fct = fctSamples[static_cast<size_t> (fctSamples.size () * 0.95)];
          
          std::cout << "  Total Flows: " << m_flowStats.size () << "\n";
          std::cout << "  Completed Flows: " << completedFlows << "\n";
          std::cout << "  Avg FCT: " << (avgFct * 1000) << " ms\n";
          std::cout << "  Median FCT: " << (medianFct * 1000) << " ms\n";
          std::cout << "  95th percentile FCT: " << (p95Fct * 1000) << " ms\n";
        }
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
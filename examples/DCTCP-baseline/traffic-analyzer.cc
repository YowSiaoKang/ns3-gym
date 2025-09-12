/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025
 */

#include "traffic-analyzer.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TrafficAnalyzer");
NS_OBJECT_ENSURE_REGISTERED (TrafficAnalyzer);

TypeId
TrafficAnalyzer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TrafficAnalyzer")
    .SetParent<Object> ()
    .SetGroupName ("Applications")
    .AddConstructor<TrafficAnalyzer> ()
  ;
  return tid;
}

TrafficAnalyzer::TrafficAnalyzer ()
  : m_totalFlows (0),
    m_completedFlows (0),
    m_totalBytes (0),
    m_lastThroughputTime (Seconds (0)),
    m_bytesInInterval (0),
    m_outputFile ("traffic_analysis.txt")
{
  NS_LOG_FUNCTION (this);
}

TrafficAnalyzer::~TrafficAnalyzer ()
{
  NS_LOG_FUNCTION (this);
  if (m_outStream.is_open ())
    {
      m_outStream.close ();
    }
}

void
TrafficAnalyzer::SetOutputFile (const std::string &filename)
{
  m_outputFile = filename;
  m_outStream.open (filename);
  if (m_outStream.is_open ())
    {
      m_outStream << "# Traffic Analysis Report\n";
      m_outStream << "# Time, FlowID, Event, Size/Bytes, Duration\n";
    }
}

void
TrafficAnalyzer::FlowStarted (uint32_t flowId, uint64_t flowSize)
{
  NS_LOG_FUNCTION (this << flowId << flowSize);
  
  FlowStats stats;
  stats.flowSize = flowSize;
  stats.bytesSent = 0;
  stats.startTime = Simulator::Now ();
  stats.completed = false;
  
  m_flowStats[flowId] = stats;
  m_flowSizes.push_back (flowSize);
  m_totalFlows++;
  
  if (m_outStream.is_open ())
    {
      m_outStream << Simulator::Now ().GetSeconds () << ", "
                  << flowId << ", START, "
                  << flowSize << ", 0\n";
    }
}

void
TrafficAnalyzer::FlowCompleted (uint32_t flowId, uint64_t bytesSent)
{
  NS_LOG_FUNCTION (this << flowId << bytesSent);
  
  auto it = m_flowStats.find (flowId);
  if (it != m_flowStats.end ())
    {
      FlowStats &stats = it->second;
      stats.bytesSent = bytesSent;
      stats.completionTime = Simulator::Now ();
      stats.duration = (stats.completionTime - stats.startTime).GetSeconds ();
      stats.completed = true;
      
      m_flowDurations.push_back (stats.duration);
      m_completedFlows++;
      
      if (m_outStream.is_open ())
        {
          m_outStream << Simulator::Now ().GetSeconds () << ", "
                      << flowId << ", COMPLETE, "
                      << bytesSent << ", "
                      << stats.duration << "\n";
        }
    }
}

void
TrafficAnalyzer::BytesSent (uint64_t bytes)
{
  m_totalBytes += bytes;
  m_bytesInInterval += bytes;
  
  // Calculate throughput every second
  Time currentTime = Simulator::Now ();
  if (currentTime - m_lastThroughputTime >= Seconds (1.0))
    {
      double throughputMbps = (m_bytesInInterval * 8.0) / (1000000.0); // Mbps
      m_throughputSamples.push_back (throughputMbps);
      
      m_bytesInInterval = 0;
      m_lastThroughputTime = currentTime;
    }
}

void
TrafficAnalyzer::GenerateReport ()
{
  NS_LOG_FUNCTION (this);
  
  std::cout << "\n=== TRAFFIC ANALYSIS REPORT ===\n";
  std::cout << std::fixed << std::setprecision (2);
  
  // Basic statistics
  std::cout << "Total Flows Started: " << m_totalFlows << "\n";
  std::cout << "Total Flows Completed: " << m_completedFlows << "\n";
  std::cout << "Completion Rate: " << (100.0 * m_completedFlows / m_totalFlows) << "%\n";
  std::cout << "Total Bytes Sent: " << m_totalBytes << " (" << (m_totalBytes / 1024.0 / 1024.0) << " MB)\n";
  
  // Flow size analysis
  if (!m_flowSizes.empty ())
    {
      std::sort (m_flowSizes.begin (), m_flowSizes.end ());
      
      double avgFlowSize = std::accumulate (m_flowSizes.begin (), m_flowSizes.end (), 0.0) / m_flowSizes.size ();
      uint64_t medianFlowSize = m_flowSizes[m_flowSizes.size () / 2];
      uint64_t p95FlowSize = m_flowSizes[static_cast<size_t> (m_flowSizes.size () * 0.95)];
      
      std::cout << "\nFlow Size Statistics:\n";
      std::cout << "  Average: " << (avgFlowSize / 1024.0) << " KB\n";
      std::cout << "  Median: " << (medianFlowSize / 1024.0) << " KB\n";
      std::cout << "  95th percentile: " << (p95FlowSize / 1024.0) << " KB\n";
      std::cout << "  Min: " << (m_flowSizes.front () / 1024.0) << " KB\n";
      std::cout << "  Max: " << (m_flowSizes.back () / 1024.0) << " KB\n";
    }
  
  // Flow duration analysis
  if (!m_flowDurations.empty ())
    {
      std::sort (m_flowDurations.begin (), m_flowDurations.end ());
      
      double avgDuration = std::accumulate (m_flowDurations.begin (), m_flowDurations.end (), 0.0) / m_flowDurations.size ();
      double medianDuration = m_flowDurations[m_flowDurations.size () / 2];
      double p95Duration = m_flowDurations[static_cast<size_t> (m_flowDurations.size () * 0.95)];
      
      std::cout << "\nFlow Duration Statistics:\n";
      std::cout << "  Average: " << (avgDuration * 1000) << " ms\n";
      std::cout << "  Median: " << (medianDuration * 1000) << " ms\n";
      std::cout << "  95th percentile: " << (p95Duration * 1000) << " ms\n";
      std::cout << "  Min: " << (m_flowDurations.front () * 1000) << " ms\n";
      std::cout << "  Max: " << (m_flowDurations.back () * 1000) << " ms\n";
    }
  
  // Throughput analysis
  if (!m_throughputSamples.empty ())
    {
      double avgThroughput = std::accumulate (m_throughputSamples.begin (), m_throughputSamples.end (), 0.0) / m_throughputSamples.size ();
      std::sort (m_throughputSamples.begin (), m_throughputSamples.end ());
      double maxThroughput = m_throughputSamples.back ();
      
      std::cout << "\nThroughput Statistics:\n";
      std::cout << "  Average: " << avgThroughput << " Mbps\n";
      std::cout << "  Peak: " << maxThroughput << " Mbps\n";
    }
  
  // Heavy-tail analysis
  if (m_flowSizes.size () >= 10)
    {
      size_t numSmallFlows = 0;
      size_t numLargeFlows = 0;
      uint64_t smallFlowBytes = 0;
      uint64_t largeFlowBytes = 0;
      
      uint64_t threshold = 100 * 1024; // 100KB threshold
      
      for (uint64_t size : m_flowSizes)
        {
          if (size <= threshold)
            {
              numSmallFlows++;
              smallFlowBytes += size;
            }
          else
            {
              numLargeFlows++;
              largeFlowBytes += size;
            }
        }
      
      std::cout << "\nHeavy-Tail Analysis (100KB threshold):\n";
      std::cout << "  Small flows (<= 100KB): " << numSmallFlows << " (" << (100.0 * numSmallFlows / m_flowSizes.size ()) << "%)\n";
      std::cout << "  Large flows (> 100KB): " << numLargeFlows << " (" << (100.0 * numLargeFlows / m_flowSizes.size ()) << "%)\n";
      std::cout << "  Small flow bytes: " << (smallFlowBytes / 1024.0 / 1024.0) << " MB (" << (100.0 * smallFlowBytes / m_totalBytes) << "%)\n";
      std::cout << "  Large flow bytes: " << (largeFlowBytes / 1024.0 / 1024.0) << " MB (" << (100.0 * largeFlowBytes / m_totalBytes) << "%)\n";
    }
  
  std::cout << "\n=== END REPORT ===\n\n";
  
  // Write summary to file
  if (m_outStream.is_open ())
    {
      m_outStream << "\n# SUMMARY\n";
      m_outStream << "# Total Flows: " << m_totalFlows << "\n";
      m_outStream << "# Completed Flows: " << m_completedFlows << "\n";
      m_outStream << "# Total Bytes: " << m_totalBytes << "\n";
      m_outStream.close ();
    }
}

} // namespace ns3

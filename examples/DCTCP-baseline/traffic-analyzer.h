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

#ifndef TRAFFIC_ANALYZER_H
#define TRAFFIC_ANALYZER_H

#include "ns3/object.h"
#include "ns3/traced-callback.h"
#include "ns3/simulator.h"
#include <vector>
#include <map>
#include <fstream>

namespace ns3 {

/**
 * \brief Traffic Analysis Helper
 * 
 * This class helps analyze traffic patterns and performance metrics
 * for data center workloads.
 */
class TrafficAnalyzer : public Object
{
public:
  static TypeId GetTypeId (void);
  
  TrafficAnalyzer ();
  virtual ~TrafficAnalyzer ();
  
  /**
   * \brief Set output file for statistics
   * \param filename Output filename
   */
  void SetOutputFile (const std::string &filename);
  
  /**
   * \brief Callback for flow start events
   * \param flowId Flow ID
   * \param flowSize Flow size in bytes
   */
  void FlowStarted (uint32_t flowId, uint64_t flowSize);
  
  /**
   * \brief Callback for flow completion events
   * \param flowId Flow ID
   * \param bytesSent Bytes actually sent
   */
  void FlowCompleted (uint32_t flowId, uint64_t bytesSent);
  
  /**
   * \brief Callback for bytes sent events
   * \param bytes Number of bytes sent
   */
  void BytesSent (uint64_t bytes);
  
  /**
   * \brief Generate final statistics report
   */
  void GenerateReport ();
  
  /**
   * \brief Get total flows started
   */
  uint32_t GetTotalFlows () const { return m_totalFlows; }
  
  /**
   * \brief Get total flows completed
   */
  uint32_t GetCompletedFlows () const { return m_completedFlows; }
  
  /**
   * \brief Get total bytes sent
   */
  uint64_t GetTotalBytes () const { return m_totalBytes; }

private:
  struct FlowStats
  {
    uint64_t flowSize;
    uint64_t bytesSent;
    Time startTime;
    Time completionTime;
    double duration;
    bool completed;
  };
  
  std::map<uint32_t, FlowStats> m_flowStats;
  std::vector<uint64_t> m_flowSizes;
  std::vector<double> m_flowDurations;
  std::vector<double> m_throughputSamples;
  
  uint32_t m_totalFlows;
  uint32_t m_completedFlows;
  uint64_t m_totalBytes;
  
  Time m_lastThroughputTime;
  uint64_t m_bytesInInterval;
  
  std::string m_outputFile;
  std::ofstream m_outStream;
};

} // namespace ns3

#endif /* TRAFFIC_ANALYZER_H */

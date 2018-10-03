/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015 University of Campinas (Unicamp)
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
 *
 * Author: Luciano Chaves <luciano@lrc.ic.unicamp.br>
 */

#ifndef APP_STATS_CALCULATOR_H
#define APP_STATS_CALCULATOR_H

#include <ns3/core-module.h>
#include <ns3/network-module.h>

namespace ns3 {

/**
 * \ingroup svelteApps
 * This class monitors some basic QoS statistics in a network traffic flow. It
 * counts the number of transmitted/received bytes and packets, computes the
 * loss ratio, the average delay and the jitter. This class can be used to
 * monitor statistics at application and network level, but keep in mind that
 * it is not aware of duplicated or fragmented packets at lower levels.
 */
class AppStatsCalculator : public Object
{
public:
  AppStatsCalculator ();          //!< Default constructor.
  virtual ~AppStatsCalculator (); //!< Dummy destructor, see DoDispose.

  /**
   * Register this type.
   * \return The object TypeId.
   */
  static TypeId GetTypeId (void);

  /**
   * Reset all internal counters.
   */
  virtual void ResetCounters (void);

  /**
   * Update TX counters for a new transmitted packet.
   * \param txBytes The total number of bytes in this packet.
   * \return The next TX sequence number to use.
   */
  virtual uint32_t NotifyTx (uint32_t txBytes);

  /**
   * Update RX counters for a new received packet.
   * \param rxBytes The total number of bytes in this packet.
   * \param timestamp The timestamp when this packet was sent.
   */
  virtual void NotifyRx (uint32_t rxBytes, Time timestamp = Simulator::Now ());

  /**
   * Get QoS statistics.
   * \return The statistic value.
   */
  //\{
  Time      GetActiveTime   (void) const;
  uint32_t  GetLostPackets  (void) const;
  double    GetLossRatio    (void) const;
  uint32_t  GetTxPackets    (void) const;
  uint32_t  GetTxBytes      (void) const;
  uint32_t  GetRxPackets    (void) const;
  uint32_t  GetRxBytes      (void) const;
  Time      GetRxDelay      (void) const;
  Time      GetRxJitter     (void) const;
  DataRate  GetRxThroughput (void) const;
  //\}

  /**
   * Get the header for the print operator <<.
   * \param os The output stream.
   * \return The output stream.
   * \internal Keep this method consistent with the << operator below.
   */
  static std::ostream & PrintHeader (std::ostream &os);

  /**
   * TracedCallback signature for AppStatsCalculator.
   * \param stats The statistics.
   */
  typedef void (*AppStatsCallback)(Ptr<const AppStatsCalculator> stats);

protected:
  /** Destructor implementation. */
  virtual void DoDispose ();

private:
  uint32_t           m_txPackets;        //!< Number of TX packets.
  uint32_t           m_txBytes;          //!< Number of TX bytes.
  uint32_t           m_rxPackets;        //!< Number of RX packets.
  uint32_t           m_rxBytes;          //!< Number of RX bytes.
  Time               m_firstTxTime;      //!< First TX time.
  Time               m_firstRxTime;      //!< First RX time.
  Time               m_lastRxTime;       //!< Last RX time.
  Time               m_lastTimestamp;    //!< Last timestamp.
  int64_t            m_jitter;           //!< Jitter estimation.
  Time               m_delaySum;         //!< Sum of packet delays.
};

/**
 * Print the application QoS metadata on an output stream.
 * \param os The output stream.
 * \param stats The AppStatsCalculator object.
 * \returns The output stream.
 * \internal Keep this method consistent with the
 *           AppStatsCalculator::PrintHeader ().
 */
std::ostream & operator << (std::ostream &os, const AppStatsCalculator &stats);

} // namespace ns3
#endif /* APP_STATS_CALCULATOR_H */

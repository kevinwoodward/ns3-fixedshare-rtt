/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Copyright (c) 2006 Georgia Tech Research Corporation
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Author: Rajib Bhattacharjea<raj.b@gatech.edu>
//

// Ported from:
// Georgia Tech Network Simulator - Round Trip Time Estimator Class
// George F. Riley.  Georgia Tech, Spring 2002

// Base class allows variations of round trip time estimators to be
// implemented

#include <iostream>
#include <cmath>

#include "rtt-estimator.h"
#include "ns3/double.h"
#include "ns3/integer.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RttEstimator");

NS_OBJECT_ENSURE_REGISTERED (RttEstimator);

/// Tolerance used to check reciprocal of two numbers.
static const double TOLERANCE = 1e-6;

TypeId 
RttEstimator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RttEstimator")
    .SetParent<Object> ()
    .SetGroupName ("Internet")
    .AddAttribute ("InitialEstimation", 
                   "Initial RTT estimate",
                   TimeValue (Seconds (1.0)),
                   MakeTimeAccessor (&RttEstimator::m_initialEstimatedRtt),
                   MakeTimeChecker ())
  ;
  return tid;
}

Time
RttEstimator::GetEstimate (void) const
{
  return m_estimatedRtt;
}

Time 
RttEstimator::GetVariation (void) const
{
  return m_estimatedVariation;
}


// Base class methods

RttEstimator::RttEstimator ()
  : m_nSamples (0)
{ 
  NS_LOG_FUNCTION (this);
  
  // We need attributes initialized here, not later, so use the 
  // ConstructSelf() technique documented in the manual
  ObjectBase::ConstructSelf (AttributeConstructionList ());
  m_estimatedRtt = m_initialEstimatedRtt;
  m_estimatedVariation = Time (0);
  NS_LOG_DEBUG ("Initialize m_estimatedRtt to " << m_estimatedRtt.GetSeconds () << " sec.");
  NS_LOG_DEBUG ("Initialize m_estimatedVariation to " << m_estimatedVariation.GetSeconds () << " sec.");
}

RttEstimator::RttEstimator (const RttEstimator& c)
  : Object (c),
    m_initialEstimatedRtt (c.m_initialEstimatedRtt),
    m_estimatedRtt (c.m_estimatedRtt),
    m_estimatedVariation (c.m_estimatedVariation),
    m_nSamples (c.m_nSamples)
{
  NS_LOG_FUNCTION (this);
}

RttEstimator::~RttEstimator ()
{
  NS_LOG_FUNCTION (this);
}

TypeId
RttEstimator::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void RttEstimator::Reset ()
{ 
  NS_LOG_FUNCTION (this);
  // Reset to initial state
  m_estimatedRtt = m_initialEstimatedRtt;
  m_estimatedVariation = Time (0);
  m_nSamples = 0;
}

uint32_t 
RttEstimator::GetNSamples (void) const
{
  return m_nSamples;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Mean-Deviation Estimator

NS_OBJECT_ENSURE_REGISTERED (RttMeanDeviation);

TypeId 
RttMeanDeviation::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RttMeanDeviation")
    .SetParent<RttEstimator> ()
    .SetGroupName ("Internet")
    .AddConstructor<RttMeanDeviation> ()
    .AddAttribute ("Alpha",
                   "Gain used in estimating the RTT, must be 0 <= alpha <= 1",
                   DoubleValue (0.125),
                   MakeDoubleAccessor (&RttMeanDeviation::m_alpha),
                   MakeDoubleChecker<double> (0, 1))
    .AddAttribute ("Beta",
                   "Gain used in estimating the RTT variation, must be 0 <= beta <= 1",
                   DoubleValue (0.25),
                   MakeDoubleAccessor (&RttMeanDeviation::m_beta),
                   MakeDoubleChecker<double> (0, 1))
  ;
  return tid;
}

RttMeanDeviation::RttMeanDeviation()
{
  NS_LOG_FUNCTION (this);
}

RttMeanDeviation::RttMeanDeviation (const RttMeanDeviation& c)
  : RttEstimator (c), m_alpha (c.m_alpha), m_beta (c.m_beta)
{
  NS_LOG_FUNCTION (this);
}

TypeId
RttMeanDeviation::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
RttMeanDeviation::CheckForReciprocalPowerOfTwo (double val) const
{
  // NS_LOG_FUNCTION (this << val);
  if (val < TOLERANCE)
    {
      return 0;
    }
  // supports 1/32, 1/16, 1/8, 1/4, 1/2
  if (std::abs (1/val - 8) < TOLERANCE)
    {
      return 3;
    }
  if (std::abs (1/val - 4) < TOLERANCE)
    {
      return 2;
    }
  if (std::abs (1/val - 32) < TOLERANCE)
    {
      return 5;
    }
  if (std::abs (1/val - 16) < TOLERANCE)
    {
      return 4;
    }
  if (std::abs (1/val - 2) < TOLERANCE)
    {
      return 1;
    }
  return 0;
}

void
RttMeanDeviation::FloatingPointUpdate (Time m)
{
  // NS_LOG_FUNCTION (this << m);

  // EWMA formulas are implemented as suggested in
  // Jacobson/Karels paper appendix A.2

  // SRTT <- (1 - alpha) * SRTT + alpha *  R'
  Time err (m - m_estimatedRtt);
  double gErr = err.ToDouble (Time::S) * m_alpha;
  m_estimatedRtt += Time::FromDouble (gErr, Time::S);

  // RTTVAR <- (1 - beta) * RTTVAR + beta * |SRTT - R'|
  Time difference = Abs (err) - m_estimatedVariation;
  m_estimatedVariation += Time::FromDouble (difference.ToDouble (Time::S) * m_beta, Time::S);

  return;
}

void
RttMeanDeviation::IntegerUpdate (Time m, uint32_t rttShift, uint32_t variationShift)
{
  // NS_LOG_FUNCTION (this << m << rttShift << variationShift);
  // Jacobson/Karels paper appendix A.2
  int64_t meas = m.GetInteger ();
  int64_t delta = meas - m_estimatedRtt.GetInteger ();
  int64_t srtt = (m_estimatedRtt.GetInteger () << rttShift) + delta;
  m_estimatedRtt = Time::From (srtt >> rttShift);
  if (delta < 0)
    {
      delta = -delta;
    }
  delta -= m_estimatedVariation.GetInteger ();
  int64_t rttvar = m_estimatedVariation.GetInteger () << variationShift;
  rttvar += delta;
  m_estimatedVariation = Time::From (rttvar >> variationShift);
  return;
}

void 
RttMeanDeviation::Measurement (Time m)
{
  m_estimates.push_back(m_estimatedRtt.GetMilliSeconds());
  m_actuals.push_back(m.GetMilliSeconds()); 

  if (m_nSamples)
    { 
      // If both alpha and beta are reciprocal powers of two, updating can
      // be done with integer arithmetic according to Jacobson/Karels paper.
      // If not, since class Time only supports integer multiplication,
      // must convert Time to floating point and back again
      uint32_t rttShift = CheckForReciprocalPowerOfTwo (m_alpha);
      uint32_t variationShift = CheckForReciprocalPowerOfTwo (m_beta);
      if (rttShift && variationShift)
        {
          IntegerUpdate (m, rttShift, variationShift);
        }
      else
        {
          FloatingPointUpdate (m);
        }
    }
  else
    { // First sample
      m_estimatedRtt = m;               // Set estimate to current
      m_estimatedVariation = m / 2;  // And variation to current / 2
      NS_LOG_DEBUG ("(first sample) m_estimatedVariation += " << m);
    }
  m_nSamples++;
}

Ptr<RttEstimator> 
RttMeanDeviation::Copy () const
{
  NS_LOG_FUNCTION (this);
  return CopyObject<RttMeanDeviation> (this);
}

void 
RttMeanDeviation::Reset ()
{ 
  NS_LOG_FUNCTION (this);
  RttEstimator::Reset ();
}

RttMeanDeviation::~RttMeanDeviation ()
{
  // NS_LOG_DEBUG("In destructor");
  if (m_actuals.size() > 0)
  {
    PrintDiagnostics();
  }
}

void RttMeanDeviation::PrintDiagnostics()
{
  double differenceSum = 0;

  for (int i = 0; i < m_actuals.size(); i++)
  {
    differenceSum += abs((double)m_estimates[i] - (double)m_actuals[i]);
  }

  differenceSum /= (double)m_actuals.size();

  NS_LOG_DEBUG("Mean error of " << differenceSum << " with a weight of " << m_actuals.size());
  
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Fixed-Share Estimator

NS_OBJECT_ENSURE_REGISTERED(RttFixedShare);

// Public

TypeId 
RttFixedShare::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RttFixedShare")
    .SetParent<RttEstimator> ()
    .SetGroupName ("Internet")
    .AddConstructor<RttFixedShare> ()
    .AddAttribute ("NumExperts",
                   "Number of experts, must be 0 < numExperts",
                   IntegerValue (100),
                   MakeIntegerAccessor (&RttFixedShare::m_numExperts),
                   MakeIntegerChecker<int> (0))
    .AddAttribute ("Alpha",
                   "Weight sharing parameter, must be 0 <= alpha <= 1",
                   DoubleValue (0.08),
                   MakeDoubleAccessor (&RttFixedShare::m_alpha),
                   MakeDoubleChecker<double> (0, 1))
    .AddAttribute ("Beta",
                   "Gain used in estimating the RTT variation, must be 0 <= beta <= 1",
                   DoubleValue (0.25),
                   MakeDoubleAccessor (&RttFixedShare::m_beta),
                   MakeDoubleChecker<double> (0, 1))
    .AddAttribute ("LR",
                   "Learning rate, must be 0 < LR",
                   DoubleValue (2.0),
                   MakeDoubleAccessor (&RttFixedShare::m_lr),
                   MakeDoubleChecker<double> (0))
  ;
  return tid;
}

RttFixedShare::RttFixedShare()
{      
  m_numExperts = 100;
  m_alpha = 0.08;
  m_beta = 0.25;
  m_lr = 2.0;
  InitializeVectors();
}

RttFixedShare::RttFixedShare (const RttFixedShare& c)
  : RttEstimator (c), m_numExperts (c.m_numExperts), m_alpha (c.m_alpha), m_beta (c.m_beta), m_lr (c.m_lr)
{
  // Initialize member variables
  
  InitializeVectors();
}

TypeId
RttFixedShare::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void RttFixedShare::Measurement(Time measure)
{ 
  if (m_nSamples > 0)
  {
    // This is the first measurement. Set est RTT to 0 and variance to RTT/2
    m_estimatedRtt = measure;
    m_estimatedVariation = measure / 2;
    return;
  }

  // Push values for logging
  m_estimates.push_back(m_estimatedRtt.GetMilliSeconds());
  m_actuals.push_back(measure.GetMilliSeconds()); 
  // NS_LOG_DEBUG("In measurement");

  // 0) Cast time to double, units in milliseconds
  
  double actualRtt = measure.GetSeconds();
  // NS_LOG_DEBUG("Actual rtt:" << measure.GetMilliSeconds());

  // 1) Get the predicted RTT from previous losses and assign to private variable

  double numeratorSum = 0;
  double denominatorSum = 0;

  for (int i = 0; i < m_numExperts; i++)
  {
    numeratorSum += m_weights[i] * m_experts[i];
    denominatorSum += m_weights[i];
  }

  double yPredicted = numeratorSum / denominatorSum;

  // Save old rtt for computing variation
  double oldEstimatedRtt = m_estimatedRtt.ToDouble(Time::S);
  m_estimatedRtt = Time::FromDouble (yPredicted, Time::S);

  // NS_LOG_DEBUG("Estimated rtt:" << m_estimatedRtt.GetMilliSeconds());

  // 2) Compute the new losses

  for (int i = 0; i < m_numExperts; i++)
  {    
    if (m_experts[i] >= actualRtt)
    {      
      m_losses[i] = pow(m_experts[i] - actualRtt, 2);
      // m_losses.push_back(pow(m_experts[i] - actualRtt, 2));
    }
    else
    {
      m_losses[i] = 2.0 * actualRtt;
      // m_losses.push_back(2.0 * actualRtt);
    }
  }

  // 3) Apply exponential updates to weights

  for (int i = 0; i < m_numExperts; i++)
  {
    m_weights[i] = m_weights[i] * exp(-m_lr * m_losses[i]);
  }

  // 4) Share weights

  double pool = 0;
  for (int i = 0; i < m_numExperts; i++)
  {
    pool += m_alpha * m_weights[i];
  }

  pool /= m_numExperts;

  for (int i = 0; i < m_numExperts; i++)
  {
    m_weights[i] = ((1 - m_alpha) * m_weights[i]) + pool;
  }

  // Update variation

  double oldRttVar = m_estimatedVariation.ToDouble(Time::S);
  double newRttVar = (1 - m_beta) * oldRttVar + m_beta * (abs(measure.ToDouble(Time::S) - oldEstimatedRtt));
  m_estimatedVariation = Time::FromDouble (newRttVar, Time::S);
}

Ptr<RttEstimator> 
RttFixedShare::Copy () const
{
  return CopyObject<RttFixedShare> (this);
}

void 
RttFixedShare::Reset ()
{ 
  // RttEstimator::Reset ();
}

RttFixedShare::~RttFixedShare ()
{
  if (m_actuals.size() > 0)
  {
    PrintDiagnostics();
  }
}

// Private

void RttFixedShare::InitializeVectors()
{ 
  double initialWeight = 1.0 / m_numExperts;
  double rttMin = 0.0;
  double rttMax = 0.4;  
  // Initialize all weights uniform to 1/N  
  for (int i = 1; i <= m_numExperts; i++)
  {    
    m_experts.push_back(rttMin + rttMax * pow(2, ((i - m_numExperts) / 4.0)));
    m_weights.push_back(initialWeight);
    m_losses.push_back(0.0);
  }
}

void RttFixedShare::PrintDiagnostics()
{
  double differenceSum = 0;
  double biggestActual = 0;
  double biggestIndex = 0;

  for (int i = 0; i < m_actuals.size(); i++)
  {
    if (m_actuals[i] > biggestActual)
    {
      biggestActual = m_actuals[i];
      biggestIndex = i;
    }
    biggestActual = std::max(biggestActual, m_actuals[i]);
    differenceSum += abs((double)m_estimates[i] - (double)m_actuals[i]);
  }

  differenceSum /= (double)m_actuals.size();

  NS_LOG_DEBUG("Mean error of " << differenceSum << " with a weight of " << m_actuals.size());
  NS_LOG_DEBUG("Max actual RTT: " << biggestActual << " at index " << biggestIndex << " out of " << m_actuals.size());
  
}

} //namespace ns3

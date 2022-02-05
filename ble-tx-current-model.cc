/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Universita' degli Studi di Napoli "Federico II"
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
 * Author: Stefano Avallone <stefano.avallone@unina.it>
 */

#include "ns3/log.h"
#include <math.h>
#include "ns3/double.h"
#include "ble-tx-current-model.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleTxCurrentModel");

NS_OBJECT_ENSURE_REGISTERED (BleTxCurrentModel);

TypeId 
BleTxCurrentModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleTxCurrentModel")
    .SetParent<Object> ()
    .SetGroupName ("Ble")
  ;
  return tid;
}

BleTxCurrentModel::BleTxCurrentModel()
{
}

BleTxCurrentModel::~BleTxCurrentModel()
{
}

NS_OBJECT_ENSURE_REGISTERED (LinearBleTxCurrentModel);

TypeId 
LinearBleTxCurrentModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LinearBleTxCurrentModel")
    .SetParent<BleTxCurrentModel> ()
    .SetGroupName ("Ble")
    .AddConstructor<LinearBleTxCurrentModel> ()
    .AddAttribute ("Eta", "The efficiency of the power amplifier.",
                   DoubleValue (0.10),
                   MakeDoubleAccessor (&LinearBleTxCurrentModel::m_eta),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Voltage", "The supply voltage (in Volts).",
                   DoubleValue (3.0),
                   MakeDoubleAccessor (&LinearBleTxCurrentModel::m_voltage),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("IdleCurrent", "The current in the IDLE state (in Ampere).",
                   DoubleValue (1e-6),
                   MakeDoubleAccessor (&LinearBleTxCurrentModel::m_idleCurrent),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}

LinearBleTxCurrentModel::LinearBleTxCurrentModel ()
{
  NS_LOG_FUNCTION (this);
}

LinearBleTxCurrentModel::~LinearBleTxCurrentModel()
{
  NS_LOG_FUNCTION (this);
}

double
DbmToW (double dBm)
{
  return std::pow (10.0, 0.1 * (dBm - 30.0));
}

double
LinearBleTxCurrentModel::CalcTxCurrent (double txPowerDbm) const
{
  NS_LOG_FUNCTION (this << txPowerDbm);
  return DbmToW (txPowerDbm) / (m_voltage * m_eta) + m_idleCurrent;
}

} // namespace ns3

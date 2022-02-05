/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Network Security Lab, University of Washington, Seattle.
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
 * Author: Sidharth Nabar <snabar@uw.edu>, He Wu <mdzz@u.washington.edu>
 */

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/pointer.h"
#include "ns3/energy-source.h"
#include "ble-radio-energy-model.h"
#include "ble-tx-current-model.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BleRadioEnergyModel");

NS_OBJECT_ENSURE_REGISTERED (BleRadioEnergyModel);

TypeId
BleRadioEnergyModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::BleRadioEnergyModel")
    .SetParent<DeviceEnergyModel> ()
    .SetGroupName ("Energy")
    .AddConstructor<BleRadioEnergyModel> ()
    .AddAttribute ("IdleCurrentA",
                   "The default radio Idle current in Ampere.",
                   DoubleValue (1e-6),  // idle mode = 273mA
                   MakeDoubleAccessor (&BleRadioEnergyModel::SetIdleCurrentA,
                                       &BleRadioEnergyModel::GetIdleCurrentA),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("CcaBusyCurrentA",
                   "The default radio CCA Busy State current in Ampere.",
                   DoubleValue (1e-6),  // default to be the same as idle mode
                   MakeDoubleAccessor (&BleRadioEnergyModel::SetCcaBusyCurrentA,
                                       &BleRadioEnergyModel::GetCcaBusyCurrentA),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("TxCurrentA",
                   "The radio TX current in Ampere.",
                   DoubleValue (0.015),    // transmit at 0dBm = 380mA
                   MakeDoubleAccessor (&BleRadioEnergyModel::SetTxCurrentA,
                                       &BleRadioEnergyModel::GetTxCurrentA),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("RxCurrentA",
                   "The radio RX current in Ampere.",
                   DoubleValue (0.015),    // receive mode = 313mA
                   MakeDoubleAccessor (&BleRadioEnergyModel::SetRxCurrentA,
                                       &BleRadioEnergyModel::GetRxCurrentA),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SwitchingCurrentA",
                   "The default radio Channel Switch current in Ampere.",
                   DoubleValue (1e-6),  // default to be the same as idle mode
                   MakeDoubleAccessor (&BleRadioEnergyModel::SetSwitchingCurrentA,
                                       &BleRadioEnergyModel::GetSwitchingCurrentA),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SleepCurrentA",
                   "The radio Sleep current in Ampere.",
                   DoubleValue (1e-6),  // sleep mode = 33mA
                   MakeDoubleAccessor (&BleRadioEnergyModel::SetSleepCurrentA,
                                       &BleRadioEnergyModel::GetSleepCurrentA),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("TxCurrentModel", "A pointer to the attached TX current model.",
                   PointerValue (),
                   MakePointerAccessor (&BleRadioEnergyModel::m_txCurrentModel),
                   MakePointerChecker<BleTxCurrentModel> ())
    .AddTraceSource ("TotalEnergyConsumption",
                     "Total energy consumption of the radio device.",
                     MakeTraceSourceAccessor (&BleRadioEnergyModel::m_totalEnergyConsumption),
                     "ns3::TracedValueCallback::Double")
  ;
  return tid;
}

BleRadioEnergyModel::BleRadioEnergyModel ()
  : m_source (0),
    m_currentState (BlePhy::State::IDLE),
    m_lastUpdateTime (Seconds (0.0)),
    m_nPendingChangeState (0)
{
  NS_LOG_FUNCTION (this);
  m_energyDepletionCallback.Nullify ();
  // set callback for BlePhy listener
  m_listener = new BleRadioEnergyModelPhyListener;
  m_listener->SetChangeStateCallback (MakeCallback (&DeviceEnergyModel::ChangeState, this));
  // set callback for updating the TX current
  m_listener->SetUpdateTxCurrentCallback (MakeCallback (&BleRadioEnergyModel::SetTxCurrentFromModel, this));
}

BleRadioEnergyModel::~BleRadioEnergyModel ()
{
  NS_LOG_FUNCTION (this);
  m_txCurrentModel = 0;
  delete m_listener;
}

void
BleRadioEnergyModel::SetEnergySource (const Ptr<EnergySource> source)
{
  NS_LOG_FUNCTION (this << source);
  NS_ASSERT (source != NULL);
  m_source = source;
  m_switchToOffEvent.Cancel ();
  Time durationToOff = GetMaximumTimeInState (m_currentState);
  m_switchToOffEvent = Simulator::Schedule (durationToOff, &BleRadioEnergyModel::ChangeState, this, BlePhy::State::OFF);
}

double
BleRadioEnergyModel::GetTotalEnergyConsumption (void) const
{
  NS_LOG_FUNCTION (this);

  Time duration = Simulator::Now () - m_lastUpdateTime;
  NS_ASSERT (duration.IsPositive ()); // check if duration is valid

  // energy to decrease = current * voltage * time
  double supplyVoltage = m_source->GetSupplyVoltage ();
  double energyToDecrease = duration.GetSeconds () * GetStateA (m_currentState) * supplyVoltage;
  
  // notify energy source
  m_source->UpdateEnergySource ();

  return m_totalEnergyConsumption + energyToDecrease;
}

double
BleRadioEnergyModel::GetIdleCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_idleCurrentA;
}

void
BleRadioEnergyModel::SetIdleCurrentA (double idleCurrentA)
{
  NS_LOG_FUNCTION (this << idleCurrentA);
  m_idleCurrentA = idleCurrentA;
}

double
BleRadioEnergyModel::GetCcaBusyCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_ccaBusyCurrentA;
}

void
BleRadioEnergyModel::SetCcaBusyCurrentA (double CcaBusyCurrentA)
{
  NS_LOG_FUNCTION (this << CcaBusyCurrentA);
  m_ccaBusyCurrentA = CcaBusyCurrentA;
}

double
BleRadioEnergyModel::GetTxCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_txCurrentA;
}

void
BleRadioEnergyModel::SetTxCurrentA (double txCurrentA)
{
  NS_LOG_FUNCTION (this << txCurrentA);
  m_txCurrentA = txCurrentA;
}

double
BleRadioEnergyModel::GetRxCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_rxCurrentA;
}

void
BleRadioEnergyModel::SetRxCurrentA (double rxCurrentA)
{
  NS_LOG_FUNCTION (this << rxCurrentA);
  m_rxCurrentA = rxCurrentA;
}

double
BleRadioEnergyModel::GetSwitchingCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_switchingCurrentA;
}

void
BleRadioEnergyModel::SetSwitchingCurrentA (double switchingCurrentA)
{
  NS_LOG_FUNCTION (this << switchingCurrentA);
  m_switchingCurrentA = switchingCurrentA;
}

double
BleRadioEnergyModel::GetSleepCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_sleepCurrentA;
}

void
BleRadioEnergyModel::SetSleepCurrentA (double sleepCurrentA)
{
  NS_LOG_FUNCTION (this << sleepCurrentA);
  m_sleepCurrentA = sleepCurrentA;
}

BlePhy::State
BleRadioEnergyModel::GetCurrentState (void) const
{
  NS_LOG_FUNCTION (this);
  return m_currentState;
}

void
BleRadioEnergyModel::SetEnergyDepletionCallback (
  BleRadioEnergyDepletionCallback callback)
{
  NS_LOG_FUNCTION (this);
  if (callback.IsNull ())
    {
      NS_LOG_DEBUG ("BleRadioEnergyModel:Setting NULL energy depletion callback!");
    }
  m_energyDepletionCallback = callback;
}

void
BleRadioEnergyModel::SetEnergyRechargedCallback (
  BleRadioEnergyRechargedCallback callback)
{
  NS_LOG_FUNCTION (this);
  if (callback.IsNull ())
    {
      NS_LOG_DEBUG ("BleRadioEnergyModel:Setting NULL energy recharged callback!");
    }
  m_energyRechargedCallback = callback;
}

void
BleRadioEnergyModel::SetTxCurrentModel (const Ptr<BleTxCurrentModel> model)
{
  m_txCurrentModel = model;
}

void
BleRadioEnergyModel::SetTxCurrentFromModel (double txPowerDbm)
{
  if (m_txCurrentModel)
    {
      m_txCurrentA = m_txCurrentModel->CalcTxCurrent (txPowerDbm);
    }
}

Time
BleRadioEnergyModel::GetMaximumTimeInState (int state) const
{
  if (state == BlePhy::State::OFF)
    {
      NS_FATAL_ERROR ("Requested maximum remaining time for OFF state");
    }
  double remainingEnergy = m_source->GetRemainingEnergy ();
  double supplyVoltage = m_source->GetSupplyVoltage ();
  double current = GetStateA (state);
  return Seconds (remainingEnergy / (current * supplyVoltage));
}

void
BleRadioEnergyModel::ChangeState (int newState)
{
  NS_LOG_FUNCTION (this << newState);

  m_nPendingChangeState++;

  if (m_nPendingChangeState > 1 && newState == BlePhy::State::OFF)
    {
      SetBleRadioState ((BlePhy::State) newState);
      m_nPendingChangeState--;
      return;
    }

  if (newState != BlePhy::State::OFF)
    {
      m_switchToOffEvent.Cancel ();
      Time durationToOff = GetMaximumTimeInState (newState);
      m_switchToOffEvent = Simulator::Schedule (durationToOff, &BleRadioEnergyModel::ChangeState, this, BlePhy::State::OFF);
    }

  Time duration = Simulator::Now () - m_lastUpdateTime;
  NS_ASSERT (duration.IsPositive ()); // check if duration is valid

  // energy to decrease = current * voltage * time
  double supplyVoltage = m_source->GetSupplyVoltage ();
  double energyToDecrease = duration.GetSeconds () * GetStateA (m_currentState) * supplyVoltage;
  // update total energy consumption
  m_totalEnergyConsumption += energyToDecrease;
  NS_ASSERT (m_totalEnergyConsumption <= m_source->GetInitialEnergy ());

  // update last update time stamp
  m_lastUpdateTime = Simulator::Now ();

  // notify energy source
  m_source->UpdateEnergySource ();

  // in case the energy source is found to be depleted during the last update, a callback might be
  // invoked that might cause a change in the Ble PHY state (e.g., the PHY is put into SLEEP mode).
  // This in turn causes a new call to this member function, with the consequence that the previous
  // instance is resumed after the termination of the new instance. In particular, the state set
  // by the previous instance is erroneously the final state stored in m_currentState. The check below
  // ensures that previous instances do not change m_currentState.

  if (m_nPendingChangeState <= 1 && m_currentState != BlePhy::State::OFF)
    {
      // update current state & last update time stamp
      SetBleRadioState ((BlePhy::State) newState);

      // some debug message
      NS_LOG_DEBUG ("BleRadioEnergyModel:Total energy consumption is " <<
                    m_totalEnergyConsumption << "J");
    }

  m_nPendingChangeState--;
}

void
BleRadioEnergyModel::HandleEnergyDepletion (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("BleRadioEnergyModel:Energy is depleted!");
  // invoke energy depletion callback, if set.
  if (!m_energyDepletionCallback.IsNull ())
    {
      m_energyDepletionCallback ();
    }
}

void
BleRadioEnergyModel::HandleEnergyRecharged (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("BleRadioEnergyModel:Energy is recharged!");
  // invoke energy recharged callback, if set.
  if (!m_energyRechargedCallback.IsNull ())
    {
      m_energyRechargedCallback ();
    }
}

void
BleRadioEnergyModel::HandleEnergyChanged (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("BleRadioEnergyModel:Energy is changed!");
  if (m_currentState != BlePhy::State::OFF)
    {
      m_switchToOffEvent.Cancel ();
      Time durationToOff = GetMaximumTimeInState (m_currentState);
      m_switchToOffEvent = Simulator::Schedule (durationToOff, &BleRadioEnergyModel::ChangeState, this, BlePhy::State::OFF);
    }
}

BleRadioEnergyModelPhyListener *
BleRadioEnergyModel::GetPhyListener (void)
{
  NS_LOG_FUNCTION (this);
  return m_listener;
}

/*
 * Private functions start here.
 */

void
BleRadioEnergyModel::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_source = NULL;
  m_energyDepletionCallback.Nullify ();
}

double
BleRadioEnergyModel::GetStateA (int state) const
{
  switch (state)
    {
    case BlePhy::State::IDLE:
      return m_idleCurrentA;
    // case BlePhy::State::CCA_BUSY:
    //   return m_ccaBusyCurrentA;
    case BlePhy::State::TX:
      return m_txCurrentA;
    case BlePhy::State::RX:
      return m_rxCurrentA;
    case BlePhy::State::TX_BUSY:
      return m_txCurrentA;
    case BlePhy::State::RX_BUSY:
      return m_rxCurrentA;
    // case BlePhy::State::SWITCHING:
    //   return m_switchingCurrentA;
    // case BlePhy::State::SLEEP:
    //   return m_sleepCurrentA;
    case BlePhy::State::OFF:
      return 0.0;
    }
  NS_FATAL_ERROR ("BleRadioEnergyModel: undefined radio state " << state);
}

double
BleRadioEnergyModel::DoGetCurrentA (void) const
{
  return GetStateA (m_currentState);
}

void
BleRadioEnergyModel::SetBleRadioState (const BlePhy::State state)
{
  NS_LOG_FUNCTION (this << state);
  m_currentState = state;
  std::string stateName;
  switch (state)
    {
    case BlePhy::State::IDLE:
      stateName = "IDLE";
      break;
    // case BlePhy::State::CCA_BUSY:
    //   stateName = "CCA_BUSY";
    //   break;
    case BlePhy::State::TX:
      stateName = "TX";
      break;
    case BlePhy::State::RX:
      stateName = "RX";
      break;
    case BlePhy::State::TX_BUSY:
      stateName = "TX";
      break;
    case BlePhy::State::RX_BUSY:
      stateName = "RX";
      break;
    // case BlePhy::State::SWITCHING:
    //   stateName = "SWITCHING";
    //   break;
    // case BlePhy::State::SLEEP:
    //   stateName = "SLEEP";
    //   break;
    case BlePhy::State::OFF:
      stateName = "OFF";
      break;
    }
  NS_LOG_DEBUG ("BleRadioEnergyModel:Switching to state: " << stateName <<
                " at time = " << Simulator::Now ());
}

// -------------------------------------------------------------------------- //

BleRadioEnergyModelPhyListener::BleRadioEnergyModelPhyListener ()
{
  NS_LOG_FUNCTION (this);
  m_changeStateCallback.Nullify ();
  m_updateTxCurrentCallback.Nullify ();
}

BleRadioEnergyModelPhyListener::~BleRadioEnergyModelPhyListener ()
{
  NS_LOG_FUNCTION (this);
}

void
BleRadioEnergyModelPhyListener::SetChangeStateCallback (DeviceEnergyModel::ChangeStateCallback callback)
{
  NS_LOG_FUNCTION (this << &callback);
  NS_ASSERT (!callback.IsNull ());
  m_changeStateCallback = callback;
}

void
BleRadioEnergyModelPhyListener::SetUpdateTxCurrentCallback (UpdateTxCurrentCallback callback)
{
  NS_LOG_FUNCTION (this << &callback);
  NS_ASSERT (!callback.IsNull ());
  m_updateTxCurrentCallback = callback;
}

void
BleRadioEnergyModelPhyListener::NotifyRxStart (Time duration)
{
  NS_LOG_FUNCTION (this << duration);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (BlePhy::State::RX);
  m_switchToIdleEvent.Cancel ();
}

void
BleRadioEnergyModelPhyListener::NotifyRxEndOk (void)
{
  NS_LOG_FUNCTION (this);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (BlePhy::State::IDLE);
}

void
BleRadioEnergyModelPhyListener::NotifyRxEndError (void)
{
  NS_LOG_FUNCTION (this);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (BlePhy::State::IDLE);
}

void
BleRadioEnergyModelPhyListener::NotifyTxStart (Time duration, double txPowerDbm)
{
  NS_LOG_FUNCTION (this << duration << txPowerDbm);
  if (m_updateTxCurrentCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Update tx current callback not set!");
    }
  m_updateTxCurrentCallback (txPowerDbm);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (BlePhy::State::TX);
  // schedule changing state back to IDLE after TX duration
  m_switchToIdleEvent.Cancel ();
  m_switchToIdleEvent = Simulator::Schedule (duration, &BleRadioEnergyModelPhyListener::SwitchToIdle, this);
}

void
BleRadioEnergyModelPhyListener::NotifyMaybeCcaBusyStart (Time duration)
{
  NS_LOG_FUNCTION (this << duration);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Change state callback not set!");
    }
//   m_changeStateCallback (BlePhy::State::CCA_BUSY);
  // schedule changing state back to IDLE after CCA_BUSY duration
  m_switchToIdleEvent.Cancel ();
  m_switchToIdleEvent = Simulator::Schedule (duration, &BleRadioEnergyModelPhyListener::SwitchToIdle, this);
}

void
BleRadioEnergyModelPhyListener::NotifySwitchingStart (Time duration)
{
  NS_LOG_FUNCTION (this << duration);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Change state callback not set!");
    }
//   m_changeStateCallback (BlePhy::State::SWITCHING);
  // schedule changing state back to IDLE after CCA_BUSY duration
  m_switchToIdleEvent.Cancel ();
  m_switchToIdleEvent = Simulator::Schedule (duration, &BleRadioEnergyModelPhyListener::SwitchToIdle, this);
}

void
BleRadioEnergyModelPhyListener::NotifySleep (void)
{
  NS_LOG_FUNCTION (this);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Change state callback not set!");
    }
//   m_changeStateCallback (BlePhy::State::SLEEP);
  m_switchToIdleEvent.Cancel ();
}

void
BleRadioEnergyModelPhyListener::NotifyWakeup (void)
{
  NS_LOG_FUNCTION (this);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (BlePhy::State::IDLE);
}

void
BleRadioEnergyModelPhyListener::NotifyOff (void)
{
  NS_LOG_FUNCTION (this);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (BlePhy::State::OFF);
  m_switchToIdleEvent.Cancel ();
}

void
BleRadioEnergyModelPhyListener::NotifyOn (void)
{
  NS_LOG_FUNCTION (this);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (BlePhy::State::IDLE);
}

void
BleRadioEnergyModelPhyListener::SwitchToIdle (void)
{
  NS_LOG_FUNCTION (this);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("BleRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (BlePhy::State::IDLE);
}

} // namespace ns3

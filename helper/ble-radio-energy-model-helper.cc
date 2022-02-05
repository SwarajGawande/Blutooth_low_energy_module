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
 * Authors: Sidharth Nabar <snabar@uw.edu>, He Wu <mdzz@u.washington.edu>
 */

#include "ble-radio-energy-model-helper.h"
#include "ns3/ble-net-device.h"
#include "/home/mihir/IoT_HOLA_Project/ns3/ns-3-dev-master/src/ble/model/ble-tx-current-model.h"
#include "ns3/ble-phy.h"

namespace ns3 {

BleRadioEnergyModelHelper::BleRadioEnergyModelHelper ()
{
  m_radioEnergy.SetTypeId ("ns3::BleRadioEnergyModel");
  m_depletionCallback.Nullify ();
  m_rechargedCallback.Nullify ();
}

BleRadioEnergyModelHelper::~BleRadioEnergyModelHelper ()
{
}

void
BleRadioEnergyModelHelper::Set (std::string name, const AttributeValue &v)
{
  m_radioEnergy.Set (name, v);
}

void
BleRadioEnergyModelHelper::SetDepletionCallback (
  BleRadioEnergyModel::BleRadioEnergyDepletionCallback callback)
{
  m_depletionCallback = callback;
}

void
BleRadioEnergyModelHelper::SetRechargedCallback (
  BleRadioEnergyModel::BleRadioEnergyRechargedCallback callback)
{
  m_rechargedCallback = callback;
}

void
BleRadioEnergyModelHelper::SetTxCurrentModel (std::string name,
                                               std::string n0, const AttributeValue& v0,
                                               std::string n1, const AttributeValue& v1,
                                               std::string n2, const AttributeValue& v2,
                                               std::string n3, const AttributeValue& v3,
                                               std::string n4, const AttributeValue& v4,
                                               std::string n5, const AttributeValue& v5,
                                               std::string n6, const AttributeValue& v6,
                                               std::string n7, const AttributeValue& v7)
{
  ObjectFactory factory;
  factory.SetTypeId (name);
  factory.Set (n0, v0);
  factory.Set (n1, v1);
  factory.Set (n2, v2);
  factory.Set (n3, v3);
  factory.Set (n4, v4);
  factory.Set (n5, v5);
  factory.Set (n6, v6);
  factory.Set (n7, v7);
  m_txCurrentModel = factory;
}


/*
 * Private function starts here.
 */

Ptr<DeviceEnergyModel>
BleRadioEnergyModelHelper::DoInstall (Ptr<NetDevice> device,
                                       Ptr<EnergySource> source) const
{
  NS_ASSERT (device != NULL);
  NS_ASSERT (source != NULL);
  // check if device is BleNetDevice
  std::string deviceName = device->GetInstanceTypeId ().GetName ();
  if (deviceName.compare ("ns3::BleNetDevice") != 0)
    {
      NS_FATAL_ERROR ("NetDevice type is not BleNetDevice!");
    }
  Ptr<Node> node = device->GetNode ();
  Ptr<BleRadioEnergyModel> model = m_radioEnergy.Create ()->GetObject<BleRadioEnergyModel> ();
  NS_ASSERT (model != NULL);

  // set energy depletion callback
  // if none is specified, make a callback to BlePhy::SetOffMode
  Ptr<BleNetDevice> BleDevice = DynamicCast<BleNetDevice> (device);
  Ptr<BlePhy> BlePhy = BleDevice->GetPhy ();
  BlePhy->SetBleRadioEnergyModel (model);
  if (m_depletionCallback.IsNull ())
    {
      model->SetEnergyDepletionCallback (MakeCallback (&BlePhy::SetOffMode, BlePhy));
    }
  else
    {
      model->SetEnergyDepletionCallback (m_depletionCallback);
    }
  // set energy recharged callback
  // if none is specified, make a callback to BlePhy::ResumeFromOff
  if (m_rechargedCallback.IsNull ())
    {
      model->SetEnergyRechargedCallback (MakeCallback (&BlePhy::ResumeFromOff, BlePhy));
    }
  else
    {
      model->SetEnergyRechargedCallback (m_rechargedCallback);
    }
  // add model to device model list in energy source
  source->AppendDeviceEnergyModel (model);
  // set energy source pointer
  model->SetEnergySource (source);
  // create and register energy model PHY listener
  BlePhy->RegisterListener (model->GetPhyListener ());
  //
  if (m_txCurrentModel.GetTypeId ().GetUid ())
    {
      Ptr<BleTxCurrentModel> txcurrent = m_txCurrentModel.Create<BleTxCurrentModel> ();
      model->SetTxCurrentModel (txcurrent);
    }
  return model;
}

} // namespace ns3

/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Written by Kevin Woodward, keawoodw@ucsc.edu
 * CSE250A, Fall 2019
 * 
 * This is a simulation design to attempt to recreate the results in 
 * "A machine learning framework for TCP round-trip time estimation".
 * 
 * This code was based off of the included ns-3 manet-routing-compare.cc
 * example. 
**/

#include <fstream>
#include <iostream>
#include <random>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Begin trace setup code
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  *stream->GetStream () << newCwnd << std::endl;

}

void SetCallback(Ptr<BulkSendApplication> app)
{
  // Get socket from app
  Ptr<Socket> sock = app->GetSocket();

  // Setup stream
  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("s1.cwnd", std::ios::app);

  sock->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream));
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// End trace setup code
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class RttExperiment
{
public:
  RttExperiment();
  void Run ();
  std::string GetCsvFilename();
  int GetNumFlows();
  int GetNumNodes();
  int GetPower();

private:
  uint32_t port;
  int m_numFlows;
  int m_numNodes;

};

NS_LOG_COMPONENT_DEFINE ("RttExperiment");

RttExperiment::RttExperiment ()
  : port (1024),
    m_numFlows(68), // To change number of flows (3	7	17	34	68	100	130)
    m_numNodes(20) // To change number of nodes
{
}

int RttExperiment::GetNumFlows() { return m_numFlows; }
int RttExperiment::GetNumNodes() { return m_numNodes; }

void RttExperiment::Run()
{
  // Seed RNG for multiple future uses
  srand(time(NULL));

  Packet::EnablePrinting ();

  // Setup simulation parameters
  double simTime = 25.0*60.0; // In seconds
  int nodeSpeed = 50; // Max speed in meters/second
  int nodePause = 0;
  std::string phyMode ("DsssRate1Mbps");

  // Set default attributes to match paper

  // Comment below line for simulation to use MeanDevation method
  Config::SetDefault ("ns3::TcpL4Protocol::RttEstimatorType", TypeIdValue(RttFixedShare::GetTypeId()));

  Config::SetDefault ("ns3::BulkSendApplication::Protocol",   TypeIdValue (TcpSocketFactory::GetTypeId ()));
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue (phyMode));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (16384));
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (16384));
  Config::SetDefault ("ns3::RangePropagationLossModel::MaxRange", DoubleValue (100.0));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue(1));
  

  NodeContainer adhocNodes;
  adhocNodes.Create (GetNumNodes());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel");

  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));

  
  wifiMac.SetType ("ns3::AdhocWifiMac");
  
  NetDeviceContainer adhocDevices = wifi.Install (wifiPhy, wifiMac, adhocNodes);

  MobilityHelper mobilityAdhoc;
  int64_t streamIndex = 0; // used to get consistent mobility across scenarios

  ObjectFactory pos;
  pos.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  pos.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1500.0]"));
  pos.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=1000.0]"));

  Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
  streamIndex += taPositionAlloc->AssignStreams (streamIndex);

  std::stringstream ssSpeed;
  // Scenario 1: 1-50 node speed
  ssSpeed << "ns3::UniformRandomVariable[Min=1.0|Max=" << nodeSpeed << "]";
  std::stringstream ssPause;
  ssPause << "ns3::ConstantRandomVariable[Constant=" << nodePause << "]";
  mobilityAdhoc.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                  "Speed", StringValue (ssSpeed.str ()),
                                  "Pause", StringValue (ssPause.str ()),
                                  "PositionAllocator", PointerValue (taPositionAlloc));
  mobilityAdhoc.SetPositionAllocator (taPositionAlloc);
  mobilityAdhoc.Install (adhocNodes);
  streamIndex += mobilityAdhoc.AssignStreams (adhocNodes, streamIndex);
  NS_UNUSED (streamIndex); // From this point, streamIndex is unused

  AodvHelper aodv;
  Ipv4ListRoutingHelper list;
  InternetStackHelper internet;

  internet.SetTcp("ns3::TcpL4Protocol");
  list.Add (aodv, 100);
  internet.SetRoutingHelper (list);
  internet.Install (adhocNodes);

  Ipv4AddressHelper addressAdhoc;
  addressAdhoc.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer adhocInterfaces;
  adhocInterfaces = addressAdhoc.Assign (adhocDevices);  

  std::vector<Ptr<BulkSendApplication> > apps;
  std::vector<int> startTimes;

  // Maps a flow x to y into a custom hash, 2^x + 2^y.
  // This is invariant to the direction of the flow, and 
  // prevents an x->y duplicate or a y->x flow given that 
  // there is already an x->y flow, as ns3 crashes for some
  // reason when this occurs (something about a packet 
  // merge being impossible). While this does limit us to 
  // 20 choose 2 = 190 flows, this isn't an issue for my 
  // simulations.
  // In short, this ensures a unique (x, y) flow in either direction.
  // std::map<int, int> flowMap;

  for (int i = 0; i < GetNumFlows(); i++)
  {
    int senderIndex = rand() % GetNumNodes();
    int receiverIndex = rand() % GetNumNodes();

    // int hashedFlow = pow(2, senderIndex) + pow(2, receiverIndex);

    while (senderIndex == receiverIndex)// || flowMap.count(hashedFlow) > 0) // Ensure not sending to self
    {
      // senderIndex = rand() % GetNumNodes();
      receiverIndex = rand() % GetNumNodes();
      // hashedFlow = pow(2, senderIndex) + pow(2, receiverIndex);
    }

    // flowMap[hashedFlow] = 1;

    NS_LOG_DEBUG("Flow from: " << senderIndex << " to: " << receiverIndex);

    int numPackets = 1000 + (rand() % 99001); // Random number of packets between 1,000 and 100,000    

    NS_LOG_DEBUG("Sending " << numPackets << " packets");

    int startTime = rand() % static_cast<int>(simTime); // Time when to start sending data
    startTimes.push_back(startTime);

    //Sender 
    Ptr<Node> node = adhocNodes.Get (senderIndex);

    //Receiver 
    Ptr<Node> nextNode = adhocNodes.Get (receiverIndex);

    BulkSendHelper sendHelper ("ns3::TcpSocketFactory", (InetSocketAddress (adhocInterfaces.GetAddress (receiverIndex), port))); // To address
    ApplicationContainer senderApp = sendHelper.Install(node); // Install onto source

    // Ensure max data sent and set up cwnd trace
    Ptr<BulkSendApplication> bsApp = DynamicCast<BulkSendApplication> (senderApp.Get(0));
    apps.push_back(bsApp);
    
    bsApp->SetMaxBytes(512 * numPackets);

    // Set up receiver 
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress(adhocInterfaces.GetAddress (receiverIndex), port++)); // To address
    ApplicationContainer sinkApp = sinkHelper.Install(nextNode); // Install onto sink

    sinkApp.Start (Seconds (startTime));
    senderApp.Start (Seconds (startTime));
    sinkApp.Stop (Seconds (simTime));
    senderApp.Stop (Seconds (simTime));
  }

  for (int i = 0; i < apps.size(); i++)
  {
    Simulator::Schedule(Seconds ((double)startTimes[i] + 0.00001), &SetCallback, apps[i]);
  }

  // Set up flow monitoring
  Ptr<FlowMonitor> flowmon;
  FlowMonitorHelper flowmonHelper;
  flowmon = flowmonHelper.InstallAll ();

  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  flowmon->SerializeToXmlFile ("s1.flowmon", false, false);
}


int 
main (int argc, char *argv[])
{
  LogComponentEnable ("RttExperiment", LOG_LEVEL_INFO);

  RttExperiment experiment;

  experiment.Run();
}

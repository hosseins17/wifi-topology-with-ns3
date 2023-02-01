/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */


#include <cstdlib>
#include<time.h>
#include <stdio.h>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

// Default Network Topology
//
//   Wifi 10.1.3.0
//            AP
//  *    *    *    *   *
//  |    |    |   |    |
// n0   n1   n3  n4   n5


using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("ThirdScriptExample");

int packets_number = 1;
Ptr<PacketSink> tcpSink;

//vector<Ptr<Socket>> receiverSockets;

void
ThroughputMonitor (FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon)
{
    //double localThrou = 0;
    std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats ();
    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin (); stats != flowStats.end (); ++stats)
    {

        Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow (stats->first);
        std::cout << "Flow ID			: "<< stats->first << " ; " << fiveTuple.sourceAddress << " -----> " << fiveTuple.destinationAddress << std::endl;
        std::cout << "Tx Packets = " << stats->second.txPackets << std::endl;
        std::cout << "Rx Packets = " << stats->second.rxPackets << std::endl;
        std::cout << "Duration		: "<< (stats->second.timeLastRxPacket.GetSeconds () - stats->second.timeFirstTxPacket.GetSeconds ()) << std::endl;
        std::cout << "Last Received Packet	: "<< stats->second.timeLastRxPacket.GetSeconds () << " Seconds" << std::endl;
        std::cout << "Throughput: " << stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds () - stats->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024  << " Mbps" << std::endl;
        //localThrou = stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds () - stats->second.timeFirstTxPacket.GetSeconds ()) / 1024 / 1024;

        std::cout << "---------------------------------------------------------------------------" << std::endl;
        flowMon = fmhelper->InstallAll ();
    }
    Simulator::Schedule (Seconds (10),&ThroughputMonitor, fmhelper, flowMon);
    flowMon->SerializeToXmlFile ("ThroughputMonitor.xml", true, true);
}

class lb : public Application
{
public:
    lb (uint16_t myPort, Ipv4InterfaceContainer& receivers);
    virtual ~lb ();

private:
    virtual void StartApplication (void);
//    virtual void StopApplication (void);

    void HandleRead (Ptr<Socket> socket);

    uint16_t myPort;
    Ptr<Socket> mySocket;
    Ipv4InterfaceContainer myReceivers;
    std::vector<Ptr<Socket>> receiverSockets;
    // Ptr<Socket> out_socket;
};


void
pnum(Ptr<PacketSink> udpSink){
    std::cout << "test packet is -----> " << udpSink->GetTotalRx()<<std::endl;
    packets_number = udpSink->GetTotalRx()/512;
}



int
main (int argc, char *argv[])
{


    uint32_t maxBytes = 0;
    bool verbose = true;
    uint32_t nWifi = 2;
    double duration = 10.0;
    bool tracing = false;

    //generate random number
    srand(time(NULL));
//    uint32_t random;
//    random = ();

    CommandLine cmd (__FILE__);
    cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

    cmd.Parse (argc,argv);

    // The underlying restriction of 18 is due to the grid position
    // allocator's configuration; the grid layout will exceed the
    // bounding box if more than 18 nodes are provided.
    if (nWifi > 18)
    {
        std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box" << std::endl;
        return 1;
    }

    if (verbose)
    {
        LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer wifiStaNodesLeft;
    wifiStaNodesLeft.Create (nWifi);

    NodeContainer wifiStaNodesRight;
    wifiStaNodesRight.Create (nWifi);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy;
    phy.SetChannel (channel.Create ());

    WifiHelper wifi;
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns-3-ssid");
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));

    NetDeviceContainer staDevicesLeft;
    staDevicesLeft = wifi.Install (phy, mac, wifiStaNodesLeft);

    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", SsidValue (ssid),
                 "ActiveProbing", BooleanValue (false));

    NetDeviceContainer staDevicesRight;
    staDevicesRight = wifi.Install (phy, mac, wifiStaNodesRight);


    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", SsidValue (ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install (phy, mac, wifiApNode);

    MobilityHelper mobility;

    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0),
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (5.0),
                                   "DeltaY", DoubleValue (10.0),
                                   "GridWidth", UintegerValue (3),
                                   "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
    mobility.Install (wifiStaNodesLeft);

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiStaNodesRight);

    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);

    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodesLeft);
    stack.Install (wifiStaNodesRight);

    Ipv4AddressHelper address;

    Ipv4InterfaceContainer staNodesLeftInterface;
    Ipv4InterfaceContainer staNodesRightInterface;
    Ipv4InterfaceContainer apNodeInterface;

    address.SetBase ("10.1.3.0", "255.255.255.0");
    staNodesLeftInterface = address.Assign (staDevicesLeft);
    apNodeInterface = address.Assign (apDevices);
    staNodesRightInterface = address.Assign (staDevicesRight);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // Create a BulkSendApplication and install it on node 0.
    uint16_t port = 1102;

    OnOffHelper source ("ns3::UdpSocketFactory",InetSocketAddress (apNodeInterface.GetAddress(0), port));

    // Set the amount of data to send in bytes. Zero is unlimited.
    source.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
    ApplicationContainer apps = source.Install (wifiStaNodesLeft.Get (0));

    apps.Start (Seconds (0.0));
    apps.Stop (Seconds (5.0));

    // Create a PacketSinkApplication and install it on node 1.
//    PacketSinkHelper sink ("ns3::UdpSocketFactory",
//                           InetSocketAddress (Ipv4Address::GetAny (), port));
//    apps = sink.Install (wifiApNode.Get (0));
//
//    apps.Start (Seconds (0.0));
//    apps.Stop (Seconds (5.0));

    // Load Balancer
    Ptr<lb> lbApp = CreateObject<lb> (port, staNodesRightInterface);
    wifiApNode.Get (0)->AddApplication (lbApp);
    lbApp->SetStartTime (Seconds (0.0));
    lbApp->SetStopTime (Seconds (10.0));


    // Receivers
    PacketSinkHelper sink ("ns3::TcpSocketFactory",
                           InetSocketAddress (Ipv4Address::GetAny (), port));
    ApplicationContainer sinkApps = sink.Install (wifiStaNodesRight);
    sinkApps.Start (Seconds (5.0));
    sinkApps.Stop (Seconds (10.0));




    NS_LOG_INFO ("Run Simulation");

//    std::string fileNameWithNoExtension = "FlowVSThroughput_";
//    std::string mainPlotTitle = "Flow vs Throughput";
//    std::string graphicsFileName        = fileNameWithNoExtension + ".png";
//    std::string plotFileName            = fileNameWithNoExtension + ".plt";
//    std::string plotTitle               = mainPlotTitle + ", Error: " + std::to_string(error);
//    std::string dataTitle               = "Throughput";
//
//    // Instantiate the plot and set its title.
//    Gnuplot gnuplot (graphicsFileName);
//    gnuplot.SetTitle (plotTitle);
//
//    // Make the graphics file, which the plot file will be when it
//    // is used with Gnuplot, be a PNG file.
//    gnuplot.SetTerminal ("png");
//
//    // Set the labels for each axis.
//    gnuplot.SetLegend ("Flow", "Throughput");


//    Gnuplot2dDataset dataset;
//    dataset.SetTitle (dataTitle);
//    dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    // Flow monitor.
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll ();

    ThroughputMonitor (&flowHelper, flowMonitor);

    Simulator::Stop (Seconds (duration));
    Simulator::Run ();

//    //Gnuplot ...continued.
//    gnuplot.AddDataset (dataset);
//    // Open the plot file.
//    std::ofstream plotFile (plotFileName.c_str ());
//    // Write the plot file.
//    gnuplot.GenerateOutput (plotFile);
//    // Close the plot file.
//    plotFile.close ();


    return 0;
}

lb::lb (uint16_t myPort, Ipv4InterfaceContainer& receivers)
        : myPort (myPort),
          myReceivers (receivers)
{
    std::srand (time(0));
}

lb::~lb ()
{
}

void
lb::StartApplication (void)
{
    mySocket = Socket::CreateSocket (GetNode (), UdpSocketFactory::GetTypeId ());
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), myPort);
    mySocket->Bind (local);

    for (uint32_t i = 0; i < myReceivers.GetN (); i++)
    {
        Ptr<Socket> sock = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
        InetSocketAddress sockAddr (myReceivers.GetAddress (i), myPort);
        sock->Connect (sockAddr);
        receiverSockets.push_back (sock);
    }

    mySocket->SetRecvCallback (MakeCallback (&lb::HandleRead, this));


}


void lb::HandleRead (Ptr<Socket> socket)
{

    Ptr<Packet> packet;
    Address from;

    while ((packet = socket->RecvFrom (from)))
    {
        if (packet->GetSize () == 0)
        {
            break;
        }

        // select a random receiver and send received message in TCP
        int receiverIndex = rand () % myReceivers.GetN ();

        // send message to the receiver
        receiverSockets[receiverIndex]->Send (packet);

    }
}

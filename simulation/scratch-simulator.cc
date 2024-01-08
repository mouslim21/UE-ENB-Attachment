#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/lte-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/config-store-module.h"
#include "ns3/netanim-module.h"
#include "ns3/gnuplot.h"
#include "ns3/lte-helper.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/vector.h"
#include "ns3/mobility-model.h"
#include <map>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <math.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("lte_simulation");


struct SimulationResult {
    double averageThroughput;
    double averageDelay;
    double averageJitter;
};

void
NotifyConnectionEstablishedUe (std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti)			
{
  std::cout << Simulator::Now ().GetSeconds () << " " << context << " UE IMSI " << imsi << ": connected to CellId " << cellid << " with RNTI " << rnti << std::endl;
}


void
NotifyConnectionEstablishedEnb (std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti)
{
  std::cout << Simulator::Now ().GetSeconds () << " " << context << " eNB CellId " << cellid << ": successful connection of UE with IMSI " << imsi  << " RNTI " << rnti << std::endl;
}

double sumOfAvgThroughputs = 0.0;
double sumOfAvgDelays = 0;
double sumOfAvgJitters = 0;
int intervalCount = 0;

void ThroughputMonitor (FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon, Gnuplot2dDataset DataSet, uint32_t numberOfUes) {
    double localThrou = 0;
    double totalThrou = 0; // Total throughput for all users
    double totalDelay = 0;
    double totalJitter = 0;
    std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());
    
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin(); stats != flowStats.end(); ++stats) {
        Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow (stats->first);
         if (fiveTuple.sourceAddress == Ipv4Address("1.0.0.2") )
        {
            localThrou = stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds())/1024 ; // Throughput in Kbps
            totalThrou += localThrou; // Aggregate the throughput
            
            if (stats->second.rxPackets > 0) {
          totalDelay += stats->second.delaySum.GetSeconds() / stats->second.rxPackets;
          totalJitter += stats->second.jitterSum.GetSeconds()/ stats->second.rxPackets;
        }
            // Continue with your existing logging
            std::cout << "Flow ID: " << stats->first << " ; " << fiveTuple.sourceAddress << " -----> " << fiveTuple.destinationAddress << std::endl;
            std::cout << "Mean{Delay}: " << (stats->second.delaySum.GetSeconds() / stats->second.rxPackets) << "\n";
            std::cout<< "Mean{Jitter}: " << (stats->second.jitterSum.GetSeconds()/(stats->second.rxPackets)) << "\n";
            std::cout<< "Lost Packets: " << (stats->second.lostPackets) << "\n";
            std::cout << "Tx Packets: " << stats->second.txPackets << std::endl;
            std::cout << "Rx Packets: " << stats->second.rxPackets << std::endl;
            std::cout << "Throughput: " << localThrou << " Kbps" << std::endl;
            // Other logs...
            DataSet.Add((double)Simulator::Now().GetSeconds(), localThrou);
            std::cout << "---------------------------------------------------------------------------" << std::endl;
        }
    }

    sumOfAvgThroughputs += totalThrou / numberOfUes; // Average throughput per user in Kbps;
    sumOfAvgDelays += totalDelay / numberOfUes;
    sumOfAvgJitters += totalJitter / numberOfUes;
    intervalCount++;

    Simulator::Schedule(Seconds(0.2), &ThroughputMonitor, fmhelper, flowMon, DataSet, numberOfUes);
    flowMon->SerializeToXmlFile("ThroughputMonitor.xml", true, true);
}

/**/

std::vector<Vector> ReadPositionsFromFile(std::string filename) {
    std::vector<Vector> positions;
    std::ifstream inFile(filename);
    double latitude, longitude;
    while (inFile >> latitude >> longitude) {
        // Convert latitude and longitude to x, y coordinates if needed
        double x = longitude; // Placeholder conversion
        double y = latitude;  // Placeholder conversion
        positions.push_back(Vector(x, y, 0)); // Assuming z=0 (flat terrain)
    }
    return positions;
}


/**
 * Sample simulation script for a X2-based handover.
 * It instantiates two eNodeB, attaches one UE to the 'source' eNB and
 * triggers a handover of the UE towards the 'target' eNB.
 */
int
main (int argc, char *argv[])
{
  uint16_t numberOfUes = 1;
  uint16_t numberOfEnbs = 1;
  std::string  resultsFile = "";
  double simTime = 1.0;

  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (MilliSeconds (20)));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (1000000));
  Config::SetDefault ("ns3::LteEnbRrc::SrsPeriodicity", UintegerValue(320));
  Config::SetDefault ("ns3::LteHelper::UseIdealRrc", BooleanValue (false));

  CommandLine cmd;
  cmd.AddValue ("numberOfUes", "Number of UEs", numberOfUes);
  cmd.AddValue ("numberOfEnbs", "Number of eNodeBs", numberOfEnbs);
  cmd.AddValue ("resultsFile", "results", resultsFile);
  cmd.Parse (argc, argv);
 
  std::cout << "Number of UEs: " << numberOfUes << std::endl;
  std::cout << "Number of eNodeBs: " << numberOfEnbs << std::endl;

  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);
  lteHelper->SetSchedulerType ("ns3::RrFfMacScheduler");
  lteHelper->SetEnbDeviceAttribute("DlBandwidth", UintegerValue(100));
  lteHelper->SetEnbDeviceAttribute("UlBandwidth", UintegerValue(100));

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

 // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer); 

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);


  // Routing of the Internet Host (towards the LTE network)
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create (numberOfEnbs);
  ueNodes.Create (numberOfUes);

 // Read positions from file
    std::vector<Vector> uePositions = ReadPositionsFromFile("ue_positions.txt");
    std::vector<Vector> enbPositions = ReadPositionsFromFile("enb_positions.txt");

    // Set Mobility for UEs
    MobilityHelper mobilityUe;
    Ptr<ListPositionAllocator> positionAllocUe = CreateObject<ListPositionAllocator>();
    for (const Vector& pos : uePositions) {
        positionAllocUe->Add(pos);
    }
    mobilityUe.SetPositionAllocator(positionAllocUe);
    mobilityUe.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityUe.Install(ueNodes);

    // Set Mobility for eNodeBs
    MobilityHelper mobilityEnb;
    Ptr<ListPositionAllocator> positionAllocEnb = CreateObject<ListPositionAllocator>();
    for (const Vector& pos : enbPositions) {
        positionAllocEnb->Add(pos);
    }
    mobilityEnb.SetPositionAllocator(positionAllocEnb);
    mobilityEnb.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobilityEnb.Install(enbNodes);
  


  // Install LTE Devices in eNB and UEs
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIfaces;
  ueIpIfaces = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));


  //lteHelper->AttachToClosestEnb (ueLteDevs, enbLteDevs);
  //AttachUesToBestSignalEnb(lteHelper, ueLteDevs, enbNodes);

      // Reading UE to eNodeB attachment decisions
    std::map<uint32_t, uint32_t> ueToEnbMap;
    std::ifstream inFile("attachment_decisions.txt"); // Adjust the path as necessary
    uint32_t ueId, enbId;

    while (inFile >> ueId >> enbId) {
        if (ueId < ueLteDevs.GetN() && enbId < enbLteDevs.GetN()) {
            ueToEnbMap[ueId] = enbId;
        } else {
            std::cout << "Warning: Invalid UE ID or eNodeB ID in attachment decisions file." << std::endl;
        }
    }
    inFile.close();

    // Attach UEs to eNodeBs based on decisions
    for (uint32_t u = 0; u < ueLteDevs.GetN(); ++u) {
        auto it = ueToEnbMap.find(u);
        if (it != ueToEnbMap.end()) {
            uint32_t enbIndex = it->second;
            if (enbIndex < enbLteDevs.GetN()) {
                Ptr<NetDevice> enbDevice = enbLteDevs.Get(enbIndex);
                lteHelper->Attach(ueLteDevs.Get(u), enbDevice);
            } else {
                std::cout << "Warning: eNodeB index out of range for UE ID " << u << std::endl;
            }
        } else {
            std::cout << "Warning: No attachment decision found for UE ID " << u << std::endl;
        }
    }   
  NS_LOG_LOGIC ("setting up applications");

  // Install and start applications on UEs and remote host
  uint16_t dlPort = 10000;
  uint16_t ulPort = 20000;

  // randomize a bit start times to avoid simulation artifacts
  // (e.g., buffer overflows due to packet transmissions happening
  // exactly at the same time)
  Ptr<UniformRandomVariable> startTimeSeconds = CreateObject<UniformRandomVariable> ();
  startTimeSeconds->SetAttribute ("Min", DoubleValue (0));
  startTimeSeconds->SetAttribute ("Max", DoubleValue (0.010));
  

  for (uint32_t u = 0; u < numberOfUes; ++u)
    {
      Ptr<Node> ue = ueNodes.Get (u);
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ue->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

      //for (uint32_t b = 0; b < numBearersPerUe; ++b)
        {
          ++dlPort;
          ++ulPort;

          ApplicationContainer clientApps;
          ApplicationContainer serverApps;

          NS_LOG_LOGIC ("installing UDP DL app for UE " << u);
          UdpClientHelper dlClientHelper (ueIpIfaces.GetAddress (u), dlPort);
          clientApps.Add (dlClientHelper.Install (remoteHost));
          PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory",
                                               InetSocketAddress (Ipv4Address::GetAny (), dlPort));
          serverApps.Add (dlPacketSinkHelper.Install (ue));

          NS_LOG_LOGIC ("installing UDP UL app for UE " << u);
          UdpClientHelper ulClientHelper (remoteHostAddr, ulPort);
          clientApps.Add (ulClientHelper.Install (ue));
          PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory",
                                               InetSocketAddress (Ipv4Address::GetAny (), ulPort));
          serverApps.Add (ulPacketSinkHelper.Install (remoteHost));

          Ptr<EpcTft> tft = Create<EpcTft> ();
          EpcTft::PacketFilter dlpf;
          dlpf.localPortStart = dlPort;
          dlpf.localPortEnd = dlPort;
          tft->Add (dlpf);
          EpcTft::PacketFilter ulpf;
          ulpf.remotePortStart = ulPort;
          ulpf.remotePortEnd = ulPort;
          tft->Add (ulpf);
          EpsBearer bearer (EpsBearer::GBR_CONV_VOICE);
          lteHelper->ActivateDedicatedEpsBearer (ueLteDevs.Get (u), bearer, tft);

          Time startTime = Seconds (startTimeSeconds->GetValue ());
          serverApps.Start (startTime);
          clientApps.Start (startTime);

        } 
    }



  lteHelper->EnablePhyTraces ();
  lteHelper->EnableMacTraces ();
  lteHelper->EnableRlcTraces ();
  lteHelper->EnablePdcpTraces ();
   Ptr<RadioBearerStatsCalculator> rlcStats = lteHelper->GetRlcStats ();
   rlcStats->SetAttribute ("EpochDuration", TimeValue (Seconds (0.020)));
  Ptr<RadioBearerStatsCalculator> pdcpStats = lteHelper->GetPdcpStats ();
  pdcpStats->SetAttribute ("EpochDuration", TimeValue (Seconds (0.02)));

   //pointToPoint.EnablePcapAll ("blabla");


  // connect custom trace sinks for RRC connection establishment and handover notification
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/ConnectionEstablished",
                   MakeCallback (&NotifyConnectionEstablishedEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished",
                   MakeCallback (&NotifyConnectionEstablishedUe));



  Simulator::Stop (Seconds (simTime));
  //AnimationInterface anim ("lte2.xml");
  //anim.EnablePacketMetadata ();
  //anim.SetMaxPktsPerTraceFile (100000000000);
  //anim.SetMobilityPollInterval(Seconds(1));
  //anim.EnablePacketMetadata(true);
    std::string fileNameWithNoExtension = "FlowVSThroughput_";
    std::string graphicsFileName        = fileNameWithNoExtension + ".png";
    std::string plotFileName            = fileNameWithNoExtension + ".plt";
    std::string plotTitle               = "Flow vs Throughput";
    std::string dataTitle               = "Throughput";

    // Instantiate the plot and set its title.
    Gnuplot gnuplot (graphicsFileName);
    gnuplot.SetTitle (plotTitle);

    // Make the graphics file, which the plot file will be when it
    // is used with Gnuplot, be a PNG file.
    gnuplot.SetTerminal ("png");

    // Set the labels for each axis.
    gnuplot.SetLegend ("Flow", "Throughput");

     
   Gnuplot2dDataset dataset;
   dataset.SetTitle (dataTitle);
   dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

  //flowMonitor declaration
  FlowMonitorHelper fmHelper;
  Ptr<FlowMonitor> allMon = fmHelper.InstallAll();
  allMon->CheckForLostPackets ();

  
  // call the flow monitor function
  ThroughputMonitor(&fmHelper, allMon, dataset, numberOfUes); 

  Simulator::Run ();

  //Gnuplot ...continued
  gnuplot.AddDataset (dataset);
  // Open the plot file.
  std::ofstream plotFile (plotFileName.c_str());
  // Write the plot file.
  gnuplot.GenerateOutput (plotFile);
  // Close the plot file.
  plotFile.close ();

  // GtkConfigStore config;
  // config.ConfigureAttributes ();

  double overallAverageThroughput = sumOfAvgThroughputs / intervalCount;
  std::cout << "Overall Average Throughput: " << overallAverageThroughput << " Kbps" << std::endl;

  double overallAverageDelays= sumOfAvgDelays / intervalCount;
  std::cout << "Overall Average Delay: " << overallAverageDelays << " s" << std::endl;

  double overallAverageJitters= sumOfAvgJitters / intervalCount;
  std::cout << "Overall Average Jitter: " << overallAverageJitters << " s" << std::endl;
  
    
 
   // Define the map for results
    std::map<int, std::map<int, SimulationResult>> resultsMap;

    // Read existing data from file
    std::ifstream inFile2(resultsFile);
    std::string line;
    int readNumberOfEnbs, readNumberOfUes;
    double readThroughput, readDelay, readJitter;
    while (std::getline(inFile2, line)) {
        std::istringstream iss(line);
        if (iss >> readNumberOfEnbs >> readNumberOfUes >> readThroughput >> readDelay >> readJitter) {
            resultsMap[readNumberOfEnbs][readNumberOfUes] = {readThroughput, readDelay, readJitter};
        }
    }
    inFile2.close();

    resultsMap[numberOfEnbs][numberOfUes] = {overallAverageThroughput, overallAverageDelays, overallAverageJitters};

    // Write results back to the file
    std::ofstream outFile(resultsFile);
    for (const auto& enbPair : resultsMap) {
        for (const auto& uePair : enbPair.second) {
            outFile << enbPair.first << " " << uePair.first << " "
                    << uePair.second.averageThroughput << " "
                    << uePair.second.averageDelay << " "
                    << uePair.second.averageJitter << std::endl;
        }
    }
    outFile.close();


  Simulator::Destroy ();
  return 0;
}
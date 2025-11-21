#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ns2-mobility-helper.h"  

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANET_Accident_Alert");

// Callback function for received packet
void ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Node> node = socket->GetNode();
  int id = node->GetId();

  std::string nodeType = (id < 10) ? "Car" : "RSU";

  Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
  Vector pos = mob->GetPosition();

  // Define accident
  Ptr<MobilityModel> accidentMob = NodeList::GetNode(0)->GetObject<MobilityModel>();
  Vector accidentPos = accidentMob->GetPosition();

  double dx = pos.x - accidentPos.x;
  double dy = pos.y - accidentPos.y;
  double distance = std::sqrt(dx* dx + dy*dy);


  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    std::string data;
    data.resize(packet->GetSize());
    packet->CopyData((uint8_t*)data.data(), packet->GetSize());

    if (distance <= 150) {
      NS_LOG_UNCOND(nodeType << " " << id << " received alert: " << data << " (distance = " << distance << " )");
      // Simulator::Schedule(Seconds(0.5), [=]() {
      //   Ptr<Socket> ackSocket = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
      //   ackSocket->SetAllowBroadcast(true);
      //   InetSocketAddress backIp = InetSocketAddress(Ipv4Address("255.255.255.255"), 80);
      //   ackSocket->Connect(backIp);
      //   std::string ackData = "I received alert from car 0";
      //   Ptr<Packet> ackPacket = Create<Packet>((uint8_t*)ackData.c_str(), ackData.length());
      //   ackSocket->Send(ackPacket);
      //   NS_LOG_UNCOND(nodeType << " " << id << " acknowledge alert at " << Simulator::Now().GetSeconds() <<"s");
      // });
    }

    if (nodeType == "RSU") {
      Simulator::Schedule(Seconds(1.0), [=] () {
        Ptr<Socket> rsuSender = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
        rsuSender->SetAllowBroadcast(true);
        InetSocketAddress remote = InetSocketAddress(Ipv4Address("255.255.255.255"), 80);
        rsuSender->Connect(remote);
        Ptr<Packet> newPacket = Create<Packet>((uint8_t*)data.c_str(), data.length());
        rsuSender->Send(newPacket);
        NS_LOG_UNCOND("RSU " << id << " rebroadcast alert at " << Simulator::Now().GetSeconds() <<"s");
      });
    }
  }
}

int main(int argc, char *argv[])
{
  std::string traceFile = "/home/mojinn/sumo-src/sumo/tools/demo_DUT_small_map/mobility.tcl";
  uint32_t carNum = 10;
  uint32_t rsuNum = 50;
  double duration = 100.0;

  CommandLine cmd(__FILE__);
  cmd.AddValue("traceFile", "Ns2 movement trace file", traceFile);
  cmd.AddValue("carNum", "Number of cars", carNum);
  cmd.AddValue("duration", "Duration of Simulation", duration);
  cmd.Parse(argc, argv);

  LogComponentEnable("VANET_Accident_Alert", LOG_LEVEL_INFO);


  NodeContainer cars;
  cars.Create(carNum);

  NodeContainer rsu;
  rsu.Create(rsuNum);

  //Step 1: Config WiFi Ad-hoc
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211p);
  // wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6MbpsBW10MHz"), "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

  YansWifiPhyHelper phy;
  phy.Set("TxPowerStart", DoubleValue(30.0)); // in dBm, tune up or down
  phy.Set("TxPowerEnd", DoubleValue(30.0));
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  NetDeviceContainer carDevices = wifi.Install(phy, mac, cars);
  NetDeviceContainer rsuDevices = wifi.Install(phy, mac, rsu);


  //Step 2: Internet stack
  InternetStackHelper internet;
  internet.Install(cars);
  internet.Install(rsu);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer nodeInterfaces = ipv4.Assign(carDevices);
  Ipv4InterfaceContainer rsuInterfaces = ipv4.Assign(rsuDevices);

  //Step 3: Connect mobility file from SUMO trace and configure RSU positions
  Ns2MobilityHelper ns2 = Ns2MobilityHelper(traceFile);
  ns2.Install();

  // Configure RSU positions
  MobilityHelper rsuMobility;
  Ptr<ListPositionAllocator> rsuPositionAlloc = CreateObject<ListPositionAllocator>();
  for (uint32_t i = 0; i < rsuNum; ++i) {
    int px = i % 7;
    int py = i / 7;
    double x = 150.0 * px;
    double y = 150.0 * py;
    rsuPositionAlloc->Add(Vector(x, y, 0.0));
  }
  rsuMobility.SetPositionAllocator(rsuPositionAlloc);
  rsuMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  rsuMobility.Install(rsu);

  //Step 4: Communicate UDP
  Ptr<Socket> sender = Socket::CreateSocket(cars.Get(0), UdpSocketFactory::GetTypeId());
  InetSocketAddress remote = InetSocketAddress(Ipv4Address("255.255.255.255"), 80);
  sender->SetAllowBroadcast(true);
  sender->Connect(remote);

  for (uint32_t i = 0; i < carNum; ++i) {
    if (i == 0) continue;
    Ptr<Socket> recvSocket = Socket::CreateSocket(cars.Get(i), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
    recvSocket->Bind(local);
    recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));
    NS_LOG_UNCOND("Car " << i << " ready to receive");
  }

  for (uint32_t i = 0; i < rsuNum; ++i) {
    Ptr<Socket> recvSocket = Socket::CreateSocket(rsu.Get(i), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
    recvSocket->Bind(local);
    recvSocket->SetRecvCallback(MakeCallback(ReceivePacket));
    NS_LOG_UNCOND("RSU " << i << " ready to receive");
  }

  // Debug: print position and distance before scheduling
  Simulator::Schedule(Seconds(49.0), [&]() {
    auto accidentMob = cars.Get(0)->GetObject<MobilityModel>();
    Vector accidentPos = accidentMob->GetPosition();
    NS_LOG_UNCOND("----Distance from Car 0----");
    for (uint32_t i = 0; i < cars.GetN(); ++i) {
      auto m = cars.Get(i)->GetObject<MobilityModel>();
      Vector p = m->GetPosition();
      double dx = p.x - accidentPos.x;
      double dy = p.y - accidentPos.y;
      double dist = std::sqrt(dx*dx + dy*dy);
      NS_LOG_UNCOND("Car " << i << " distance = " << dist << " m");
    }
  });

  //Step 5: Car 0 send alert after 50s
  Simulator::Schedule(Seconds(50.0), [&]() {
    std::string msg = "Car 0 has accident. HELP, PLEASE";
    Ptr<Packet> packet = Create<Packet>((uint8_t *)msg.c_str(), msg.length()); 
    sender->Send(packet);
    NS_LOG_INFO("Car 0 send accident alert at " << Simulator::Now().GetSeconds() << "s");
  });

  // Simulate with NetAnim
  AnimationInterface anim("/home/mojinn/netanim/project/demo_accident_dut.xml");
  anim.UpdateNodeColor(0, 255, 0, 0); // accident car: red
  for (uint32_t i = 1; i < cars.GetN(); ++i) {
    anim.UpdateNodeColor(i, 0, 255, 0); // other cars: green
  }
  for (uint32_t i = 0; i < rsu.GetN(); ++i) {
    anim.UpdateNodeColor(carNum + i, 255, 255, 0); // RSU nodes have yellow
  }
  anim.SetMaxPktsPerTraceFile(999999);

  // write pcap file to view in wireshark
  phy.EnablePcapAll("/home/mojinn/wireshark/vanet_trace");

  Simulator::Stop(Seconds(duration));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

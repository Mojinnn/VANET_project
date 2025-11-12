#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ns2-mobility-helper.h"

// AODV & routing helper
#include "ns3/aodv-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANET_Accident_Alert");

static const uint16_t APP_PORT = 80;

// Callback function for received packet
void ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
  {
    InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
    Ipv4Address senderIp = address.GetIpv4();
    uint32_t packetSize = packet->GetSize();

    std::cout << "RSU received alert from " << senderIp << " (" << packetSize << " bytes)" << std::endl;

    // Create ACK message;
    std::ostringstream msg;
    msg << "RSU received alert from " << senderIp << " (" << packetSize << " bytes)" << std::endl;
    Ptr<Packet> ackPacket = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());

    // Send ACK back to sender;
    socket->SendTo(ackPacket, 0, InetSocketAddress(senderIp, APP_PORT));
    std::cout << "RSU sent acknowledgement to " << senderIp << std::endl;
  }
}

void carReceivePacket(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
        std::cout << "Car received ACK from " << address.GetIpv4() << " at " << Simulator::Now().GetSeconds() << "s" << std::endl;
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
  mac.SetType("ns3::AdhocWifiMac");
  NetDeviceContainer carDevices = wifi.Install(phy, mac, cars);
  NetDeviceContainer rsuDevices = wifi.Install(phy, mac, rsu);

  //Step 2: Internet stack with AODV
  AodvHelper aodv;
  Ipv4ListRoutingHelper list;
  list.Add(aodv, 100);

  InternetStackHelper internet;
  internet.SetRoutingHelper(list);
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
  for (uint32_t i = 0; i < carNum; ++i) {
    // if (i == 0) continue;
    Ptr<Socket> recvSocket = Socket::CreateSocket(cars.Get(i), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), APP_PORT);
    recvSocket->Bind(local);
    if (i == 0) {
      recvSocket->SetRecvCallback(MakeCallback(&carReceivePacket));
    }
    else {
      recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));
    }
    // recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));
    NS_LOG_UNCOND("Car " << i << " ready to receive");
  }

  for (uint32_t i = 0; i < rsuNum; ++i) {
    Ptr<Socket> recvSocket = Socket::CreateSocket(rsu.Get(i), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), APP_PORT);
    recvSocket->Bind(local);
      recvSocket->SetRecvCallback(MakeCallback(&ReceivePacket));
    NS_LOG_UNCOND("RSU " << i << " ready to receive");
  }

  
  // --- Sender on Car 0: send unicast to processing center (choose RSU index 0 => global node id = carNum) ---
  // Determine processing center IP: that's rsuInterfaces.GetAddress(0)
  Ipv4Address processingCenterIp = rsuInterfaces.GetAddress(0);
  NS_LOG_UNCOND("Processing center IP: " << processingCenterIp);

  Ptr<Socket> sender = Socket::CreateSocket(cars.Get(0), UdpSocketFactory::GetTypeId());
  sender->SetAllowBroadcast(false);
  // connect to processing center (unicast)
  sender->Connect(InetSocketAddress(processingCenterIp, APP_PORT));

  // Step: send accident alert from car 0 at 30s (AODV will route)
  Simulator::Schedule(Seconds(30.0), [&]() {
    std::string msg = "Car 0 has accident. HELP, PLEASE";
    Ptr<Packet> packet = Create<Packet>((uint8_t *)msg.c_str(), msg.length());
    sender->Send(packet);
    NS_LOG_INFO("Car 0 send accident alert at " << Simulator::Now().GetSeconds() << "s to " << processingCenterIp);
  });

  // NetAnim
  AnimationInterface anim("/home/mojinn/netanim/project/demo_accident_aodv.xml");
  anim.SetMaxPktsPerTraceFile(999999);
  anim.UpdateNodeColor(0, 255, 0, 0); // accident car red
  for (uint32_t i = 1; i < cars.GetN(); ++i) anim.UpdateNodeColor(i, 0, 255, 0);
  for (uint32_t i = 0; i < rsu.GetN(); ++i) anim.UpdateNodeColor(carNum + i, 255, 255, 0);

  // pcap (optional)
  phy.EnablePcapAll("/home/mojinn/wireshark/vanet_aodv");

  Simulator::Stop(Seconds(duration));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

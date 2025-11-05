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
      // NS_LOG_UNCOND(nodeType << " " << id << " triggered ReceivePacket() at " << Simulator::Now().GetSeconds() << "s, distance = " << distance << " m");
      std::string data;
      data.resize(packet->GetSize());
      packet->CopyData((uint8_t *)data.data(), packet->GetSize());
      if (distance <= 150.0)
      {
        NS_LOG_INFO(nodeType << " " << id << " received alert: " << data <<" (distance = " << distance << " m)");
      }

      // RSU rebroadcast
      if (nodeType == "RSU") {
        Simulator::Schedule(Seconds(1.0), [=]() {
          Ptr<Socket> rsuSender = Socket::CreateSocket(node, UdpSocketFactory::GetTypeId());
          rsuSender->SetAllowBroadcast(true);
          InetSocketAddress remote = InetSocketAddress(Ipv4Address("255.255.255.255"), 80);
          rsuSender->Connect(remote);
          Ptr<Packet> newPacket = Create<Packet>((uint8_t*)data.c_str(), data.length());
          rsuSender->Send(newPacket);
          NS_LOG_UNCOND("RSU " << id << " rebroadcast alert at " << Simulator::Now().GetSeconds() << "s");
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
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

    // Debug: Increase Tx power------
    phy.Set("TxPowerStart", DoubleValue(30.0)); // dBM
    phy.Set("TxPowerEnd", DoubleValue(30.0));
    phy.Set("RxGain", DoubleValue(0));
    phy.Set("RxNoiseFigure", DoubleValue(7));
    channel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(300.0));
    //--------------------------
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

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
    Simulator::Schedule(Seconds(29.0), [&]() {
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

    //Step 5: Car 0 send alert after 30s
    Simulator::Schedule(Seconds(30.0), [&]() {
      std::string msg = "Car 0 has accident. HELP, PLEASE";
      Ptr<Packet> packet = Create<Packet>((uint8_t *)msg.c_str(), msg.length());
      sender->Send(packet);
      NS_LOG_INFO("Car 0 send accident alert at 30s");

      // Stop the car imediately
      // --- Freeze the car permanently ---
      // Ptr<Node> car0 = cars.Get(0);
      // Ptr<MobilityModel> oldMob = car0->GetObject<MobilityModel>();
      // Vector pos = oldMob->GetPosition();

      // // Remove old (dynamic) mobility model
      // car0->AggregateObject(nullptr);

      // // Install a ConstantPositionMobilityModel to freeze it
      // Ptr<ConstantPositionMobilityModel> stopMob = CreateObject<ConstantPositionMobilityModel>();
      // car0->AggregateObject(stopMob);
      // stopMob->SetPosition(pos);

      // NS_LOG_UNCOND("Car 0 permanently stopped at (" << pos.x << ", " << pos.y << ")");
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

    Simulator::Stop(Seconds(duration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EquipeX_Simulacao");

void RunSimulation(uint32_t nClients,
                   bool mobility,
                   int protocol)
{
    NodeContainer serverNode, apNode, clientNodes;
    serverNode.Create(1);
    apNode.Create(1);
    clientNodes.Create(nClients);

    // Conexão cabeada entre Servidor e AP
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer p2pDevices = p2p.Install(serverNode.Get(0), apNode.Get(0));

    // Configuração do Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    WifiMacHelper mac;
    // Substitua esta linha:
    // YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    // Por isso:
    YansWifiPhyHelper phy;
    phy.Set("RxGain", DoubleValue(0));  // Exemplo de configuração, você pode ajustar conforme necessário
    phy.Set("TxGain", DoubleValue(0));  // Defina outras propriedades, se necessário

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    Ssid ssid = Ssid("EquipeX");

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer clientDevices = wifi.Install(phy, mac, clientNodes);

    // Configuração da mobilidade
    MobilityHelper mobilityHelper;
    if (mobility)
    {
        mobilityHelper.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                            "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                            "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
        mobilityHelper.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                        "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    }
    else
    {
        mobilityHelper.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    }
    mobilityHelper.Install(clientNodes);
    mobilityHelper.Install(apNode);
    mobilityHelper.Install(serverNode);

    // Pilha TCP/IP
    InternetStackHelper stack;
    stack.Install(serverNode);
    stack.Install(apNode);
    stack.Install(clientNodes);

    // Endereçamento IP
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer serverApIp = address.Assign(p2pDevices);

    address.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer clientIp = address.Assign(clientDevices);
    Ipv4InterfaceContainer apIp = address.Assign(apDevice);

    // Configuração das aplicações
    uint16_t port = 9;
    ApplicationContainer serverApp, clientApp;

    if (protocol == 0)    // UDP
    {
        UdpEchoServerHelper echoServer(port);
        serverApp = echoServer.Install(serverNode.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));

        UdpEchoClientHelper echoClient(serverApIp.GetAddress(0),
                                       port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        for (uint32_t i = 0; i < nClients; ++i)
        {
            clientApp.Add(echoClient.Install(clientNodes.Get(i)));
        }
    }
    else if (protocol == 1)      // TCP
    {
        BulkSendHelper bulkSend("ns3::TcpSocketFactory",
                                Address(InetSocketAddress(serverApIp.GetAddress(0), port)));
        bulkSend.SetAttribute("MaxBytes", UintegerValue(0));

        for (uint32_t i = 0; i < nClients; ++i)
        {
            clientApp.Add(bulkSend.Install(clientNodes.Get(i)));
        }

        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
        serverApp = sink.Install(serverNode.Get(0));
    }
    else      // TCP + UDP
    {
        for (uint32_t i = 0; i < nClients; ++i)
        {
            if (i % 2 == 0)    // UDP
            {
                UdpEchoClientHelper echoClient(serverApIp.GetAddress(0),
                                               port);
                echoClient.SetAttribute("MaxPackets", UintegerValue(10));
                echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
                echoClient.SetAttribute("PacketSize", UintegerValue(1024));
                clientApp.Add(echoClient.Install(clientNodes.Get(i)));
            }
            else      // TCP
            {
                BulkSendHelper bulkSend("ns3::TcpSocketFactory",
                                        Address(InetSocketAddress(serverApIp.GetAddress(0), port)));
                bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
                clientApp.Add(bulkSend.Install(clientNodes.Get(i)));
            }
        }

        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
        serverApp = sink.Install(serverNode.Get(0));
    }

    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Coleta de estatísticas
    FlowMonitorHelper flowMonitor;
    Ptr < FlowMonitor > monitor = flowMonitor.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Relatório de desempenho
    monitor->SerializeToXmlFile("resultados.xml", true, true);

    Simulator::Destroy();
}

int main(int argc,
         char *argv[])
{
    LogComponentEnable("EquipeX_Simulacao", LOG_LEVEL_INFO);

    uint32_t clientCounts[] = { 1, 2, 4, 8, 16, 32 };
    bool mobilityOptions[] = { false, true };
    int protocolOptions[] = { 0, 1, 2 };  // 0 = UDP, 1 = TCP, 2 = TCP+UDP

    for (bool mobility : mobilityOptions)
    {
        for (int protocol : protocolOptions)
        {
            for (uint32_t nClients : clientCounts)
            {
                NS_LOG_INFO("Simulando " << nClients << " clientes, Mobilidade: " << mobility
                                         << ", Protocolo: " << (protocol == 0 ? "UDP" : protocol == 1 ? "TCP" : "TCP+UDP"));
                RunSimulation(nClients, mobility, protocol);
            }
        }
    }

    return 0;
}

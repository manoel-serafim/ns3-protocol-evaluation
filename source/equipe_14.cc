#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Equipe14_Simulacao");

enum 
{
    UDP_PROTO = 0,
    TCP_PROTO = 1,
    UDP_TCP_PROTO = 2
};


void RunSimulation(uint32_t nClients,
                   bool mobility,
                   int protocol)
{
    //pointers to node containers
    NodeContainer server_node, ap_node, client_nodes;
    server_node.Create(1UL);
    ap_node.Create(1UL);
    client_nodes.Create(nClients);

    // cable connection between server and AP
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer p2p_devices = p2p.Install(server_node.Get(0), ap_node.Get(0));

    // AP wifi adapter config
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    WifiMacHelper mac;
    
    YansWifiPhyHelper phy;
    phy.Set("RxGain", DoubleValue(0));  
    phy.Set("TxGain", DoubleValue(0)); 

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    Ssid ssid = Ssid("Equipe14");

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer ap_device = wifi.Install(phy, mac, ap_node);

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer client_devices = wifi.Install(phy, mac, client_nodes);

    // mobility configuration
    MobilityHelper mobility_helper;
    if (mobility)
    {
        mobility_helper.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                            "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"),
                                            "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
        mobility_helper.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                        "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    }
    else
    {
        mobility_helper.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    }
    mobility_helper.Install(client_nodes);
    mobility_helper.Install(ap_node);
    mobility_helper.Install(server_node);

    // Pilha TCP/IP
    InternetStackHelper stack;
    stack.Install(server_node);
    stack.Install(ap_node);
    stack.Install(client_nodes);

    // Endereçamento IP
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer server_ap_ip = address.Assign(p2p_devices);

    address.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer clientIp = address.Assign(client_devices);
    Ipv4InterfaceContainer ap_ip = address.Assign(ap_device);

    // Configuração das aplicações
    uint16_t port = 9;
    ApplicationContainer server_app, client_app;

    if (protocol == UDP_PROTO)    // UDP
    {
        UdpEchoServerHelper echo_server(port);
        server_app = echo_server.Install(server_node.Get(0));
        server_app.Start(Seconds(1.0));
        server_app.Stop(Seconds(10.0));

        UdpEchoClientHelper echo_client(server_ap_ip.GetAddress(0),
                                       port);
        echo_client.SetAttribute("MaxPackets", UintegerValue(10));
        echo_client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echo_client.SetAttribute("PacketSize", UintegerValue(1024));

        for (uint32_t i = 0; i < nClients; ++i)
        {
            client_app.Add(echo_client.Install(client_nodes.Get(i)));
        }
    }
    else if (protocol == TCP_PROTO)      // TCP
    {
        BulkSendHelper bulkSend("ns3::TcpSocketFactory",
                                Address(InetSocketAddress(server_ap_ip.GetAddress(0), port)));
        bulkSend.SetAttribute("MaxBytes", UintegerValue(0));

        for (uint32_t i = 0; i < nClients; ++i)
        {
            client_app.Add(bulkSend.Install(client_nodes.Get(i)));
        }

        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
        server_app = sink.Install(server_node.Get(0));
    }
    else if (protocol == UDP_TCP_PROTO) 
    {
        for (uint32_t i = 0; i < nClients; ++i)
        {
            if (i % 2 == 0)    // UDP
            {
                UdpEchoClientHelper echo_client(server_ap_ip.GetAddress(0),
                                               port);
                echo_client.SetAttribute("MaxPackets", UintegerValue(10));
                echo_client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
                echo_client.SetAttribute("PacketSize", UintegerValue(1024));
                client_app.Add(echo_client.Install(client_nodes.Get(i)));
            }
            else      // TCP
            {
                BulkSendHelper bulkSend("ns3::TcpSocketFactory",
                                        Address(InetSocketAddress(server_ap_ip.GetAddress(0), port)));
                bulkSend.SetAttribute("MaxBytes", UintegerValue(0));
                client_app.Add(bulkSend.Install(client_nodes.Get(i)));
            }
        }

        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
        server_app = sink.Install(server_node.Get(0));
    }

    client_app.Start(Seconds(2.0));
    client_app.Stop(Seconds(10.0));
    server_app.Start(Seconds(1.0));
    server_app.Stop(Seconds(10.0));

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
    LogComponentEnable("Equipe14_Simulacao", LOG_LEVEL_ALL);

    uint32_t clientCounts[] = { 1, 2, 4, 8, 16, 32 };
    bool mobilityOptions[] = { false, true };
    int protocolOptions[] = { UDP_PROTO, TCP_PROTO, UDP_TCP_PROTO };  

    for (bool mobility : mobilityOptions)
    {
        for (int protocol : protocolOptions)
        {
            for (uint32_t nClients : clientCounts)
            {
                NS_LOG_INFO("Simulando " << nClients << " clientes, Mobilidade: " << mobility
                                         << ", Protocolo: " << (protocol == UDP_PROTO ? "UDP" : protocol == TCP_PROTO ? "TCP" : "TCP+UDP"));
                RunSimulation(nClients, mobility, protocol);
            }
        }
    }

    return 0;
}


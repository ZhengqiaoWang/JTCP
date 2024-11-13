#include "JTCP/JTCP.h"
#include <iostream>
#include <thread>

using namespace JTCP;

void onNewClientConnect(Server::TCPPeerClientPtr);

int main(int argc, char const* argv[])
{
    // 解析参数，获取监听的IP和端口
    Types::IPStrType listen_ip{argv[1]};
    Types::PortType  listen_port{atoi(argv[2])};

    // 实例化TCPServer
    Server::TCPServer server;

    // 设置新客户端连接时的回调
    server.setOnNewClient(onNewClientConnect);

    // 启动服务
    auto ret = server.start(listen_ip, listen_port);
    if (ret.isFailure()) {
        perror(ret.getFailurePtr()->c_str());
        return -1;
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "server is running" << std::endl;
    }

    return 0;
}

void onNewClientConnect(Server::TCPPeerClientPtr client)
{
    std::cout << "new client connected" << std::endl;
    while (1) {
        // 缓冲区大小和接受的数据大小
        std::size_t buff_size = 2048, recv_data_size{0};
        char        buff[buff_size]{0};

        // 接受数据
        if (auto ret = client->readData(buff, buff_size); ret.isFailure()) {
            std::cout << "client disconnect" << std::endl;
            return;
        }
        else {
            // 实际读取到的数据长度
            recv_data_size = *(ret.getSuccessPtr());
        }
        std::cout << "recv data(" << recv_data_size << "): " << buff << std::endl;

        // 将收到的消息返回
        if (auto ret = client->sendData(buff, recv_data_size); ret.isFailure()) {
            perror(ret.getFailurePtr()->c_str());
            return;
        }
    }
}
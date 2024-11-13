#include "JTCP/JTCP.h"
#include <iostream>
#include <thread>

using namespace JTCP;

int main(int argc, char const* argv[])
{
    // 解析参数，获取连接的IP和端口
    Types::IPStrType server_ip{argv[1]};
    Types::PortType  server_port{atoi(argv[2])};

    // 实例化TCPClient
    auto client_ret = Client::TCPClient::createNew(server_ip, server_port);
    if (client_ret.isFailure()) {
        perror(client_ret.getFailurePtr()->c_str());
        return -1;
    }

    auto client = client_ret.getSuccessPtr()->get();
    if (auto ret = client->sendData("hello world", 11); ret.isFailure()) {
        perror(ret.getFailurePtr()->c_str());
    }
    std::size_t buff_size = 2048;
    char        buff[buff_size]{0};
    if (auto ret = client->recvData(buff, buff_size); ret.isFailure()) {
        perror(ret.getFailurePtr()->c_str());
    }
    else {
        std::cout << "recv data: " << buff << std::endl;
    }

    return 0;
}

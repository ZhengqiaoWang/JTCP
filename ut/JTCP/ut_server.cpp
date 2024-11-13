#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "JTCP/JTCP.h"
#include "doctest.h"
#include <atomic>
#include <chrono>
#include <thread>

TEST_CASE("server")
{
    using namespace JTCP;

    std::atomic_int client_num = 0;

    Server::TCPServer server;
    server.setOnNewClient([&](Server::TCPPeerClientPtr client) {
        client_num++;
        std::cout << "client connect: " << client->getPeerIP() << ":" << client->getPeerPort()
                  << std::endl;
        client->setOnClientExitCB([&client_num](Server::TCPPeerClient* ptr) {
            std::cout << "client disconnect fd: " << ptr->getFileDescribe()->getFD() << std::endl;
            client_num--;
        });
        client->setOnRecvDataCB(
            [&client_num](Server::TCPPeerClient* ptr, const char* data, size_t len) {
                if (len == 0) {
                    return;
                }

                auto recv_num = atoi(data);

                std::string ret_num = std::to_string(recv_num + 1);

                if (auto ret = ptr->sendData(ret_num.c_str(), ret_num.size()); ret.isFailure()) {
                    std::cout << "send data failed: " << *(ret.getFailurePtr()) << std::endl;
                }

                std::cout << "client send to server: " << ret_num << std::endl;
            });
    });

    server.start("0.0.0.0", 9998);

    for (int i = 0; i < 5; ++i) {
        if (auto ret = Client::TCPClient::createNew("127.0.0.1", 9998); ret.isFailure()) {
            std::cout << "create client failed" << std::endl;
        }
        else {
            auto client = ret.getSuccessPtr()->get();
            if (auto ret2 = client->sendData("1", 1); ret2.isFailure()) {
                std::cout << "send data failed " << *(ret2.getFailurePtr()) << std::endl;
            }
        }
        std::cout << "client num: " << client_num << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server.stop();
}

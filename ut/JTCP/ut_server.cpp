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

        client->setOnRecvDataCB([](Server::TCPPeerClient* ptr) {
            while (1) {
                std::size_t buff_size = 2048;
                char        buff[buff_size]{0};
                auto        ret = ptr->readData(buff, buff_size);
                if (ret.isFailure()) {
                    std::cout << std::endl << "client disconnect" << std::endl;
                    return;
                }

                std::cout << "recv data(" << *(ret.getSuccessPtr()) << "): " << buff;

                std::string html{"<!DOCTYPE html><html> <head><title>test</title></head><body> "
                                 "<h1> TEST </h1> </body> </html>"};
                ptr->sendData(html.c_str(), html.size());
            }
        });

        client->setOnDisconnectCB([](Server::TCPPeerClient* ptr) {
            std::cout << "client disconnect: " << ptr->getPeerIP() << ":" << ptr->getPeerPort()
                      << std::endl;
        });
    });

    server.start("0.0.0.0", 9998);

    for (int i = 0; i < 5; ++i) {
        if (auto ret = Client::TCPClient::createNew("127.0.0.1", 9998); ret.isFailure()) {
            std::cout << "create client failed: " << *(ret.getFailurePtr()) << std::endl;
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

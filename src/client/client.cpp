#include "JTCP/client/client.h"


namespace JTCP::Client {

JResultWithSuccErrMsg<std::shared_ptr<TCPClient>> TCPClient::createNew(
    const Types::IPStrType& server_ip, const Types::PortType& server_port)
{
    auto client  = std::make_shared<TCPClient>();
    client->m_fd = std::make_shared<FileDescribe>(socket(AF_INET, SOCK_STREAM, 0));
    if (client->m_fd->isInvalid()) {
        return JResultWithSuccErrMsg<std::shared_ptr<TCPClient>>::failure(
            "failed to create socket");
    }

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(server_port);
    addr.sin_addr.s_addr = inet_addr(server_ip.c_str());
    if (connect(client->m_fd->getFD(), (const struct sockaddr*)&addr, sizeof(struct sockaddr_in)) <
        0) {
        return JResultWithSuccErrMsg<std::shared_ptr<TCPClient>>::failure(
            "failed to connect to server");
    }

    // connect done
    return JResultWithSuccErrMsg<std::shared_ptr<TCPClient>>::success(std::move(client));
}

JResultWithErrMsg TCPClient::sendData(const char* data, size_t len)
{
    if (m_fd->isInvalid()) {
        return JResultWithErrMsg::failure("invalid file descriptor");
    }

    if (send(m_fd->getFD(), data, len, 0) < 0) {
        return JResultWithErrMsg::failure("failed to send data");
    }

    return JResultWithErrMsg::success();
}

JResultWithErrMsg TCPClient::recvData(char* data, size_t& len)
{
    if (m_fd->isInvalid()) {
        return JResultWithErrMsg::failure("invalid file descriptor");
    }

    if (recv(m_fd->getFD(), data, len, 0) < 0) {
        return JResultWithErrMsg::failure("failed to recv data");
    }

    return JResultWithErrMsg::success();
}

}   // namespace JTCP::Client

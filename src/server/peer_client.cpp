#include "JTCP/server/peer_client.h"

namespace JTCP::Server {

struct sockaddr_in* TCPPeerClient::getSockAddr()
{
    return &m_sock_addr;
}

Types::IPStrVType TCPPeerClient::getPeerIP() const
{
    return inet_ntoa(m_sock_addr.sin_addr);
}
const Types::PortType TCPPeerClient::getPeerPort() const
{
    return ntohs(m_sock_addr.sin_port);
}

void TCPPeerClient::setFileDescribe(FileDescribePtr fd) noexcept
{
    m_fd = fd;
}
FileDescribePtr TCPPeerClient::getFileDescribe() const noexcept
{
    return m_fd;
}

void TCPPeerClient::setOnRecvDataCB(OnRecvDataCBType cb)
{
    m_on_recv_data_cb = cb;
}
void TCPPeerClient::onRecvData()
{
    m_on_recv_data_cb(this);
}

JResultWithSuccErrMsg<std::size_t> TCPPeerClient::sendData(const char* data, size_t len)
{
    auto sended_length = send(m_fd->getFD(), data, len, 0);
    if (sended_length < 0) {
        return JResultWithSuccErrMsg<std::size_t>::failure("failed to send data");
    }

    return JResultWithSuccErrMsg<std::size_t>::success(sended_length);
}

JResultWithSuccErrMsg<std::size_t> TCPPeerClient::readData(char* data, const size_t&  expect_len)
{
    ssize_t ret = read(m_fd->getFD(), data, expect_len);
    // 当返回值异常或退出时，都通知删除该客户端
    if (-1 == ret) {
        m_server->delClient(m_fd->getFD());
        return JResultWithSuccErrMsg<std::size_t>::failure("read failed");
    }
    else if (0 == ret) {
        return JResultWithSuccErrMsg<std::size_t>::failure(
            m_server->delClient(m_fd->getFD()).getFailurePtr());
    }

    return JResultWithSuccErrMsg<std::size_t>::success(ret);
}
}   // namespace JTCP::Server
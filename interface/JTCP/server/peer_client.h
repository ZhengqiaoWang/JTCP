#pragma once

#include "JTCP/common/common_define.h"
#include "JTCP/common/file_describe.h"
#include <functional>

namespace JTCP::Server {
/**
 * @brief 对手方客户端管理器
 *
 */
class TCPPeerClient
{
public:
    using OnClientExitCBType = std::function<void(TCPPeerClient*)>;
    using OnRecvDataCBType   = std::function<void(TCPPeerClient*, const char*, size_t)>;

    struct sockaddr_in* getSockAddr() { return &m_sock_addr; }

    Types::IPStrVType     getPeerIP() const { return inet_ntoa(m_sock_addr.sin_addr); }
    const Types::PortType getPeerPort() const { return ntohs(m_sock_addr.sin_port); }

    void            setFileDescribe(FileDescribePtr fd) noexcept { m_fd = fd; }
    FileDescribePtr getFileDescribe() const noexcept { return m_fd; }

    void setOnClientExitCB(OnClientExitCBType cb) { m_on_client_exit_cb = cb; }
    void onClientExit() { m_on_client_exit_cb(this); }

    void setOnRecvDataCB(OnRecvDataCBType cb) { m_on_recv_data_cb = cb; }
    void onRecvData(const char* data, size_t len) { m_on_recv_data_cb(this, data, len); }

    JResultWithSuccErrMsg<std::size_t> sendData(const char* data, size_t len)
    {
        auto sended_length = send(m_fd->getFD(), data, len, 0);
        if (sended_length < 0) {
            return JResultWithSuccErrMsg<std::size_t>::failure("failed to send data");
        }

        return JResultWithSuccErrMsg<std::size_t>::success(sended_length);
    }

private:
    FileDescribePtr    m_fd{nullptr};
    struct sockaddr_in m_sock_addr;

    OnClientExitCBType m_on_client_exit_cb{[](TCPPeerClient*) {}};
    OnRecvDataCBType   m_on_recv_data_cb{[](TCPPeerClient*, const char*, size_t) {}};
};
using TCPPeerClientPtr = std::shared_ptr<TCPPeerClient>;
}   // namespace JTCP::Server

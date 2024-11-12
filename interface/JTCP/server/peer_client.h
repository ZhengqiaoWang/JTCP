#ifndef _JTCP_SERVER_PEER_CLIENT_H_
#define _JTCP_SERVER_PEER_CLIENT_H_

#include "JTCP/common/common_define.h"
#include "JTCP/common/file_describe.h"
#include <functional>

namespace JTCP::Server
{
    /**
     * @brief 对手方客户端管理器
     * 
     */
    class TCPPeerClient
    {
    public:
        using OnClientExitCBType = std::function<void()>;
        using OnRecvDataCBType = std::function<void(const char *, size_t)>;

        struct sockaddr_in *getSockAddr() { return &m_sock_addr; }

        Types::IPStrVType getPeerIP() const { return inet_ntoa(m_sock_addr.sin_addr); }
        const Types::PortType getPeerPort() const { return ntohs(m_sock_addr.sin_port); }

        void setFileDescribe(FileDescribePtr fd) noexcept
        {
            m_fd = fd;
        }
        FileDescribePtr getFileDescribe() const noexcept
        {
            return m_fd;
        }

        void setOnClientExitCB(OnClientExitCBType cb)
        {
            m_on_client_exit_cb = cb;
        }
        void onClientExit()
        {
            m_on_client_exit_cb();
        }

        void setOnRecvDataCB(OnRecvDataCBType cb)
        {
            m_on_recv_data_cb = cb;
        }
        void onRecvData(const char *data, size_t len)
        {
            m_on_recv_data_cb(data, len);
        }

    private:
        FileDescribePtr m_fd{nullptr};
        struct sockaddr_in m_sock_addr;

        OnClientExitCBType m_on_client_exit_cb{[]() {}};
        OnRecvDataCBType m_on_recv_data_cb{[](const char *, size_t) {}};
    };
    using TCPPeerClientPtr = std::shared_ptr<TCPPeerClient>;
}

#endif
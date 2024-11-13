#pragma once

#include "JResult/JResult.h"
#include "JTCP/common/common_define.h"
#include "JTCP/common/file_describe.h"
#include "JTCP/server/server.h"
#include <functional>

namespace JTCP::Server {

class TCPServer;

/**
 * @brief 对手方客户端管理器
 *
 */
class TCPPeerClient
{
public:
    TCPPeerClient(TCPServer* server)
        : m_server(server)
    {}

    using OnRecvDataCBType = std::function<void(TCPPeerClient*)>;

public:
    struct sockaddr_in* getSockAddr();


    Types::IPStrVType     getPeerIP() const;
    const Types::PortType getPeerPort() const;

    void            setFileDescribe(FileDescribePtr fd) noexcept;
    FileDescribePtr getFileDescribe() const noexcept;

    void setOnRecvDataCB(OnRecvDataCBType cb);
    void onRecvData();

    JResultWithSuccErrMsg<std::size_t> sendData(const char* data, size_t len);
    JResultWithSuccErrMsg<std::size_t> readData(char* data, const size_t& expect_len);

private:
    TCPServer*         m_server{nullptr};
    FileDescribePtr    m_fd{nullptr};
    struct sockaddr_in m_sock_addr;
    OnRecvDataCBType   m_on_recv_data_cb{[](TCPPeerClient*) {}};
};

using TCPPeerClientPtr = std::shared_ptr<TCPPeerClient>;

}   // namespace JTCP::Server

#pragma once

#include "JResult/JResult.h"
#include "JTCP/common/common_define.h"
#include "JTCP/common/file_describe.h"

namespace JTCP::Client {
class TCPClient
{
public:
    TCPClient() {}
    TCPClient(const TCPClient&) = delete;
    TCPClient(TCPClient&&)      = delete;

public:
    static JResultWithSuccErrMsg<std::shared_ptr<TCPClient>> createNew(
        const Types::IPStrType& server_ip, const Types::PortType& server_port);

    JResultWithErrMsg sendData(const char* data, size_t len);
    JResultWithErrMsg recvData(char* data, size_t& len);

private:
    FileDescribePtr m_fd{nullptr};
};
}   // namespace JTCP::Client

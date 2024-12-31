/**
 * @author Wangzhengqiao (me@zhengqiao.wang)
 * @date 2024-11-28
 *
 */
#pragma once

#include "JResult/JResult.h"
#include "JTCP/common/common_define.h"
#include "JTCP/common/file_describe.h"

/**
 * @brief 客户端命名空间
 *
 */
namespace JTCP::Client {
/**
 * @brief TCP客户端类
 *
 */
class TCPClient
{
public:
    /**
     * @brief 默认构造函数
     *
     */
    TCPClient() {}
    TCPClient(const TCPClient&) = delete;
    TCPClient(TCPClient&&)      = delete;

public:
    /**
     * @brief 创建新的TCP客户端
     *
     * @param server_ip 服务器IP地址
     * @param server_port 服务器端口
     * @return JResultWithSuccErrMsg<std::shared_ptr<TCPClient>> 创建结果
     */
    static JResultWithSuccErrMsg<std::shared_ptr<TCPClient>> createNew(
        const Types::IPStrType& server_ip, const Types::PortType& server_port);

    /**
     * @brief 发送消息
     *
     * @param data 消息数据首地址
     * @param len 消息长度
     * @return JResultWithErrMsg 发送结果
     */
    JResultWithErrMsg sendData(const char* data, size_t len);

    /**
     * @brief 接收消息
     *
     * @param data 消息数据首地址
     * @param len 消息长度
     * @return JResultWithErrMsg 接收结果
     */
    JResultWithErrMsg recvData(char* data, size_t& len);

private:
    FileDescribePtr m_fd{nullptr};   ///< 文件描述符
};
}   // namespace JTCP::Client

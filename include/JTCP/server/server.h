/**
 * @author Wangzhengqiao (me@zhengqiao.wang)
 * @date 2024-11-28
 * 
 */
#pragma once

#include "JResult/JResult.h"
#include "JTCP/server/peer_client.h"
#include <fcntl.h>
#include <functional>
#include <future>
#include <unordered_map>
#include <mutex>

/**
 * @brief Server命名空间
 * 
 */
namespace JTCP::Server {

/**
 * @brief TCP对手方客户端对象类，见include/JTCP/server/peer_client.h文件
 * 
 */
class TCPPeerClient;
using TCPPeerClientPtr = std::shared_ptr<TCPPeerClient>;

/**
 * @brief TCP服务对象
 *
 */
class TCPServer
{
public:
    /**
     * @brief 新客户端连接回调类型
     * 
     */
    using OnNewClientCBType = std::function<void(TCPPeerClientPtr)>;
    /**
     * @brief 设置当有客户端链接时触发的回调
     *
     * @param cb 回调函数
     */
    void setOnNewClient(OnNewClientCBType cb) noexcept;

    using ListenMaxNumType = int32_t;
    /**
     * @brief 开始监听
     *
     * @param listen_addr 监听地址
     * @param listen_port 监听端口
     * @param listen_max_num 最大监听队列数量，超出的请求会被拒绝
     * @return JResultWithErrMsg 返回值
     */
    JResultWithErrMsg start(const Types::IPStrType& listen_addr, const Types::PortType& listen_port,
                            const ListenMaxNumType& listen_max_num = 20);
    /**
     * @brief 停止监听
     *
     * @return JResultWithErrMsg 返回值
     */
    JResultWithErrMsg stop();

private:
    /**
     * @brief epoll处理线程函数
     *
     * @param listen_max_num 最大监听数量
     * @return JResultWithErrMsg 返回值
     */
    JResultWithErrMsg acceptThreadFunc(const ListenMaxNumType& listen_max_num);

    /**
     * @brief 初始化epoll
     *
     * @return JResultWithErrMsg 返回值
     */
    JResultWithErrMsg initEpoll();

    using EpollEventType = uint32_t;
    using EpollOprType   = int32_t;
    JResultWithErrMsg epollOprEvent(EpollOprType opr, FileDescribe::FDType fd,
                                    EpollEventType epoll_events);

    JResultWithErrMsg handleNewClientConnect();
    JResultWithErrMsg handleClientMsg(epoll_event& event);

    friend class TCPPeerClient;
    JResultWithErrMsg delClient(const FileDescribe::FDType& fd);

    /**
     * 设置文件描述符的阻塞或非阻塞模式
     *
     * @param fd 文件描述符
     * @param is_block true表示阻塞模式，false表示非阻塞模式
     * @return 返回
     *
     * 此函数使用fcntl函数来获取和设置文件描述符的属性根据is_block参数，
     * 它将文件描述符设置为阻塞或非阻塞模式。
     */
    JResultWithErrMsg setFileDiscribeBlock(FileDescribe::FDType fd, bool is_block);

private:
    OnNewClientCBType              m_on_new_client_cb;            ///< 新客户端连接的回调
    FileDescribePtr                m_server_listen_fd{nullptr};   ///< 监听的文件描述符
    std::future<JResultWithErrMsg> m_accept_thread;               ///< 监听连接的线程

    using ClientMgrType = std::unordered_map<FileDescribe::FDType, TCPPeerClientPtr>;
    ClientMgrType m_client_mgr;         ///< 客户端管理
    std::mutex    m_client_mgr_mutex;   ///< 客户端管理锁

    using EpollFileDescribeType = FileDescribe::FDType;
    EpollFileDescribeType m_epollfd;   ///< epoll文件描述符

    bool m_run_flag{false};   ///< 运行标志
};
}   // namespace JTCP::Server

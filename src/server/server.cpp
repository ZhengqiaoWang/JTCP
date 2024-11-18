#include "JTCP/server/server.h"

namespace JTCP::Server {

void TCPServer::setOnNewClient(OnNewClientCBType cb) noexcept
{
    m_on_new_client_cb = cb;
}

JResultWithErrMsg TCPServer::start(const Types::IPStrType& listen_addr,
                                   const Types::PortType&  listen_port,
                                   const ListenMaxNumType& listen_max_num)
{
    m_server_listen_fd = std::make_shared<FileDescribe>(socket(AF_INET, SOCK_STREAM, 0));
    if (m_server_listen_fd->isInvalid()) {
        return JResultWithErrMsg::failure("create socket failed");
    }

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr(listen_addr.c_str());
    addr.sin_port        = htons(listen_port);

    if (bind(m_server_listen_fd->getFD(), (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        return JResultWithErrMsg::failure("bind socket failed");
    }
    if (listen(m_server_listen_fd->getFD(), listen_max_num) < 0) {
        return JResultWithErrMsg::failure("listen socket failed");
    }

    m_accept_thread = std::async(std::launch::async,
                                 std::bind(&TCPServer::acceptThreadFunc, this, listen_max_num));

    while (m_run_flag == false) {
        if (m_accept_thread.wait_for(std::chrono::milliseconds(100)) !=
            std::future_status::timeout) {
            return m_accept_thread.get();
        }
    }

    return JResultWithErrMsg::success();
}

JResultWithErrMsg TCPServer::stop()
{
    m_run_flag = false;
    return m_accept_thread.get();
}

JResultWithErrMsg TCPServer::acceptThreadFunc(const ListenMaxNumType& listen_max_num)
{
    if (auto ret = initEpoll(); ret.isFailure()) {
        return ret;
    }

    using ReadyNumType = int;
    ReadyNumType             ready_event_num{0};           ///< 触发的事件数量
    std::vector<epoll_event> event_list(listen_max_num);   ///< 缓存的event列表

    m_run_flag = true;
    while (m_run_flag) {
        ready_event_num = epoll_wait(m_epollfd,
                                     &*event_list.begin(),
                                     static_cast<int>(event_list.size()),
                                     1000);   // 超时1秒
        if (ready_event_num == -1) {
            return JResultWithErrMsg::failure("epoll_wait failed");
        }
        if (ready_event_num == 0)   // 超时，继续等
        {
            continue;
        }

        if ((size_t)ready_event_num == event_list.size())   // 对clients进行扩容
        {
            event_list.resize(event_list.size() * 2);
        }

        // 对每个事件进行处理
        for (ReadyNumType i = 0; i < ready_event_num; i++) {
            auto& event = event_list[i];
            if (event.data.fd == m_server_listen_fd->getFD()) {
                if (auto ret = handleNewClientConnect(); ret.isFailure()) {
                    return ret;
                }
            }
            else if (event.events & EPOLLIN) {
                if (event.data.fd < 0) {
                    continue;
                }
                if (auto ret = handleClientMsg(event); ret.isFailure()) {
                    return ret;
                }
            }
        }
    }

    // 服务停止，释放各个资源
    m_run_flag = false;
    {
        std::lock_guard<std::mutex> lock_guard(m_client_mgr_mutex);
        m_client_mgr.clear();
    }
    return JResultWithErrMsg::success();
}

JResultWithErrMsg TCPServer::initEpoll()
{
    // 创建一个 epoll 实例，并设置文件描述符为关闭执行时关闭
    m_epollfd = epoll_create1(EPOLL_CLOEXEC);
    return epollOprEvent(EPOLL_CTL_ADD, m_server_listen_fd->getFD(), EPOLLIN | EPOLLET);
}
JResultWithErrMsg TCPServer::epollOprEvent(EpollOprType opr, FileDescribe::FDType fd,
                                           EpollEventType epoll_events)
{
    struct epoll_event event;
    // 设置event为边缘触发模式，并关注读事件
    event.events = epoll_events;
    // 设置event的fd为监听的fd
    event.data.fd = fd;
    // 将监听的fd添加到epoll中
    if (epoll_ctl(m_epollfd, opr, fd, &event) < 0) {
        // 如果添加失败，返回错误信息
        return JResultWithErrMsg::failure(std::string("epoll_ctl when add event:") +
                                          std::to_string(epoll_events) +
                                          " for fd: " + std::to_string(fd) + " failed");
    }

    return JResultWithErrMsg::success();
}

JResultWithErrMsg TCPServer::handleNewClientConnect()
{
    // 新客户端连接
    auto peer_client = std::make_shared<TCPPeerClient>(this);

    socklen_t len  = sizeof(sockaddr_in);
    auto      conn = std::make_shared<FileDescribe>(
        accept(m_server_listen_fd->getFD(), (sockaddr*)peer_client->getSockAddr(), &len));
    if (conn->isInvalid()) {
        return JResultWithErrMsg::failure("accept failed");
    }

    peer_client->setFileDescribe(conn);

    // 设为非阻塞
    if (auto ret = setFileDiscribeBlock(conn->getFD(), false); ret.isFailure()) {
        return ret;
    }

    // 将该event设置为监听目标
    if (auto ret = epollOprEvent(EPOLL_CTL_ADD, conn->getFD(), EPOLLIN | EPOLLET);
        ret.isFailure()) {
        return ret;
    }

    {
        std::lock_guard<std::mutex> lock_guard(m_client_mgr_mutex);
        m_client_mgr[conn->getFD()] = peer_client;
    }
    m_on_new_client_cb(peer_client);

    return JResultWithErrMsg::success();
}

JResultWithErrMsg TCPServer::handleClientMsg(epoll_event& event)
{
    TCPPeerClientPtr peer_client{nullptr};
    {
        std::lock_guard<std::mutex> lock_guard(m_client_mgr_mutex);
        peer_client = m_client_mgr.at(event.data.fd);
    }
    if (nullptr == peer_client) {
        return JResultWithErrMsg::failure("peer client is nullptr");
    }

    // 通知客户端，让用户自己决定如何处理
    peer_client->onRecvData();

    return JResultWithErrMsg::success();
}

JResultWithErrMsg TCPServer::delClient(const FileDescribe::FDType& fd)
{
    if (auto ret = epollOprEvent(EPOLL_CTL_DEL, fd, 0); ret.isFailure()) {
        return ret;
    }
    TCPPeerClientPtr client{nullptr};
    {
        std::lock_guard<std::mutex> lock_guard(m_client_mgr_mutex);
        auto                        client_iter = m_client_mgr.find(fd);
        if (client_iter != m_client_mgr.end()) {
            client = client_iter->second;
        }
        m_client_mgr.erase(fd);
    }

    if (nullptr == client) {
        return JResultWithErrMsg::failure("peer client is nullptr");
    }

    client->onDisconnect();
    printf("delete client: %d\n", fd);

    return JResultWithErrMsg::success();
}

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
JResultWithErrMsg TCPServer::setFileDiscribeBlock(FileDescribe::FDType fd, bool is_block)
{
    // 获取当前文件描述符的属性
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        // 如果获取属性失败，直接返回成功，不做任何操作
        JResultWithErrMsg::success();
    }
    // 根据is_block参数设置文件描述符为阻塞或非阻塞模式
    if (is_block)   // 阻塞
    {
        flags &= ~O_NONBLOCK;
    }
    else   // 非阻塞
    {
        flags |= O_NONBLOCK;
    }

    // 尝试设置文件描述符的新属性
    if (fcntl(fd, F_SETFL, flags) < 0) {
        // 如果设置失败，返回一个表示失败的结果对象，并附带错误消息
        return JResultWithErrMsg::failure("fcntl failed");
    }
    // 如果设置成功，返回一个表示成功的对象
    return JResultWithErrMsg::success();
}

}   // namespace JTCP::Server

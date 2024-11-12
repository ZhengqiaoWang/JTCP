#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <future>
#include <memory>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "JResult/JResult.h"
/**
 * @brief 文件描述符RAII封装
 *
 */
class FileDescribe
{
public:
    using FDType = int;
    FileDescribe(const FDType &fd) : m_fd(fd) {}
    ~FileDescribe() { close(m_fd); }

    FileDescribe() = delete;
    FileDescribe(const FileDescribe &) = delete;
    FileDescribe(FileDescribe &&) = delete;

public:
    FDType getFD() const { return m_fd; }
    bool isValid() const { return m_fd >= 0; }
    bool isInvalid() const { return false == isValid(); }

private:
    FDType m_fd{0};
};
using FileDescribePtr = std::shared_ptr<FileDescribe>;

using PortType = uint16_t;
using IPStrType = std::string;
using IPStrVType = std::string_view;

class TCPPeerClient
{
public:
    using OnClientExitCBType = std::function<void()>;
    using OnRecvDataCBType = std::function<void(const char *, size_t)>;

    struct sockaddr_in *getSockAddr() { return &m_sock_addr; }

    IPStrVType getPeerIP() const { return inet_ntoa(m_sock_addr.sin_addr); }
    const PortType getPeerPort() const { return ntohs(m_sock_addr.sin_port); }

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
        printf("tcp client(%s,%d) disconnect\n", getPeerIP().data(), getPeerPort());
        m_on_client_exit_cb();
    }

    void setOnRecvDataCB(OnRecvDataCBType cb)
    {
        m_on_recv_data_cb = cb;
    }
    void onRecvData(const char *data, size_t len)
    {
        printf("recv(%s,%d) data: %s\n", getPeerIP().data(), getPeerPort(), data);
        m_on_recv_data_cb(data, len);
    }

private:
    FileDescribePtr m_fd{nullptr};
    struct sockaddr_in m_sock_addr;

    OnClientExitCBType m_on_client_exit_cb{[]() {}};
    OnRecvDataCBType m_on_recv_data_cb{[](const char *, size_t) {}};
};
using TCPPeerClientPtr = std::shared_ptr<TCPPeerClient>;

class TCPServer
{
public:
    // 设置当有客户端链接时触发的回调
    using OnNewClientCBType = std::function<void(TCPPeerClientPtr)>;
    void setOnNewClient(OnNewClientCBType cb) noexcept
    {
        m_on_new_client_cb = cb;
    }

    using ListenMaxNumType = int32_t;

    JResultWithErrMsg start(const IPStrType &listen_addr, const PortType &listen_port, const ListenMaxNumType &listen_max_num = 20)
    {
        m_server_listen_fd = std::make_shared<FileDescribe>(socket(AF_INET, SOCK_STREAM, 0));
        if (m_server_listen_fd->isInvalid())
        {
            return JResultWithErrMsg::failure("create socket failed");
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(listen_addr.c_str());
        addr.sin_port = htons(listen_port);

        if (bind(m_server_listen_fd->getFD(), (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            return JResultWithErrMsg::failure("bind socket failed");
        }
        if (listen(m_server_listen_fd->getFD(), listen_max_num) < 0)
        {
            return JResultWithErrMsg::failure("listen socket failed");
        }

        m_accept_thread = std::async(std::launch::async, std::bind(&TCPServer::acceptThreadFunc, this, listen_max_num));

        while (m_run_flag == false)
        {
            if (m_accept_thread.wait_for(std::chrono::milliseconds(100)) != std::future_status::timeout)
            {
                return m_accept_thread.get();
            }
        }

        return JResultWithErrMsg::success();
    }

private:
    JResultWithErrMsg acceptThreadFunc(const ListenMaxNumType &listen_max_num)
    {
        m_epollfd = epoll_create1(EPOLL_CLOEXEC);
        {
            // 设置监听，将监听的fd加入epoll中
            struct epoll_event event;
            event.events = EPOLLIN | EPOLLET;
            event.data.fd = m_server_listen_fd->getFD(); // 先设置event的fd为监听的fd
            if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, m_server_listen_fd->getFD(), &event) < 0)
            {
                return JResultWithErrMsg::failure("epoll_ctl failed");
            }
        }

        using ReadyNumType = int;
        ReadyNumType ready_event_num{0};                     ///< 触发的事件数量
        std::vector<epoll_event> event_list(listen_max_num); ///< 缓存的event列表

        m_run_flag = true;
        while (m_run_flag)
        {
            ready_event_num = epoll_wait(m_epollfd, &*event_list.begin(), static_cast<int>(event_list.size()), -1);
            if (ready_event_num == -1)
            {
                return JResultWithErrMsg::failure("epoll_wait failed");
            }
            if (ready_event_num == 0) // 肯定不会走到这里，因为上面没设置超时时间
            {
                continue;
            }

            if ((size_t)ready_event_num == event_list.size()) // 对clients进行扩容
            {
                event_list.resize(event_list.size() * 2);
            }

            for (ReadyNumType i = 0; i < ready_event_num; i++)
            {
                if (event_list[i].data.fd == m_server_listen_fd->getFD())
                {
                    // 新客户端连接
                    auto peer_client = std::make_shared<TCPPeerClient>();
                    // TODO 这里应对peer_client进行设置，比如回调之类的

                    socklen_t len = sizeof(sockaddr_in);
                    auto conn = std::make_shared<FileDescribe>(accept(m_server_listen_fd->getFD(), (sockaddr *)peer_client->getSockAddr(), &len));
                    if (conn->isInvalid())
                    {
                        return JResultWithErrMsg::failure("accept failed");
                    }

                    peer_client->setFileDescribe(conn);
                    printf("client connect, conn:%d(fd:%d),ip:%s, port:%d\n", conn->getFD(), m_server_listen_fd->getFD(), peer_client->getPeerIP().data(), peer_client->getPeerPort());

                    // 设为非阻塞
                    setfdisblock(conn->getFD(), false);
                    // 将该event设置为监听目标

                    struct epoll_event event;
                    event.data.fd = conn->getFD(); // 这样在epoll_wait返回时就可以直接用了
                    event.events = EPOLLIN | EPOLLET;
                    epoll_ctl(m_epollfd, EPOLL_CTL_ADD, conn->getFD(), &event);

                    m_client_mgr[conn->getFD()] = peer_client;
                    m_on_new_client_cb(peer_client);
                }
                else if (event_list[i].events & EPOLLIN)
                {
                    if (event_list[i].data.fd < 0)
                    {
                        continue;
                    }
                    auto peer_client = m_client_mgr.at(event_list[i].data.fd);

                    // 接受数据
                    char buf[1024] = {0};
                    int ret = read(peer_client->getFileDescribe()->getFD(), buf, sizeof(buf));
                    if (-1 == ret)
                    {
                        return JResultWithErrMsg::failure("read failed");
                    }
                    else if (0 == ret)
                    {
                        // 从epoll中清除回调
                        auto &event = event_list[i];
                        epoll_ctl(m_epollfd, EPOLL_CTL_DEL, peer_client->getFileDescribe()->getFD(), &event);
                        m_client_mgr.erase(peer_client->getFileDescribe()->getFD());

                        // 触发客户端退出的回调，通知退出
                        peer_client->onClientExit();
                    }
                    else
                    {
                        // 接受数据，触发客户端接受数据的回调
                        peer_client->onRecvData(buf, ret);
                    }
                }
            }
        }

        // 服务停止，释放各个资源
        m_run_flag = false;
        m_client_mgr.clear();
        return JResultWithErrMsg::success();
    }

    void setfdisblock(int fd, bool isblock)
    {
        int flags = fcntl(fd, F_GETFL);
        if (flags < 0)
            return;
        if (isblock) // 阻塞
        {
            flags &= ~O_NONBLOCK;
        }
        else // 非阻塞
        {
            flags |= O_NONBLOCK;
        }

        if (fcntl(fd, F_SETFL, flags) < 0)
            perror("fcntl set");
    }

private:
    OnNewClientCBType m_on_new_client_cb;
    FileDescribePtr m_server_listen_fd{nullptr};
    std::future<JResultWithErrMsg> m_accept_thread;

    using ClientMgrType = std::unordered_map<int, TCPPeerClientPtr>;
    ClientMgrType m_client_mgr;

    using EpollFileDescribeType = int;
    EpollFileDescribeType m_epollfd;

    bool m_run_flag{false};
};

constexpr uint16_t port = 8080;

int main(int argc, char const *argv[])
{
    TCPServer server;
    server.setOnNewClient([&](TCPPeerClientPtr client)
                          { 
                            printf("a new client connected, ip_port: %s:%d\n", client->getPeerIP().data(), client->getPeerPort());
                            client->setOnClientExitCB([&]()
                                                      { printf("client exit\n"); });
                            client->setOnRecvDataCB([&](const char *data, std::size_t len)
                                                    { printf("recv data from client, data(%lu): %s\n", len, data); }); });

    if (auto ret = server.start("0.0.0.0", port); ret.isFailure())
    {
        printf("server start failed, error: %s\n", ret.getFailurePtr()->c_str());
        return -1;
    }
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}

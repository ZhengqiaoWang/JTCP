#ifndef _JTCP_SERVER_SERVER_H_
#define _JTCP_SERVER_SERVER_H_

#include "JResult/JResult.h"
#include "JTCP/server/peer_client.h"
#include <fcntl.h>
#include <future>

namespace JTCP::Server
{
    /**
     * @brief TCP服务对象
     *
     */
    class TCPServer
    {
    public:
        using OnNewClientCBType = std::function<void(TCPPeerClientPtr)>;
        /**
         * @brief 设置当有客户端链接时触发的回调
         *
         * @param cb 回调函数
         */
        void setOnNewClient(OnNewClientCBType cb) noexcept
        {
            m_on_new_client_cb = cb;
        }

        using ListenMaxNumType = int32_t;
        /**
         * @brief 开始监听
         *
         * @param listen_addr 监听地址
         * @param listen_port 监听端口
         * @param listen_max_num 最大监听队列数量，超出的请求会被拒绝
         * @return JResultWithErrMsg 返回值
         */
        JResultWithErrMsg start(const Types::IPStrType &listen_addr, const Types::PortType &listen_port, const ListenMaxNumType &listen_max_num = 20)
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
        /**
         * @brief epoll处理线程函数
         *
         * @param listen_max_num 最大监听数量
         * @return JResultWithErrMsg 返回值
         */
        JResultWithErrMsg acceptThreadFunc(const ListenMaxNumType &listen_max_num)
        {
            if (auto ret = initEpoll(); ret.isFailure())
            {
                return ret;
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

                // 对每个事件进行处理
                for (ReadyNumType i = 0; i < ready_event_num; i++)
                {
                    auto &event = event_list[i];
                    if (event.data.fd == m_server_listen_fd->getFD())
                    {
                        if (auto ret = handleNewClientConnect(); ret.isFailure())
                        {
                            return ret;
                        }
                    }
                    else if (event.events & EPOLLIN)
                    {
                        if (event.data.fd < 0)
                        {
                            continue;
                        }
                        if (auto ret = handleClientMsg(event); ret.isFailure())
                        {
                            return ret;
                        }
                    }
                }
            }

            // 服务停止，释放各个资源
            m_run_flag = false;
            m_client_mgr.clear();
            return JResultWithErrMsg::success();
        }

        JResultWithErrMsg initEpoll()
        {
            // 创建一个 epoll 实例，并设置文件描述符为关闭执行时关闭
            m_epollfd = epoll_create1(EPOLL_CLOEXEC);
            return epollAddEvent(m_server_listen_fd->getFD(), EPOLLIN | EPOLLET);
        }

        using EpollEventType = uint32_t;
        JResultWithErrMsg epollAddEvent(FileDescribe::FDType fd, EpollEventType epoll_events){
            struct epoll_event event;
            // 设置event为边缘触发模式，并关注读事件
            event.events = epoll_events;
            // 设置event的fd为监听的fd
            event.data.fd = fd;
            // 将监听的fd添加到epoll中
            if (epoll_ctl(m_epollfd, EPOLL_CTL_ADD, fd, &event) < 0)
            {
                // 如果添加失败，返回错误信息
                return JResultWithErrMsg::failure(
                    std::string("epoll_ctl when add event:") +
                    std::to_string(epoll_events) +
                    " for fd: " +
                    std::to_string(fd) +
                    " failed");
            }

            return JResultWithErrMsg::success();
        }

        JResultWithErrMsg handleNewClientConnect()
        {
            // 新客户端连接
            auto peer_client = std::make_shared<TCPPeerClient>();

            socklen_t len = sizeof(sockaddr_in);
            auto conn = std::make_shared<FileDescribe>(accept(m_server_listen_fd->getFD(), (sockaddr *)peer_client->getSockAddr(), &len));
            if (conn->isInvalid())
            {
                return JResultWithErrMsg::failure("accept failed");
            }

            peer_client->setFileDescribe(conn);

            // 设为非阻塞
            if (auto ret = setFileDiscribeBlock(conn->getFD(), false); ret.isFailure())
            {
                return ret;
            }

            // 将该event设置为监听目标
            if (auto ret = epollAddEvent(conn->getFD(), EPOLLIN | EPOLLET); ret.isFailure())
            {
                return ret;
            }

            m_client_mgr[conn->getFD()] = peer_client;
            m_on_new_client_cb(peer_client);

            return JResultWithErrMsg::success();
        }

        JResultWithErrMsg handleClientMsg(epoll_event &event)
        {
            auto peer_client = m_client_mgr.at(event.data.fd);

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
                if (epoll_ctl(m_epollfd, EPOLL_CTL_DEL, peer_client->getFileDescribe()->getFD(), &event) < 0)
                {
                    return JResultWithErrMsg::failure("epoll_ctl when handle client msg failed");
                }
                m_client_mgr.erase(peer_client->getFileDescribe()->getFD());

                // 触发客户端退出的回调，通知退出
                peer_client->onClientExit();
            }
            else
            {
                // 接受数据，触发客户端接受数据的回调
                peer_client->onRecvData(buf, ret);
            }

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
        JResultWithErrMsg setFileDiscribeBlock(FileDescribe::FDType fd, bool is_block)
        {
            // 获取当前文件描述符的属性
            int flags = fcntl(fd, F_GETFL);
            if (flags < 0)
            {
                // 如果获取属性失败，直接返回成功，不做任何操作
                JResultWithErrMsg::success();
            }
            // 根据is_block参数设置文件描述符为阻塞或非阻塞模式
            if (is_block) // 阻塞
            {
                flags &= ~O_NONBLOCK;
            }
            else // 非阻塞
            {
                flags |= O_NONBLOCK;
            }

            // 尝试设置文件描述符的新属性
            if (fcntl(fd, F_SETFL, flags) < 0)
            {
                // 如果设置失败，返回一个表示失败的结果对象，并附带错误消息
                return JResultWithErrMsg::failure("fcntl failed");
            }
            // 如果设置成功，返回一个表示成功的对象
            return JResultWithErrMsg::success();
        }

    private:
        OnNewClientCBType m_on_new_client_cb;           ///< 新客户端连接的回调
        FileDescribePtr m_server_listen_fd{nullptr};    ///< 监听的文件描述符
        std::future<JResultWithErrMsg> m_accept_thread; ///< 监听连接的线程

        using ClientMgrType = std::unordered_map<FileDescribe::FDType, TCPPeerClientPtr>;
        ClientMgrType m_client_mgr; ///< 客户端管理

        using EpollFileDescribeType = FileDescribe::FDType;
        EpollFileDescribeType m_epollfd; ///< epoll文件描述符

        bool m_run_flag{false}; ///< 运行标志
    };
}

#endif
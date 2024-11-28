/**
 * @author Wangzhengqiao (me@zhengqiao.wang)
 * @date 2024-11-28
 * 
 */
#pragma once

#include <arpa/inet.h>
#include <memory>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace JTCP {
/**
 * @brief 文件描述符RAII封装
 *
 */
class FileDescribe
{
public:
    using FDType = int;
    FileDescribe(const FDType& fd)
        : m_fd(fd)
    {}
    ~FileDescribe() { close(m_fd); }

    // 删除默认构造函数，防止外部创建FileDescribe的实例
    FileDescribe()                    = delete;
    FileDescribe(const FileDescribe&) = delete;
    FileDescribe(FileDescribe&&)      = delete;

public:
    FDType getFD() const { return m_fd; }
    bool   isValid() const { return m_fd >= 0; }
    bool   isInvalid() const { return false == isValid(); }

private:
    FDType m_fd{0};
};
using FileDescribePtr = std::shared_ptr<FileDescribe>;
};   // namespace JTCP

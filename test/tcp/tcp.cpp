#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory>

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

private:
    FDType m_fd{0};
};
using FileDescribePtr = std::shared_ptr<FileDescribe>;

class TCPPeerClient
{
public:
private:
    FDType m_fd;
};

class TCPServer
{
public:

private:
};

int main(int argc, char const *argv[])
{
    /* code */
    return 0;
}

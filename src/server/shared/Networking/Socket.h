#ifndef __SOCKET_H__
#define __SOCKET_H__

#include "MessageBuffer.h"
#include "Log.h"
#include <atomic>
#include <queue>
#include <memory>
#include <functional>
#include <type_traits>
#include <boost/asio/ip/tcp.hpp>

using boost::asio::ip::tcp;

#define READ_BLOCK_SIZE 4096
#ifdef BOOST_ASIO_HAS_IOCP
#define TC_SOCKET_USE_IOCP
#endif

template<class T>
class Socket : public std::enable_shared_from_this<T>
{
public:
    explicit Socket(tcp::socket&& socket) : _socket(std::move(socket)), _remoteAddress(_socket.remote_endpoint().address()),
        _remotePort(_socket.remote_endpoint().port()), _readBuffer(), _closed(false), _closing(false), _isWritingAsync(false)
    {
        _readBuffer.Resize(READ_BLOCK_SIZE);
    }

    virtual ~Socket()
    {
        _closed = true;
        boost::system::error_code error;
        _socket.close(error);
    }

    virtual void Start() = 0;

    virtual bool Update()
    {
        if (_closed)
            return false;

#ifndef TC_SOCKET_USE_IOCP // 这个函数是用来处理异步写入的，如果不需要异步写入，就跳过
        if (_isWritingAsync || (_writeQueue.empty() && !_closing))
            return true;
        // 启动异步写入
        for (; HandleQueue();); // 处理队列中的数据
#endif

        return true;
    }

    boost::asio::ip::address GetRemoteIpAddress() const
    {
        return _remoteAddress;
    }

    uint16 GetRemotePort() const
    {
        return _remotePort;
    }
    // 此函数用于异步读取数据 版本1
    void AsyncRead()
    {
        if (!IsOpen())
            return;

        _readBuffer.Normalize();
        _readBuffer.EnsureFreeSpace();
        // shared_from_this() 返回一个指向当前对象的共享指针，用于在异步操作中保持对象的生存期
        // 不能使用make_shared，这样会导致循环引用
        // 不能用this，因为this是裸指针，不是共享指针，一旦this被销毁，再异步地访问它，那么就会出现问题
        _socket.async_read_some(boost::asio::buffer(_readBuffer.GetWritePointer(), _readBuffer.GetRemainingSpace()),
            std::bind(&Socket<T>::ReadHandlerInternal, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
            // 写入到缓冲区内，后续读取出来
    }
    // 此函数用于异步读取数据，并调用回调函数处理读取的数据 版本2
    void AsyncReadWithCallback(void (T::*callback)(boost::system::error_code, std::size_t))
    {
        if (!IsOpen())
            return;

        _readBuffer.Normalize(); // 清除已读的数据，读指针指向缓冲区的开始位置
        _readBuffer.EnsureFreeSpace(); // 动态地扩展缓冲区，确保有足够的空间来存储新的数据
        _socket.async_read_some(boost::asio::buffer(_readBuffer.GetWritePointer(), _readBuffer.GetRemainingSpace()),
            std::bind(callback, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    }

    // 异步发送给客户端数据
    void QueuePacket(MessageBuffer&& buffer)
    {
        _writeQueue.push(std::move(buffer));

#ifdef TC_SOCKET_USE_IOCP
        AsyncProcessQueue();
#endif
    }

    bool IsOpen() const { return !_closed && !_closing; }

    void CloseSocket()
    {
        if (_closed.exchange(true))
            return;

        boost::system::error_code shutdownError;
        _socket.shutdown(boost::asio::socket_base::shutdown_send, shutdownError);
        if (shutdownError)
            TC_LOG_DEBUG("network", "Socket::CloseSocket: {} errored when shutting down socket: {} ({})", GetRemoteIpAddress().to_string(),
                shutdownError.value(), shutdownError.message());

        OnClose();
    }

    // 这个函数是当写入缓冲区为空时，用来标记关闭套接字的
    void DelayedCloseSocket()
    {
        if (_closing.exchange(true))
            return;

        if (_writeQueue.empty())
            CloseSocket();
    }

    MessageBuffer& GetReadBuffer() { return _readBuffer; }

protected:
    virtual void OnClose() {}

    virtual void ReadHandler() = 0;

    bool AsyncProcessQueue()
    {
        if (_isWritingAsync)
            return false;

        _isWritingAsync = true;

#ifdef TC_SOCKET_USE_IOCP
        MessageBuffer& buffer = _writeQueue.front();
        _socket.async_write_some(boost::asio::buffer(buffer.GetReadPointer(), buffer.GetActiveSize()), std::bind(&Socket<T>::WriteHandler,
            this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
#else
        // 启动异步写入操作，实际上这里不写入任何数据，只是设置一个回调函数
        _socket.async_write_some(boost::asio::null_buffers(), std::bind(&Socket<T>::WriteHandlerWrapper,
            this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
#endif

        return false;
    }

    void SetNoDelay(bool enable)
    {
        boost::system::error_code err;
        _socket.set_option(tcp::no_delay(enable), err);
        if (err)
            TC_LOG_DEBUG("network", "Socket::SetNoDelay: failed to set_option(boost::asio::ip::tcp::no_delay) for {} - {} ({})",
                GetRemoteIpAddress().to_string(), err.value(), err.message());
    }

private:
    void ReadHandlerInternal(boost::system::error_code error, size_t transferredBytes)
    {
        if (error)
        {
            CloseSocket();
            return;
        }
        // 将写指针移动到已读数据的后面 更新位置
        _readBuffer.WriteCompleted(transferredBytes);
        ReadHandler(); // 再调用子类的读取处理函数
    }

#ifdef TC_SOCKET_USE_IOCP

    void WriteHandler(boost::system::error_code error, std::size_t transferedBytes)
    {
        if (!error)
        {
            _isWritingAsync = false;
            _writeQueue.front().ReadCompleted(transferedBytes);
            if (!_writeQueue.front().GetActiveSize())
                _writeQueue.pop();

            if (!_writeQueue.empty())
                AsyncProcessQueue();
            else if (_closing)
                CloseSocket();
        }
        else
            CloseSocket();
    }

#else
    // 这里发送数据，使用write_some的方式，也就是说不会一次性发送完所有数据，而是发送一部分，然后等待下一次回调
    void WriteHandlerWrapper(boost::system::error_code /*error*/, std::size_t /*transferedBytes*/)
    {
        _isWritingAsync = false;
        HandleQueue();
    }

    bool HandleQueue()
    {
        if (_writeQueue.empty())
            return false;

        MessageBuffer& queuedMessage = _writeQueue.front();

        std::size_t bytesToSend = queuedMessage.GetActiveSize();

        boost::system::error_code error;
        // 尝试发送数据
        std::size_t bytesSent = _socket.write_some(boost::asio::buffer(queuedMessage.GetReadPointer(), bytesToSend), error);
        // bytesSent实际上发送的数据也许会小于需要发送的数据，所以需要判断

        if (error)
        {
            if (error == boost::asio::error::would_block || error == boost::asio::error::try_again)
                return AsyncProcessQueue();

            _writeQueue.pop();
            if (_closing && _writeQueue.empty())
                CloseSocket();
            return false;
        }
        else if (bytesSent == 0) // 如果要发送的数据为0
        {
            _writeQueue.pop(); // 从队列中移除
            if (_closing && _writeQueue.empty()) // 如果正在关闭并且队列为空
                CloseSocket();
            return false;
        }
        else if (bytesSent < bytesToSend) // now n > 0 如果发送的数据小于队列中的数据，则继续发送
        {
            queuedMessage.ReadCompleted(bytesSent);
            return AsyncProcessQueue();
        }

        _writeQueue.pop();
        if (_closing && _writeQueue.empty())
            CloseSocket();
        return !_writeQueue.empty();
    }

#endif

    tcp::socket _socket;

    boost::asio::ip::address _remoteAddress;
    uint16 _remotePort;

    MessageBuffer _readBuffer;
    std::queue<MessageBuffer> _writeQueue;

    std::atomic<bool> _closed;
    std::atomic<bool> _closing;

    bool _isWritingAsync;
};

#endif // __SOCKET_H__

#ifndef SocketMgr_h__
#define SocketMgr_h__

#include "AsyncAcceptor.h"
#include "Errors.h"
#include "NetworkThread.h"
#include <boost/asio/ip/tcp.hpp>
#include <memory>

using boost::asio::ip::tcp;

template<class SocketType>
class SocketMgr
{
public:
    virtual ~SocketMgr()
    {
        ASSERT(!_threads && !_acceptor && !_threadCount, "StopNetwork must be called prior to SocketMgr destruction");
    }

    virtual bool StartNetwork(Trinity::Asio::IoContext& ioContext, std::string const& bindIp, uint16 port, int threadCount)
    {
        ASSERT(threadCount > 0);

        AsyncAcceptor* acceptor = nullptr;
        try
        {
            acceptor = new AsyncAcceptor(ioContext, bindIp, port);
        }
        catch (boost::system::system_error const& err)
        {
            TC_LOG_ERROR("network", "Exception caught in SocketMgr.StartNetwork ({}:{}): {}", bindIp, port, err.what());
            return false;
        }

        if (!acceptor->Bind())
        {
            TC_LOG_ERROR("network", "StartNetwork failed to bind socket acceptor");
            delete acceptor;
            return false;
        }

        _acceptor = acceptor;
        _threadCount = threadCount;
        _threads = CreateThreads();

        ASSERT(_threads);

        for (int32 i = 0; i < _threadCount; ++i)
            _threads[i].Start();

        _acceptor->SetSocketFactory([this]() { return GetSocketForAccept(); });

        return true;
    }

    virtual void StopNetwork()
    {
        _acceptor->Close();

        if (_threadCount != 0)
            for (int32 i = 0; i < _threadCount; ++i)
                _threads[i].Stop();

        Wait();

        delete _acceptor;
        _acceptor = nullptr;
        delete[] _threads;
        _threads = nullptr;
        _threadCount = 0;
    }

    void Wait()
    {
        if (_threadCount != 0)
            for (int32 i = 0; i < _threadCount; ++i)
                _threads[i].Wait();
    }

    virtual void OnSocketOpen(tcp::socket&& sock, uint32 threadIndex)
    {
        try
        {
            std::shared_ptr<SocketType> newSocket = std::make_shared<SocketType>(std::move(sock));
            newSocket->Start();

            _threads[threadIndex].AddSocket(newSocket);
        }
        catch (boost::system::system_error const& err)
        {
            TC_LOG_WARN("network", "Failed to retrieve client's remote address {}", err.what());
        }
    }

    int32 GetNetworkThreadCount() const { return _threadCount; }
    // 找最少连接的那个线程
    uint32 SelectThreadWithMinConnections() const
    {
        uint32 min = 0;

        for (int32 i = 1; i < _threadCount; ++i)
            if (_threads[i].GetConnectionCount() < _threads[min].GetConnectionCount())
                min = i;

        return min;
    }

    std::pair<tcp::socket*, uint32> GetSocketForAccept()
    {
        uint32 threadIndex = SelectThreadWithMinConnections();
        // 返回一个pair，第一个是最小负载线程索引对应的socket，第二个是最小负载线程索引
        return std::make_pair(_threads[threadIndex].GetSocketForAccept(), threadIndex);
    }

protected:
    SocketMgr() : _acceptor(nullptr), _threads(nullptr), _threadCount(0)
    {
    }

    virtual NetworkThread<SocketType>* CreateThreads() const = 0;

    AsyncAcceptor* _acceptor;
    NetworkThread<SocketType>* _threads;
    int32 _threadCount;
};

#endif // SocketMgr_h__

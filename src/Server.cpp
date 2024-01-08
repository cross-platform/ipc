/******************************************************************************
IPC - Simple cross-platform C++ IPC library
Copyright (c) 2024, Marcus Tomlinson

BSD 2-Clause License

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#include <Server.h>

#include <Common.h>
#include <Message.h>

#include <assert.h>
#include <iostream>

using namespace Ipc;

namespace Ipc::Private
{

class ServerImpl final
{
public:
    explicit ServerImpl( const std::filesystem::path& path )
        : socketPath( path.string() )
    {
        std::error_code err;
        std::filesystem::create_directories( path.parent_path(), err );

#ifdef _WIN32
        WSADATA wsd;
        if ( WSAStartup( WINSOCK_VERSION, &wsd ) != 0 )
        {
            std::cerr << "WSAStartup failed" << std::endl;
            return;
        }
#endif

        if ( socketPath.length() > sizeof( sockaddr_un::sun_path ) )
        {
            std::cerr << "socket path too long: " << socketPath << std::endl;
            return;
        }

        // Create a AF_UNIX stream server socket
        serverSocket = socket( AF_UNIX, SOCK_STREAM, 0 );
        if ( serverSocket == INVALID_SOCKET )
        {
            std::cerr << "socket failed (error: " << lastError() << ")" << std::endl;
            return;
        }

        memset( &socketAddr, 0, sizeof( socketAddr ) );
        socketAddr.sun_family = AF_UNIX;
        strncpy( socketAddr.sun_path, socketPath.c_str(), socketPath.length() );

#ifdef _WIN32
        int timeout = 2000;
        setsockopt( serverSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>( &timeout ),
                    sizeof( timeout ) );
        setsockopt( serverSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>( &timeout ),
                    sizeof( timeout ) );
#else
        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;
        setsockopt( serverSocket, SOL_SOCKET, SO_SNDTIMEO, static_cast<const void*>( &timeout ), sizeof( timeout ) );
        setsockopt( serverSocket, SOL_SOCKET, SO_RCVTIMEO, static_cast<const void*>( &timeout ), sizeof( timeout ) );
#endif

        // Bind the socket to the path
        remove( socketPath.c_str() );
        if ( bind( serverSocket, reinterpret_cast<const sockaddr*>( &socketAddr ), sizeof( socketAddr ) ) ==
             SOCKET_ERROR )
        {
            std::cerr << "bind failed (error: " << lastError() << ")" << std::endl;
            closesocket( serverSocket );
            serverSocket = INVALID_SOCKET;
            return;
        }

        // Listen to start accepting connections
        if ( listen( serverSocket, SOMAXCONN ) == SOCKET_ERROR )
        {
            std::cerr << "listen failed (error: " << lastError() << ")" << std::endl;
            closesocket( serverSocket );
            serverSocket = INVALID_SOCKET;
            return;
        }

        std::cout << "Accepting connections" << std::endl;
    }

    ~ServerImpl()
    {
        if ( serverSocket != INVALID_SOCKET )
        {
            closesocket( serverSocket );
        }

        remove( socketPath.c_str() );
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool Listen( const std::function<Message( const Message& header, const Message& message )>& callback )
    {
        if ( serverSocket == INVALID_SOCKET )
        {
            std::cerr << "Server socket is invalid" << std::endl;
            return false;
        }

        // Accept a connection
        fd_set fd;
        FD_ZERO( &fd );
        FD_SET( serverSocket, &fd );

        struct timeval timeout;
        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        if ( select( (int)serverSocket + 1, &fd, nullptr, nullptr, &timeout ) <= 0 )
        {
            return false;
        }

        SOCKET clientSocket = accept( serverSocket, NULL, NULL );
        if ( clientSocket == INVALID_SOCKET )
        {
            std::cerr << "accept failed (error: " << lastError() << ")" << std::endl;
            return false;
        }

#ifdef _WIN32
        int timeoutMs = 2000;
        setsockopt( clientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>( &timeoutMs ),
                    sizeof( timeoutMs ) );
        setsockopt( clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>( &timeoutMs ),
                    sizeof( timeoutMs ) );
#else
        setsockopt( clientSocket, SOL_SOCKET, SO_SNDTIMEO, static_cast<const void*>( &timeout ), sizeof( timeout ) );
        setsockopt( clientSocket, SOL_SOCKET, SO_RCVTIMEO, static_cast<const void*>( &timeout ), sizeof( timeout ) );
#endif

        // Receive header data
        auto recvHeaderBytes = Receive( clientSocket );
        if ( recvHeaderBytes.empty() )
        {
            if ( lastError() != EINVAL )
            {
                std::cerr << "recieve header failed (error: " << lastError() << ")" << std::endl;
            }
            closesocket( clientSocket );
            return false;
        }

        // Send ack
        if ( !Send( clientSocket, std::vector<unsigned char>{ 1 } ) )
        {
            std::cerr << "send ack failed (error: " << lastError() << ")" << std::endl;
            closesocket( clientSocket );
            return false;
        }

        // Receive message data
        auto recvMessageBytes = Receive( clientSocket );
        if ( recvMessageBytes.empty() )
        {
            std::cerr << "recieve message failed (error: " << lastError() << ")" << std::endl;
            closesocket( clientSocket );
            return false;
        }

        // Send some data
        auto sendMessage = callback( recvHeaderBytes, recvMessageBytes );
        if ( !Send( clientSocket, sendMessage ) )
        {
            std::cerr << "send response failed (error: " << lastError() << ")" << std::endl;
            closesocket( clientSocket );
            return false;
        }

        closesocket( clientSocket );
        return true;
    }

    bool StopListening() const
    {
        SOCKET clientSocket = socket( AF_UNIX, SOCK_STREAM, PF_UNSPEC );
        if ( clientSocket == INVALID_SOCKET )
        {
            std::cerr << "socket failed (error: " << lastError() << ")" << std::endl;
            return false;
        }

        if ( connect( clientSocket, reinterpret_cast<const sockaddr*>( &socketAddr ), sizeof( socketAddr ) ) ==
             SOCKET_ERROR )
        {
            std::cerr << "connect failed (error: " << lastError() << ")" << std::endl;
            closesocket( clientSocket );
            return false;
        }

        closesocket( clientSocket );
        return true;
    }

    static bool Send( SOCKET clientSocket, const Message& message )
    {
        int sendResult = send( clientSocket, reinterpret_cast<const char*>( message.AsRaw() ), (int)message.Size(), 0 );
        if ( sendResult == SOCKET_ERROR )
        {
            return false;
        }
        return true;
    }

    std::vector<unsigned char> Receive( SOCKET clientSocket )
    {
        std::vector<unsigned char> result;

        int recvResult = 0;
        int recvFlags = 0;
        do
        {
            recvResult = recv( clientSocket, reinterpret_cast<char*>( recvBytes ), c_recvBufferSize, recvFlags );
            if ( recvResult > 0 )
            {
                result.insert( result.end(), recvBytes, recvBytes + recvResult );
#ifdef _WIN32
                u_long opt = 1;
                ioctlsocket( clientSocket, FIONBIO, &opt );
#else
                recvFlags = MSG_DONTWAIT;
#endif
            }
            else if ( recvResult < 0 )
            {
                auto const errCode = lastError();
#ifdef _WIN32
                if ( errCode != WSAEWOULDBLOCK )
#else
                if ( errCode != EWOULDBLOCK )
#endif
                {
                    result.clear();
                    std::cerr << "recv failed (error: " << errCode << ")" << std::endl;
                }
            }
        } while ( recvResult > 0 );

#ifdef _WIN32
        u_long opt = 0;
        ioctlsocket( clientSocket, FIONBIO, &opt );
#endif

        return result;
    }

    SOCKET serverSocket = INVALID_SOCKET;
    std::string socketPath = "";
    sockaddr_un socketAddr;
    unsigned char recvBytes[c_recvBufferSize] = {};
};

}  // namespace Ipc::Private

Server::Server( const std::filesystem::path& socketPath )
    : p( std::make_unique<Private::ServerImpl>( socketPath ) )
{
}

Server::~Server() = default;

bool Server::Listen( const std::function<Message( const Message& header, const Message& message )>& callback )
{
    return p->Listen( callback );
}

bool Server::StopListening()
{
    return p->StopListening();
}

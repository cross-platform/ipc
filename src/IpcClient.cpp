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

#include <IpcClient.h>

#include <IpcCommon.h>

#include <mutex>

using namespace Ipc;

namespace Ipc::Private
{

class ClientImpl
{
public:
    explicit ClientImpl( const std::filesystem::path& path )
        : socketPath( path.string() )
    {
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
                auto const errorCode = lastError();
#ifdef _WIN32
                if ( errorCode != WSAEWOULDBLOCK )
#else
                if ( errorCode != EWOULDBLOCK )
#endif
                {
                    result.clear();
                }
            }
        } while ( recvResult > 0 );

#ifdef _WIN32
        u_long opt = 0;
        ioctlsocket( clientSocket, FIONBIO, &opt );
#endif

        return result;
    }

    std::string initError;
    std::string socketPath = "";
    sockaddr_un socketAddr;
    unsigned char recvBytes[c_recvBufferSize] = {};

    std::mutex sendMutex;
};

}  // namespace Ipc::Private

Client::Client( const std::filesystem::path& socketPath )
    : p( std::make_unique<Private::ClientImpl>( socketPath ) )
{
#ifdef _WIN32
    WSADATA wsd;
    if ( WSAStartup( WINSOCK_VERSION, &wsd ) != 0 )
    {
        p->initError = "WSAStartup() failed";
        return;
    }
#endif

    if ( p->socketPath.length() > sizeof( sockaddr_un::sun_path ) )
    {
        p->initError = "socket path too long: " + p->socketPath;
        return;
    }

    memset( &p->socketAddr, 0, sizeof( p->socketAddr ) );
    p->socketAddr.sun_family = AF_UNIX;
    strncpy( p->socketAddr.sun_path, p->socketPath.c_str(), p->socketPath.length() );
}

Client::~Client()
{
    std::lock_guard<std::mutex> lock( p->sendMutex );
#ifdef _WIN32
    WSACleanup();
#endif
}

Message Client::Send( const Message& header, const Message& message )
{
    if ( !p->initError.empty() )
    {
        return Message( p->initError, true );
    }

    std::lock_guard<std::mutex> lock( p->sendMutex );

    if ( header.Size() == 0 )
    {
        return Message( "header can not be empty", true );
    }
    if ( message.Size() == 0 )
    {
        return Message( "message can not be empty", true );
    }

    SOCKET clientSocket = socket( AF_UNIX, SOCK_STREAM, PF_UNSPEC );
    if ( clientSocket == INVALID_SOCKET )
    {
        return Message( "socket() failed (error: " + std::to_string( lastError() ) + ")", true );
    }

#ifdef _WIN32
    int timeout = 2000;
    setsockopt( clientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>( &timeout ), sizeof( timeout ) );
    setsockopt( clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>( &timeout ), sizeof( timeout ) );
#else
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt( clientSocket, SOL_SOCKET, SO_SNDTIMEO, static_cast<const void*>( &timeout ), sizeof( timeout ) );
    setsockopt( clientSocket, SOL_SOCKET, SO_RCVTIMEO, static_cast<const void*>( &timeout ), sizeof( timeout ) );
#endif

    if ( connect( clientSocket, reinterpret_cast<const sockaddr*>( &p->socketAddr ), sizeof( p->socketAddr ) ) ==
         SOCKET_ERROR )
    {
        closesocket( clientSocket );
        return Message( "connect() failed (error: " + std::to_string( lastError() ) + ")", true );
    }

    // Send header data
    if ( !p->Send( clientSocket, header ) )
    {
        closesocket( clientSocket );
        return Message( "header send() failed (error: " + std::to_string( lastError() ) + ")", true );
    }

    // Receive ack
    auto recvBytes = p->Receive( clientSocket );
    if ( recvBytes.empty() || recvBytes[0] != 1 )
    {
        closesocket( clientSocket );
        return Message( "ack recv() failed (error: " + std::to_string( lastError() ) + ")", true );
    }

    // Send message data
    if ( !p->Send( clientSocket, message ) )
    {
        closesocket( clientSocket );
        return Message( "message send() failed (error: " + std::to_string( lastError() ) + ")", true );
    }

    // Receive some data
    recvBytes = p->Receive( clientSocket );

    closesocket( clientSocket );
    return recvBytes;
}

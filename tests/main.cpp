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
#include <IpcServer.h>

#include <gtest/gtest.h>

#include <future>
#include <thread>

static const char* c_serverSocket = "server.sock";

Ipc::Message RecvCallback( const Ipc::Message& recvHeader, const Ipc::Message& recvMessage )
{
    if ( recvMessage.AsString() != "Hello?" )
    {
        EXPECT_FALSE( recvHeader.IsError() );
        EXPECT_EQ( recvHeader.AsString(), "bin" );
        EXPECT_EQ( recvHeader.Size(), 3 );
        for ( size_t i = 0; i < recvHeader.Size(); ++i )
        {
            EXPECT_EQ( recvHeader.AsByteVect()[i], recvHeader.AsRaw()[i] );
        }

        EXPECT_FALSE( recvMessage.IsError() );
        EXPECT_EQ( recvMessage.AsRaw()[0], 0 );
        EXPECT_EQ( recvMessage.AsString()[0], '\0' );
        EXPECT_EQ( recvMessage.AsByteVect(), std::vector<unsigned char>{ 0 } );

        return std::vector<unsigned char>{ 1 };
    }

    return std::string( "Unix Domain Sockets!" );
}

TEST( Ipc, SameProcess )
{
    Ipc::Server server( c_serverSocket );
    auto listenThread = std::thread(
        [&server]
        {
            ASSERT_FALSE( server.Listen( RecvCallback ).IsError() );
            ASSERT_FALSE( server.Listen( RecvCallback ).IsError() );
        } );

    Ipc::Client client( c_serverSocket );
    Ipc::Client client2( c_serverSocket );

    auto response = client.Send( std::string( "bin" ), std::vector<unsigned char>{ 0 } );
    ASSERT_FALSE( response.IsError() );
    ASSERT_EQ( response.AsByteVect(), std::vector<unsigned char>{ 1 } );

    auto response2 = client2.Send( std::vector<unsigned char>{ 0 }, std::string( "Hello?" ) );
    ASSERT_FALSE( response2.IsError() );
    ASSERT_EQ( response2.AsString(), "Unix Domain Sockets!" );

    listenThread.join();
}

TEST( Ipc, SeparateProcess )
{
    std::promise<void> ready;
    std::thread(
        [&ready]
        {
            Ipc::Server server( c_serverSocket );
            ready.set_value();
            ASSERT_FALSE( server.Listen( RecvCallback ).IsError() );
            ASSERT_FALSE( server.Listen( RecvCallback ).IsError() );
        } )
        .detach();
    ready.get_future().wait();

    EXPECT_EXIT(
        {
            Ipc::Client client( c_serverSocket );

            auto response = client.Send( std::string( "bin" ), std::vector<unsigned char>{ 0 } );
            ASSERT_FALSE( response.IsError() );
            ASSERT_EQ( response.AsByteVect(), std::vector<unsigned char>{ 1 } );

            exit( 0 );
        },
        testing::ExitedWithCode( 0 ), "" );

    EXPECT_EXIT(
        {
            Ipc::Client client( c_serverSocket );

            auto response = client.Send( std::vector<unsigned char>{ 0 }, std::string( "Hello?" ) );
            ASSERT_FALSE( response.IsError() );
            ASSERT_EQ( response.AsString(), "Unix Domain Sockets!" );

            exit( 0 );
        },
        testing::ExitedWithCode( 0 ), "" );
}

TEST( Ipc, StopListening )
{
    Ipc::Server server( c_serverSocket );
    auto listenThread = std::thread( [&server] { ASSERT_FALSE( server.Listen( RecvCallback ).IsError() ); } );

    server.StopListening();

    listenThread.join();
}

TEST( Ipc, PathTooLong )
{
    auto longPath =
        "really/really/really/really/really/really/really/really/really/really/really/really/really/really/really/"
        "really/really/really/really/really/really/really/really/really/really/really/really/really/really/long/path";

    // Server
    Ipc::Server server( longPath );

    auto result = server.Listen( RecvCallback );
    ASSERT_TRUE( result.IsError() );
    ASSERT_EQ( result.AsString(), std::string( "socket path too long: " ) + longPath );

    // Client
    Ipc::Client client( longPath );
    auto response = client.Send( Ipc::Message( "header" ), Ipc::Message( "message" ) );
    ASSERT_TRUE( response.IsError() );
    ASSERT_EQ( response.AsString(), std::string( "socket path too long: " ) + longPath );
}

TEST( Ipc, EmptyMessage )
{
    Ipc::Client client( c_serverSocket );

    auto response1 = client.Send( Ipc::Message( "" ), Ipc::Message( "message" ) );
    ASSERT_TRUE( response1.IsError() );
    ASSERT_EQ( response1.AsString(), "header can not be empty" );

    auto response2 = client.Send( Ipc::Message( "header" ), Ipc::Message( "" ) );
    ASSERT_TRUE( response2.IsError() );
    ASSERT_EQ( response2.AsString(), "message can not be empty" );
}

TEST( Ipc, MessageConversion )
{
    std::string messageStr = "test message 1 2 3";
    Ipc::Message message( reinterpret_cast<unsigned char*>( &messageStr[0] ), messageStr.size() );

    ASSERT_FALSE( message.IsError() );

    ASSERT_EQ( message.Size(), messageStr.size() );
    ASSERT_EQ( message.AsString().size(), messageStr.size() );
    ASSERT_EQ( message.AsByteVect().size(), messageStr.size() );

    ASSERT_EQ( message.AsString(), messageStr );

    for ( size_t i = 0; i < messageStr.size(); ++i )
    {
        ASSERT_EQ( message.AsByteVect()[i], (unsigned char)messageStr[i] );
        ASSERT_EQ( message.AsRaw()[i], (unsigned char)messageStr[i] );
    }
}

int main( int argc, char** argv )
{
    ::testing::InitGoogleTest( &argc, argv );

    return RUN_ALL_TESTS();
}

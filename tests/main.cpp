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

#include <Client.h>
#include <Server.h>

#include <gtest/gtest.h>

#include <thread>

static const char* c_serverSocket = "server.sock";

Ipc::Message RecvCallback( const Ipc::Message& recvHeader, const Ipc::Message& recvMessage )
{
    if ( recvMessage.AsString() != "Hello?" )
    {
        EXPECT_EQ( recvHeader.AsString(), "bin" );
        EXPECT_EQ( recvMessage.AsByteVect(), std::vector<unsigned char>{ 0 } );
        EXPECT_EQ( recvMessage.AsRaw()[0], 0 );
        return std::vector<unsigned char>{ 1 };
    }

    return std::string( "Unix Domain Sockets!" );
}

TEST( Client, SameProcess )
{
    Ipc::Server server( c_serverSocket );
    auto listenThread = std::thread(
        [&server]
        {
            ASSERT_TRUE( server.Listen( RecvCallback ) );
            ASSERT_TRUE( server.Listen( RecvCallback ) );
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

TEST( Simple, SeparateProcess )
{
    std::thread(
        []
        {
            Ipc::Server server( c_serverSocket );
            ASSERT_TRUE( server.Listen( RecvCallback ) );
            ASSERT_TRUE( server.Listen( RecvCallback ) );
        } )
        .detach();

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

TEST( Server, StopListening )
{
    Ipc::Server server( c_serverSocket );
    auto listenThread = std::thread( [&server] { ASSERT_FALSE( server.Listen( RecvCallback ) ); } );

    server.StopListening();

    listenThread.join();
}

int main( int argc, char** argv )
{
    ::testing::InitGoogleTest( &argc, argv );

    return RUN_ALL_TESTS();
}

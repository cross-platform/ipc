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

#include <IpcMessage.h>

using namespace Ipc;

namespace Ipc::Private
{

class MessageImpl
{
public:
    MessageImpl( unsigned char* message, size_t length )
        : size( length )
        , ownRaw( true )
        , asRaw( message )
    {
    }

    explicit MessageImpl( const std::vector<unsigned char>& message )
    {
        size = message.size();
        if ( size > 0 )
        {
            asByteVect = message;
            asRaw = static_cast<unsigned char*>( &asByteVect[0] );
        }
    }

    MessageImpl( const std::string& message, bool isError )
        : isError( isError )
    {
        size = message.size();
        if ( size > 0 )
        {
            asString = message;
            asRaw = reinterpret_cast<unsigned char*>( &asString[0] );
        }
    }

    ~MessageImpl()
    {
        if ( ownRaw )
        {
            free( asRaw );
        }
    }

    const std::string& AsString() const
    {
        if ( size > 0 && asString.empty() )
        {
            const_cast<MessageImpl*>( this )->asString = std::string( reinterpret_cast<const char*>( asRaw ), size );
        }

        return asString;
    }

    const std::vector<unsigned char>& AsByteVect() const
    {
        if ( size > 0 && asByteVect.empty() )
        {
            const_cast<MessageImpl*>( this )->asByteVect = std::vector<unsigned char>( asRaw, asRaw + size );
        }

        return asByteVect;
    }

    bool isError = false;

    size_t size = 0;
    bool ownRaw = false;
    unsigned char* asRaw = nullptr;

    std::vector<unsigned char> asByteVect;
    std::string asString;
};

}  // namespace Ipc::Private

Message::Message( unsigned char* message, size_t length )
    : p( std::make_unique<Private::MessageImpl>( message, length ) )
{
}

Message::Message( const std::vector<unsigned char>& message )
    : p( std::make_unique<Private::MessageImpl>( message ) )
{
}

Message::Message( const std::string& message, bool isError )
    : p( std::make_unique<Private::MessageImpl>( message, isError ) )
{
}

Message::~Message()
{
}

bool Message::IsError() const
{
    return p->isError;
}

size_t Message::Size() const
{
    return p->size;
}

const unsigned char* Message::AsRaw() const
{
    return p->asRaw;
}

const std::string& Message::AsString() const
{
    return p->AsString();
}

const std::vector<unsigned char>& Message::AsByteVect() const
{
    return p->AsByteVect();
}

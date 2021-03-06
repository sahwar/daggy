/*
Copyright 2017-2018 Mikhail Milovidov <milovidovmikhail@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "Precompiled.h"
#include "CDefaultRemoteServersFabric.h"

#include "CSshRemoteServer.h"
#include "CLocalRemoteServer.h"

using namespace daggycore;

IRemoteAgregator* CDefaultRemoteServersFabric::createRemoteServer(const DataSource& data_source, QObject* parent_ptr)
{
    IRemoteServer* result = nullptr;

    if (data_source.connection_type.isEmpty() || data_source.connection_type == CSshRemoteServer::connection_type_global)
        result = new CSshRemoteServer(data_source, parent_ptr);
    else if (data_source.connection_type == CLocalRemoteServer::connection_type_global)
        result = new CLocalRemoteServer(data_source, parent_ptr);

    return result;
}

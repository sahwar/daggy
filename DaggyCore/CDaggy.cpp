/*
Copyright 2017-2018 Mikhail Milovidov <milovidovmikhail@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "Precompiled.h"
#include "CDaggy.h"

#include <ssh/sshconnection.h>

#include "CDefaultRemoteServersFabric.h"
#include "IRemoteAgregatorReciever.h"

using namespace daggycore;

CDaggy::CDaggy(const DataSources& data_sources,
                               IRemoteServersFabric* const remote_servers_fabric,
                               QObject* const pParent)
    : IRemoteAgregator(pParent)
    , data_sources_(data_sources)
    , remote_servers_fabric_(remote_servers_fabric == nullptr ? new CDefaultRemoteServersFabric : remote_servers_fabric)
{
}

CDaggy::~CDaggy()
{
    delete remote_servers_fabric_;
}

void CDaggy::connectRemoteAgregatorReciever(IRemoteAgregatorReciever* const remote_agregator_ptr)
{
    connect(this, &IRemoteAgregator::connectionStatusChanged, remote_agregator_ptr, &IRemoteAgregatorReciever::onConnectionStatusChanged);
    connect(this, &IRemoteAgregator::remoteCommandStatusChanged, remote_agregator_ptr, &IRemoteAgregatorReciever::onRemoteCommandStatusChanged);
    connect(this, &IRemoteAgregator::newRemoteCommandStream, remote_agregator_ptr, &IRemoteAgregatorReciever::onNewRemoteCommandStream);
}

void CDaggy::dicsonnectRemoteAgregatorReciever(IRemoteAgregator* const remote_agregator_ptr)
{
    disconnect(remote_agregator_ptr);
}

size_t CDaggy::runingRemoteCommandsCount() const
{
    size_t result = 0;
    for (const IRemoteAgregator* const remote_agregator_ptr : remoteAgregators())
        result += remote_agregator_ptr->runingRemoteCommandsCount();
    return result;
}

void CDaggy::startAgregator()
{
    for (const DataSource& data_source : data_sources_) {
        if (!isExistsRemoteServer(data_source.server_name)) {
            createRemoteServer(data_source);
        }
    }

    const QList<IRemoteAgregator*>& remote_agregators = remoteAgregators();
    for (IRemoteAgregator* const remote_server_ptr : remote_agregators)
        remote_server_ptr->start();

    if (remote_agregators.size() == 0)
        setStopped();
}

void CDaggy::stopAgregator(const bool hard_stop)
{
    for (IRemoteAgregator* const remote_server_ptr : remoteAgregators())
        remote_server_ptr->stop(hard_stop);
}

QList<IRemoteAgregator*> CDaggy::remoteAgregators() const
{
    return findChildren<IRemoteAgregator*>();
}

void CDaggy::createRemoteServer(const DataSource& data_source)
{
    IRemoteAgregator* const remote_server_ptr = remote_servers_fabric_->createRemoteServer(data_source, this);
    if (remote_server_ptr) {
        remote_server_ptr->setObjectName(data_source.server_name);

        connect(remote_server_ptr, &IRemoteAgregator::connectionStatusChanged, this, &IRemoteAgregator::connectionStatusChanged);
        connect(remote_server_ptr, &IRemoteAgregator::remoteCommandStatusChanged, this, &IRemoteAgregator::remoteCommandStatusChanged);
        connect(remote_server_ptr, &IRemoteAgregator::newRemoteCommandStream, this, &IRemoteAgregator::newRemoteCommandStream);

        connect(remote_server_ptr, &IRemoteAgregator::stateChanged, this, &CDaggy::onRemoteAgregatorStateChanged);
    }
}

IRemoteAgregator* CDaggy::getRemoteServer(const QString& server_name) const
{
    return findChild<IRemoteAgregator*>(server_name);
}

bool CDaggy::isExistsRemoteServer(const QString& server_name) const
{
    return getRemoteServer(server_name) != nullptr;
}

size_t CDaggy::notStoppedRemoteAgregatorsCount() const
{
    size_t result = 0;
    for (const IRemoteAgregator* remote_agregator_ptr : remoteAgregators()) {
        if (remote_agregator_ptr->state() != State::Stopped)
            result++;
    }
    return result;
}

void CDaggy::onRemoteAgregatorStateChanged(const IRemoteAgregator::State agregator_state)
{
    if (agregator_state == State::Stopped && notStoppedRemoteAgregatorsCount() == 0) {
        setStopped();
    }
}

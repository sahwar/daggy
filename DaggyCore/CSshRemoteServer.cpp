/*
Copyright 2017-2018 Mikhail Milovidov <milovidovmikhail@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "Precompiled.h"
#include "CSshRemoteServer.h"

#include <ssh/sshconnection.h>
#include <ssh/sshremoteprocess.h>

using namespace QSsh;
using namespace daggycore;

namespace
{

constexpr const char* host_field_global( "host" );
constexpr const char* login_field_global( "login" );
constexpr const char* password_field_global( "password" );
constexpr const char* key_field_global( "key" );
constexpr const char* timeout_field_global( "timeout" );
constexpr const char* port_field_global( "port" );
constexpr const char* force_kill_global( "forceKill" );
constexpr const char* ignore_default_proxy_global( "ignoreProxy" );
constexpr const char* enable_strict_conformance_checks_global( "strictConformance" );

constexpr const char* default_host_global( "127.0.0.1" );

constexpr int timeout_default_global = 2;

constexpr int invalid_signal_global = -1;
constexpr int default_kill_signal_global = 15;

constexpr const char* kill_command_global =
  "pids=$(pstree -p $PPID | grep -oP \"\\d+\" | grep -v $PPID | grep -v $$ | tac);"
  "for pid in $pids; do "
  "while kill -0 $pid; do "
  "kill -%1 $pid;"
  "sleep 0.1;"
  "done "
  "done ";

} // namespace

SshConnectionParameters getConnectionParameters( const DataSource& data_source );
RemoteCommand::Status convertStatus( const SshRemoteProcess::ExitStatus exitStatus );
QString userName();
QString privateKeyPath();

CSshRemoteServer::CSshRemoteServer( const DataSource& data_source, QObject* parent_pointer )
  : IRemoteServer( data_source, parent_pointer )
  , ssh_connection_pointer_( new SshConnection( getConnectionParameters( data_source ), this ) )
  , force_kill_( data_source.connection_parameters.value( force_kill_global, default_kill_signal_global ).toInt() )
{
     connect( ssh_connection_pointer_, &SshConnection::connected, this, &CSshRemoteServer::onHostConnected );
     connect( ssh_connection_pointer_, &SshConnection::disconnected, this, &CSshRemoteServer::onHostDisconnected );
     connect( ssh_connection_pointer_, &SshConnection::error, this, &CSshRemoteServer::onHostError );
}

CSshRemoteServer::~CSshRemoteServer()
{
     stop( true );
}

bool CSshRemoteServer::isForceKill() const
{
     return force_kill_ != invalid_signal_global;
}

void CSshRemoteServer::startAgregator()
{
     reconnect();
}

void CSshRemoteServer::startRemoteSshProcess( const QString& command_name, const QString& command )
{
     QSharedPointer<SshRemoteProcess> remote_process_pointer =
       ssh_connection_pointer_->createRemoteProcess( qPrintable( command ) );
     remote_process_pointer->setObjectName( command_name );
     ssh_processes_[command_name] = remote_process_pointer;

     connect( remote_process_pointer.data(), &SshRemoteProcess::started, this, &CSshRemoteServer::onCommandStarted );
     connect( remote_process_pointer.data(),
              &SshRemoteProcess::readyReadStandardOutput,
              this,
              &CSshRemoteServer::onNewStandardStreamData );
     connect( remote_process_pointer.data(),
              &SshRemoteProcess::readyReadStandardError,
              this,
              &CSshRemoteServer::onNewErrorStreamData );
     connect( remote_process_pointer.data(), &SshRemoteProcess::closed, this, &CSshRemoteServer::onCommandWasExit );

     setRemoteCommandStatus( command_name, RemoteCommand::Status::Started );
     remote_process_pointer->start();
}

void CSshRemoteServer::restartCommand( const QString& command_name )
{
     const QSharedPointer<SshRemoteProcess> ssh_remote_process_pointer = ssh_processes_.value( command_name, nullptr );
     if ( ssh_remote_process_pointer && !ssh_remote_process_pointer->isRunning() )
     {
          ssh_processes_.remove( command_name );
     }
     if ( !ssh_remote_process_pointer || !ssh_remote_process_pointer->isRunning() )
     {
          const RemoteCommand& remote_command = getRemoteCommand( command_name );
          startRemoteSshProcess( command_name, remote_command.command );
     }
}

void CSshRemoteServer::stopAgregator( const bool hard_stop )
{
     if ( hard_stop )
     {
          for ( const QSharedPointer<SshRemoteProcess>& remote_process_pointer : ssh_processes_.values() )
          {
               if ( remote_process_pointer->isRunning() )
                    remote_process_pointer->close();
          }
          ssh_connection_pointer_->disconnectFromHost();
     }
     else if ( isForceKill() && isExistsRunningRemoteCommands() )
          killConnection();
     else
          closeConnection();
}

void CSshRemoteServer::reconnect()
{
     if ( ssh_connection_pointer_->state() == SshConnection::Unconnected )
          ssh_connection_pointer_->connectToHost();
}

void CSshRemoteServer::onHostConnected()
{
     setConnectionStatus( RemoteConnectionStatus::Connected );
}

void CSshRemoteServer::onHostDisconnected()
{
     setConnectionStatus( RemoteConnectionStatus::Disconnected );
}

void CSshRemoteServer::onHostError()
{
     setConnectionStatus( RemoteConnectionStatus::ConnectionError, ssh_connection_pointer_->errorString() );
}

void CSshRemoteServer::onNewStandardStreamData()
{
     const QString& command_name = sender()->objectName();
     const QSharedPointer<SshRemoteProcess>& ssh_remote_process_pointer = getSshRemoteProcess( command_name );
     if ( ssh_remote_process_pointer )
     {
          setNewRemoteCommandStream(
            command_name, ssh_remote_process_pointer->readAllStandardOutput(), RemoteCommand::Stream::Type::Standard );
     }
}

void CSshRemoteServer::onNewErrorStreamData()
{
     const QString& command_name = sender()->objectName();
     const QSharedPointer<SshRemoteProcess>& ssh_remote_process_pointer = getSshRemoteProcess( command_name );
     if ( ssh_remote_process_pointer )
     {
          setNewRemoteCommandStream(
            command_name, ssh_remote_process_pointer->readAllStandardError(), RemoteCommand::Stream::Type::Error );
     }
}

void CSshRemoteServer::onCommandStarted()
{
     const QString& command_name = sender()->objectName();
     setRemoteCommandStatus( command_name, RemoteCommand::Status::Started );
}

void CSshRemoteServer::onCommandWasExit( int exitStatus )
{
     const QString& command_name = sender()->objectName();
     const QSharedPointer<SshRemoteProcess>& ssh_remote_process_pointer = getSshRemoteProcess( command_name );
     if ( ssh_remote_process_pointer )
     {
          const SshRemoteProcess::ExitStatus status = static_cast<SshRemoteProcess::ExitStatus>( exitStatus );
          setRemoteCommandStatus( command_name, convertStatus( status ), ssh_remote_process_pointer->exitCode() );
     }
}

void CSshRemoteServer::killConnection()
{
     if ( ssh_connection_pointer_->state() == SshConnection::Connected
          && ( !kill_childs_process_pointer_ || !kill_childs_process_pointer_->isRunning() ) )
     {
          kill_childs_process_pointer_ = ssh_connection_pointer_->createRemoteProcess(
            qPrintable( QString( kill_command_global ).arg( force_kill_ ) ) );
          connect(
            kill_childs_process_pointer_.data(), &SshRemoteProcess::closed, this, &CSshRemoteServer::closeConnection );
          kill_childs_process_pointer_->start();
     }
}

void CSshRemoteServer::closeConnection()
{
     if ( ssh_connection_pointer_->state() == SshConnection::Connected )
     {
          if ( isExistsRunningRemoteCommands() )
          {
               for ( const QSharedPointer<SshRemoteProcess>& remote_process_pointer : ssh_processes_.values() )
               {
                    if ( remote_process_pointer->isRunning() )
                         remote_process_pointer->close();
               }
          }
          else
          {
               ssh_connection_pointer_->disconnectFromHost();
          }
     }
     else
     {
          ssh_connection_pointer_->disconnectFromHost();
          onHostDisconnected();
     }
}

QSharedPointer<SshRemoteProcess> CSshRemoteServer::getSshRemoteProcess( const QString& command_name ) const
{
     return ssh_processes_.value( command_name, nullptr );
}

QString userName()
{
     QString name = qgetenv( "USER" );
     if ( name.isEmpty() )
          name = qgetenv( "USERNAME" );
     return name;
}

QSsh::SshConnectionParameters getConnectionParameters( const DataSource& data_source )
{
     SshConnectionParameters result;
     SshConnectionOptions connection_options;
     connection_options &= 0;
     const QVariantMap& connection_parameters = data_source.connection_parameters;

     QString host = data_source.host;
     if ( host.isEmpty() )
     {
          if ( connection_parameters.contains( host_field_global ) )
          {
               host = connection_parameters[host_field_global].toString();
          }
          else
          {
               host = default_host_global;
          }
     }

     const QString& login = connection_parameters.value( login_field_global, userName() ).toString();
     const QString& password = connection_parameters[password_field_global].toString();
     const QString& key_file =
       password.isEmpty() ? connection_parameters.value( key_field_global, privateKeyPath() ).toString() : QString();

     const int timeout = connection_parameters.value( timeout_field_global, timeout_default_global ).toInt();
     const int port = connection_parameters.value( port_field_global, 22 ).toInt();
     const bool ignore_default_proxy = connection_parameters.value( ignore_default_proxy_global, true ).toBool();
     const bool enable_strict_conformance_checks =
       connection_parameters.value( enable_strict_conformance_checks_global, true ).toBool();
     if ( ignore_default_proxy )
          connection_options |= SshConnectionOption::SshIgnoreDefaultProxy;
     if ( enable_strict_conformance_checks )
          connection_options = SshConnectionOption::SshEnableStrictConformanceChecks;
     result.options = connection_options;

     result.setHost( host );
     result.setUserName( login );
     result.setPassword( password );
     result.privateKeyFile = key_file;
     result.authenticationType = key_file.isEmpty() ? SshConnectionParameters::AuthenticationTypePassword
                                                    : SshConnectionParameters::AuthenticationTypePublicKey;

     result.setPort( port );
     result.timeout = timeout;

     return result;
}

RemoteCommand::Status convertStatus( const SshRemoteProcess::ExitStatus exitStatus )
{
     RemoteCommand::Status remoteCommandStatus = RemoteCommand::Status::NotStarted;
     switch ( exitStatus )
     {
          case SshRemoteProcess::FailedToStart:
               remoteCommandStatus = RemoteCommand::Status::FailedToStart;
               break;
          case SshRemoteProcess::CrashExit:
               remoteCommandStatus = RemoteCommand::Status::CrashExit;
               break;
          case SshRemoteProcess::NormalExit:
               remoteCommandStatus = RemoteCommand::Status::NormalExit;
               break;
     }

     return remoteCommandStatus;
}

QString privateKeyPath()
{
     return QStandardPaths::displayName( QStandardPaths::HomeLocation ) + "/.ssh/id_rsa";
}

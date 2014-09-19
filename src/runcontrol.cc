/*
  Freebox QtCreator plugin for QML application development.

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not see
  http://www.gnu.org/licenses/lgpl-2.1.html.

  Copyright (c) 2014, Freebox SAS, See AUTHORS for details.
*/
#include <QDebug>
#include <QIcon>

#include <projectexplorer/target.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/devicesupport/idevice.h>

#include "constants.hh"
#include "configuration.hh"
#include "runcontrol.hh"
#include "remoterunconfiguration.hh"
#include "project.hh"
#include "device.hh"

namespace Freebox {

RunControl::RunControl(ProjectExplorer::RunConfiguration *rc,
                       ProjectExplorer::RunMode mode) :
    ProjectExplorer::RunControl(rc, mode),
    mDebug(false),
    mRunning(false)
{
    if (!rc || !rc->target())
        return;

    ProjectExplorer::Kit *kit = rc->target()->kit();
    Project *project = qobject_cast<Project *>(rc->target()->project());
    ProjectExplorer::IDevice::ConstPtr device =
            ProjectExplorer::DeviceKitInformation::device(kit);
    FreeboxDevice::ConstPtr fbxDevice = device.dynamicCast<const FreeboxDevice>();

    if (fbxDevice) {
        mAddress = fbxDevice->address();
        mQmlRemote.setAddress(mAddress);
    }

    if (project)
        mServer.setPath(project->projectDir().absolutePath());

    connect(&mQmlRemote, SIGNAL(started(uint16_t, uint16_t, uint16_t)),
            SLOT(emitRemoteStarted(uint16_t, uint16_t, uint16_t)));
    connect(&mQmlRemote, SIGNAL(failed(const QString &)),
            SLOT(emitRemoteFailed(const QString &)));
    connect(&mStdout, SIGNAL(readyRead()), SLOT(readOutput()));
    connect(&mStderr, SIGNAL(readyRead()), SLOT(readOutput()));
    connect(&mStdout, SIGNAL(disconnected()), SLOT(emitRemoteStopped()));
}

void RunControl::start()
{
    if (mRunning)
        return;

    RemoteRunConfiguration *fbxrc = qobject_cast<RemoteRunConfiguration *>(runConfiguration());
    ProjectExplorer::Kit *kit = fbxrc->target()->kit();
    ProjectExplorer::IDevice::ConstPtr device =
            ProjectExplorer::DeviceKitInformation::device(kit);
    FreeboxDevice::ConstPtr fbxDevice = device.dynamicCast<const FreeboxDevice>();

    QFileInfo fileInfo(fbxrc->mainScript());

    mServer.listen();
    mQmlRemote.start(QString::fromUtf8(fbxrc->id().name()), mServer.serverPort(), mDebug);
    mRunning = true;
    emit started();
}

ProjectExplorer::RunControl::StopResult RunControl::stop()
{
    if (mRunning) {
        unlinkOutput();
        mRunning = false;
        mServer.close();
        emit finished();
    }
    return ProjectExplorer::RunControl::StoppedSynchronously;
}

bool RunControl::isRunning() const
{
    return mRunning;
}

QIcon RunControl::icon() const
{
    return QIcon();
}

void RunControl::emitRemoteStarted(uint16_t port,
                                   uint16_t out,
                                   uint16_t err)
{
    emit remoteStarted(port);
    linkOutput(out, err);
}

void RunControl::emitRemoteFailed(const QString &err)
{
    appendMessage(err, Utils::StdErrFormat);
}

void RunControl::emitRemoteStopped()
{
    stop();
    emit remoteStopped();
}

void RunControl::linkOutput(uint16_t out, uint16_t err)
{
    mStdout.connectToHost(mAddress, out);
    mStderr.connectToHost(mAddress, err);
}

void RunControl::unlinkOutput()
{
    mStdout.disconnectFromHost();
    mStderr.disconnectFromHost();
}

void RunControl::readOutput()
{
    QTcpSocket *output = qobject_cast<QTcpSocket *>(sender());

    if (output) {
        QByteArray data = output->readAll();

        if (output == &mStdout) {
            appendMessage(QString::fromAscii(data), Utils::StdOutFormat);
        }
        else if (output == &mStderr) {
            appendMessage(QString::fromAscii(data), Utils::StdErrFormat);
        }
    }
}

} // namespace Freebox
/**
 * SPDX-FileCopyrightText: 2019 Matthijs Tijink <matthijstijink@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "multiplexchannel.h"
#include "core_debug.h"
#include "multiplexchannelstate.h"

MultiplexChannel::MultiplexChannel(QSharedPointer<MultiplexChannelState> state)
    : state{state}
{
    QIODevice::open(QIODevice::ReadWrite);

    connect(this, &QIODevice::aboutToClose, state.data(), &MultiplexChannelState::requestClose);
    connect(state.data(), &MultiplexChannelState::readyRead, this, &QIODevice::readyRead);
    connect(state.data(), &MultiplexChannelState::bytesWritten, this, &QIODevice::bytesWritten);
    connect(state.data(), &MultiplexChannelState::disconnected, this, &MultiplexChannel::disconnect);
}

MultiplexChannel::~MultiplexChannel()
{
}

bool MultiplexChannel::atEnd() const
{
    return !isOpen() || (!state->connected && state->read_buffer.isEmpty());
}

void MultiplexChannel::disconnect()
{
    state->connected = false;
    setOpenMode(QIODevice::ReadOnly);
    Q_EMIT state->readyRead();
    Q_EMIT state->requestClose();
    if (state->read_buffer.isEmpty()) {
        close();
    }
}

qsizetype MultiplexChannel::bytesAvailable() const
{
    return state->read_buffer.size() + QIODevice::bytesAvailable();
}

qsizetype MultiplexChannel::bytesToWrite() const
{
    return state->write_buffer.size() + QIODevice::bytesToWrite();
}

qsizetype MultiplexChannel::readData(char *data, qsizetype maxlen)
{
    if (maxlen <= state->read_buffer.size() || state->read_buffer.size() > 0) {
        const qsizetype num_to_read = std::min(maxlen, state->read_buffer.size());
        std::memcpy(data, state->read_buffer.data(), num_to_read);
        state->read_buffer.remove(0, num_to_read);
        Q_EMIT state->readAvailable();
        if (!state->connected && state->read_buffer.isEmpty()) {
            close();
        }
        return num_to_read;
    } else if (isOpen() && state->connected) {
        if (state->requestedReadAmount < BUFFER_SIZE) {
            Q_EMIT state->readAvailable();
        }
        return 0;
    } else {
        close();
        return -1;
    }
}

qsizetype MultiplexChannel::writeData(const char *data, qsizetype len)
{
    state->write_buffer.append(data, len);
    Q_EMIT state->writeAvailable();
    return len;
}

bool MultiplexChannel::canReadLine() const
{
    return isReadable() && (QIODevice::canReadLine() || state->read_buffer.contains('\n'));
}

bool MultiplexChannel::isSequential() const
{
    return true;
}

#include "moc_multiplexchannel.cpp"

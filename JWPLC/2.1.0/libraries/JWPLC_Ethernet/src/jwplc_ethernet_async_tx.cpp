#include "jwplc_ethernet_async_tx.h"

#include <SPI.h>
#include "utility/w5100.h"

JWPLC_EthernetAsyncTx::JWPLC_EthernetAsyncTx()
    : _socket(MAX_SOCK_NUM),
      _pending(false)
{
}

uint16_t JWPLC_EthernetAsyncTx::readTxFreeStable(uint8_t socket)
{
    uint16_t previous = W5100.readSnTX_FSR(socket);

    while (true)
    {
        const uint16_t current = W5100.readSnTX_FSR(socket);
        if (current == previous)
        {
            return current;
        }
        previous = current;
    }
}

void JWPLC_EthernetAsyncTx::writeTxData(
    uint8_t socket,
    const uint8_t *data,
    uint16_t length)
{
    uint16_t ptr = W5100.readSnTX_WR(socket);
    const uint16_t offset = ptr & W5100.SMASK;
    const uint16_t destination = offset + W5100.SBASE(socket);

    if (W5100.hasOffsetAddressMapping() ||
        (uint16_t)(offset + length) <= W5100.SSIZE)
    {
        W5100.write(destination, data, length);
    }
    else
    {
        const uint16_t firstPart = W5100.SSIZE - offset;
        W5100.write(destination, data, firstPart);
        W5100.write(W5100.SBASE(socket),
                    data + firstPart,
                    length - firstPart);
    }

    ptr = (uint16_t)(ptr + length);
    W5100.writeSnTX_WR(socket, ptr);
}

int JWPLC_EthernetAsyncTx::begin(
    EthernetClient &client,
    const uint8_t *data,
    uint16_t length)
{
    if (_pending)
    {
        return 0;
    }

    if (length == 0)
    {
        reset();
        return 1;
    }

    if (data == nullptr || length > W5100.SSIZE)
    {
        reset();
        return -1;
    }

    const uint8_t socket = client.getSocketNumber();
    if (socket >= MAX_SOCK_NUM)
    {
        reset();
        return -1;
    }

    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

    const uint8_t status = W5100.readSnSR(socket);
    if (status != SnSR::ESTABLISHED && status != SnSR::CLOSE_WAIT)
    {
        SPI.endTransaction();
        reset();
        return -1;
    }

    const uint16_t freeBytes = readTxFreeStable(socket);
    if (freeBytes < length)
    {
        SPI.endTransaction();
        return 0;
    }

    // Elimina flags de un SEND anterior antes de disparar uno nuevo.
    W5100.writeSnIR(socket, (uint8_t)(SnIR::SEND_OK | SnIR::TIMEOUT));

    writeTxData(socket, data, length);
    W5100.execCmdSn(socket, Sock_SEND);

    SPI.endTransaction();

    _socket = socket;
    _pending = true;
    return 0;
}

int JWPLC_EthernetAsyncTx::poll(EthernetClient &client)
{
    if (!_pending)
    {
        return -1;
    }

    const uint8_t socket = client.getSocketNumber();
    if (socket >= MAX_SOCK_NUM || socket != _socket)
    {
        reset();
        return -1;
    }

    SPI.beginTransaction(SPI_ETHERNET_SETTINGS);

    const uint8_t interruptFlags = W5100.readSnIR(socket);
    const uint8_t status = W5100.readSnSR(socket);

    if ((interruptFlags & SnIR::SEND_OK) != 0)
    {
        W5100.writeSnIR(socket, SnIR::SEND_OK);
        SPI.endTransaction();
        reset();
        return 1;
    }

    if ((interruptFlags & SnIR::TIMEOUT) != 0)
    {
        W5100.writeSnIR(socket, SnIR::TIMEOUT);
        SPI.endTransaction();
        reset();
        return -1;
    }

    SPI.endTransaction();

    if (status == SnSR::CLOSED ||
        status == SnSR::FIN_WAIT ||
        status == SnSR::CLOSING)
    {
        reset();
        return -1;
    }

    return 0;
}

bool JWPLC_EthernetAsyncTx::inProgress() const
{
    return _pending;
}

void JWPLC_EthernetAsyncTx::reset()
{
    _socket = MAX_SOCK_NUM;
    _pending = false;
}

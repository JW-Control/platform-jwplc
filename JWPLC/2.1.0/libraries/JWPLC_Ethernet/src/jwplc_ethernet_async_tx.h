#ifndef JWPLC_ETHERNET_ASYNC_TX_H
#define JWPLC_ETHERNET_ASYNC_TX_H

#include <Arduino.h>
#include "JWPLC_W5x00_Ethernet.h"

// Helper interno JWPLC para iniciar un SEND TCP y consultar su finalización
// sin esperar síncronamente a SEND_OK. El caller debe proteger cada begin/poll
// con el mutex SPI compartido JWPLC.
class JWPLC_EthernetAsyncTx
{
public:
    JWPLC_EthernetAsyncTx();

    // -1 = error, 0 = SEND iniciado/pending, 1 = nada que enviar (len=0).
    int begin(EthernetClient &client, const uint8_t *data, uint16_t length);

    // -1 = error/timeout/socket cerrado, 0 = pending, 1 = SEND_OK.
    int poll(EthernetClient &client);

    bool inProgress() const;
    void reset();

private:
    uint8_t _socket;
    bool _pending;

    static uint16_t readTxFreeStable(uint8_t socket);
    static void writeTxData(uint8_t socket,
                            const uint8_t *data,
                            uint16_t length);
};

#endif // JWPLC_ETHERNET_ASYNC_TX_H

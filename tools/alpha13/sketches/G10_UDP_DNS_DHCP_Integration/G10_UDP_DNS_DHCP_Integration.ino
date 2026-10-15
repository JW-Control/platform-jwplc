/*
 * Alpha13 G10-I — N2 integración de compilación UDP/DNS/DHCP.
 * No ejecuta tráfico automáticamente. Valida símbolos y enlace del backend real.
 * N1 verifica las carreras SPI_BUSY; este sketch NO certifica corrección B03.
 * El autoload habitual del package permanece intacto.
 */
#include <JWPLC_W5x00_Ethernet.h>
#include <Dns.h>

static EthernetUDP gUdp;
static EthernetClient gTcp;
static EthernetServer gServer(502);
static DNSClient gDns;
static IPAddress gResult;

static void exerciseApiOnDemand() {
  // El cuerpo es alcanzable desde loop() solo si una persona habilita
  // explícitamente el disparador por Serial; CI únicamente compila/enlaza.
  gDns.begin(IPAddress(192, 168, 1, 1));
  int state = gDns.beginResolveAsync("example.com", gResult, 1000);
  if (state == 0) {
    (void)gDns.pollResolveAsync();
  }
  (void)gDns.resolveAsyncInProgress();
  gDns.cancelResolveAsync();

  if (gUdp.begin(32001)) {
    (void)gUdp.beginPacket(IPAddress(192, 168, 1, 1), 32002);
    (void)gUdp.write(static_cast<uint8_t>(0x5A));
    (void)gUdp.beginEndPacketAsync();
    if (gUdp.endPacketAsyncInProgress()) {
      (void)gUdp.pollEndPacketAsync();
    }
    gUdp.cancelEndPacketAsync();
    (void)gUdp.parsePacket();
    gUdp.stop();
  }

  // Link the candidate Client RX and TX bodies for B02 source provenance.
  uint8_t tcpByte = 0;
  (void)gTcp.available();
  (void)gTcp.read(&tcpByte, 1);
  (void)gTcp.read();
  (void)gTcp.peek();
  (void)gTcp.jwplcReadTcpFastDeferred(&tcpByte, 1);
  (void)gTcp.jwplcCommitRxFast();
  (void)gTcp.write(&tcpByte, 1);
  (void)gTcp.beginWriteAsync(&tcpByte, 1);
  (void)gTcp.pollWriteAsync();
  gTcp.cancelWriteAsync();
  (void)gTcp.availableForWrite();
  (void)gTcp.beginFlushAsync();
  (void)gTcp.pollFlushAsync();
  gTcp.cancelFlushAsync();
  (void)gTcp.localPort();
  (void)gTcp.remoteIP();
  (void)gTcp.remotePort();
  // B01/B04: link the internal tri-state probe and durable close paths.
  uint8_t tcpStatus = 0;
  (void)gTcp.jwplcProbeSocketStatus(tcpStatus);
  gTcp.cancelConnectAsync();
  gTcp.cancelStopAsync();
  gTcp.stop();
  // Reach Server listener and broadcast bodies for B04/B05 ELF provenance.
  gServer.begin();
  (void)static_cast<bool>(gServer);
  (void)gServer.write(&tcpByte, 1);
}

void setup() {
  Serial.begin(115200);
}

void loop() {
  if (Serial.available() && Serial.read() == 'T') {
    exerciseApiOnDemand();
  }
}

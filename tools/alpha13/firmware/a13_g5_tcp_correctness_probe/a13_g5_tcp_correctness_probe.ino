#include <Arduino.h>
#include <JWPLC_Ethernet.h>
#include <jwplc_spi_bus.h>

static const char G5_TOKEN[] = "__G5_TOKEN__";
static constexpr uint16_t PC_PORT = 5008;
static constexpr uint16_t JWPLC_SERVER_PORT = 5018;
static constexpr size_t TX_BYTES = 5000;
static constexpr size_t RX_BYTES = 128;
static constexpr size_t SERVER_BYTES = 64;
static EthernetServer g_server(JWPLC_SERVER_PORT);
static bool g_serverReady = false;
static bool g_done = false;
static size_t g_noClientWritten = (size_t)-1;
static uint8_t g_tx[TX_BYTES];

static bool lockSPI() {
  if (!jwplcSPI_acquire(150)) return false;
  jwplcSPI_deselectAll();
  return true;
}
static void unlockSPI() { jwplcSPI_release(); }

static bool parseIPv4(const String &str, IPAddress &ip) {
  unsigned a,b,c,d;
  if (sscanf(str.c_str(), "%u.%u.%u.%u", &a,&b,&c,&d) != 4 ||
      a>255 || b>255 || c>255 || d>255) return false;
  ip=IPAddress((uint8_t)a,(uint8_t)b,(uint8_t)c,(uint8_t)d);
  return true;
}

static void runCase(IPAddress host) {
  int errors=0;
  EthernetClient client;
  client.setConnectionTimeout(3500);

  bool connected=false;
  if (lockSPI()) {
    connected=(client.connect(host,PC_PORT)==1);
    unlockSPI();
  }
  if (!connected) ++errors;

  size_t bytesWritten=0;
  if (connected && lockSPI()) {
    bytesWritten=client.write(g_tx,TX_BYTES);
    unlockSPI();
  }
  if (bytesWritten!=TX_BYTES) ++errors;

  // El PC devuelve 128 bytes después de verificar los 5000 de TX.
  int bytesRead=-1;
  uint8_t *rx=(uint8_t *)malloc(32768);
  if (!rx) ++errors;
  if (connected && rx) {
    uint32_t started=millis();
    while ((uint32_t)(millis()-started)<4000) {
      if (lockSPI()) {
        const int available=client.available();
        if (available>=(int)RX_BYTES) {
          // Contrato A13-G5: size_t 32768 no debe convertirse a int16_t negativo.
          bytesRead=client.read(rx,32768);
          unlockSPI();
          break;
        }
        unlockSPI();
      }
      delay(3);
    }
    if (bytesRead!=(int)RX_BYTES) ++errors;
    if (bytesRead>0) {
      for (int i=0;i<bytesRead;i++) {
        if (rx[i]!=(uint8_t)((i*7+3)&255)) {
          ++errors; break;
        }
      }
    }
  }
  free(rx);

  if (connected && lockSPI()) {
    client.stop();
    unlockSPI();
  }

  // El cliente del PC ya conectó al listener JWPLC al recibir G5_READY.
  static uint8_t serverPayload[SERVER_BYTES];
  for (size_t i=0;i<SERVER_BYTES;i++) serverPayload[i]=(uint8_t)((i*11+5)&255);
  size_t serverWritten=0;
  if (lockSPI()) {
    serverWritten=g_server.write(serverPayload,SERVER_BYTES);
    unlockSPI();
  }
  if (serverWritten!=SERVER_BYTES) ++errors;

  Serial.print("G5_NO_CLIENT_BYTES="); Serial.println((unsigned long)g_noClientWritten);
  if (g_noClientWritten != 0) ++errors;
  Serial.print("G5_TX_BYTES="); Serial.println((unsigned long)bytesWritten);
  Serial.print("G5_RX_BYTES="); Serial.println(bytesRead);
  Serial.print("G5_SERVER_BYTES="); Serial.println((unsigned long)serverWritten);
  Serial.print("G5_ERRORS="); Serial.println(errors);
  Serial.print("G5_RESULT="); Serial.println(errors==0?"PASS":"FAIL");
  Serial.print("G5_DONE="); Serial.println(G5_TOKEN);
  Serial.flush();
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(500);
  for (size_t i=0;i<TX_BYTES;i++) g_tx[i]=(uint8_t)(i&255);
}
void loop() {
  if (g_done) { delay(50); return; }
  if (!JWPLC_Ethernet.isReady()) { delay(100); return; }

  if (!g_serverReady && lockSPI()) {
    g_server.begin();
    // Antes de anunciar READY, ningún cliente del PC se ha conectado.
    const uint8_t single = 0x55;
    g_noClientWritten = g_server.write(&single, 1);
    unlockSPI();
    g_serverReady=true;
  }
  static uint32_t last=0;
  if (g_serverReady && (uint32_t)(millis()-last)>500) {
    last=millis();
    Serial.print("G5_READY="); Serial.print(G5_TOKEN);
    Serial.print(" IP="); Serial.println(JWPLC_Ethernet.localIP());
  }
  if (Serial.available()) {
    String cmd=Serial.readStringUntil('\n');
    cmd.trim();
    const String prefix=String("G5_RUN:")+G5_TOKEN+" ";
    if (cmd.startsWith(prefix) && g_serverReady) {
      IPAddress host;
      g_done=true; // una sola ejecución por sketch físico
      if (!parseIPv4(cmd.substring(prefix.length()),host)) {
        Serial.println("G5_RESULT=FAIL");
        Serial.println("G5_ERRORS=1");
        Serial.print("G5_DONE="); Serial.println(G5_TOKEN);
        return;
      }
      runCase(host);
    }
  }
  delay(3);
}

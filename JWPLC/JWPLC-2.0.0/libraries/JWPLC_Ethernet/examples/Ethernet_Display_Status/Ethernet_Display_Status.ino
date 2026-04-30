/*
  Ethernet_Display_Status

  Prueba de integración básica Ethernet + Display.

  La pantalla IDLE usa el indicador ETH para reflejar link/estado Ethernet.

  Valida:
  - Arranque rápido sin cable RJ45.
  - Detección de W5500.
  - Detección de Link OFF.
  - Reintento automático cuando se conecta el cable después del arranque.
  - Indicador ETH en pantalla IDLE.
*/

#include <JWPLC_Ethernet.h>
#include <JWPLC_Display.h>

bool ethernetStarted = false;
bool displayConfigured = false;

unsigned long lastCheckMs = 0;
unsigned long lastRetryMs = 0;

const unsigned long CHECK_PERIOD_MS = 1000;
const unsigned long RETRY_PERIOD_MS = 5000;

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println();
  Serial.println("JWPLC_Ethernet + Display status test");

  ethernetStarted = JWPLC_Ethernet.begin();
  JWPLC_Ethernet.printStatus(Serial);
}

void loop() {
  if (ethernetStarted) {
    JWPLC_Ethernet.maintain();
  }

  if (!displayConfigured && JWPLC_Display.isReady()) {
    displayConfigured = true;

    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
    JWPLC_Display.setIdleTimeoutMs(8000);

    Serial.println("Display ready");
  }

  unsigned long now = millis();

  // Reintento automático:
  // Si arrancó sin cable o falló DHCP, vuelve a probar cada cierto tiempo.
  if (JWPLC_Ethernet.isEnabled() && !ethernetStarted && (now - lastRetryMs >= RETRY_PERIOD_MS)) {
    lastRetryMs = now;

    Serial.println("Retry Ethernet begin...");
    ethernetStarted = JWPLC_Ethernet.begin();

    Serial.print("Retry result: ");
    Serial.println(ethernetStarted ? "OK" : JWPLC_Ethernet.statusString());
  }

  if (now - lastCheckMs >= CHECK_PERIOD_MS) {
    lastCheckMs = now;

    bool ethOk = ethernetStarted && JWPLC_Ethernet.linkUp();

    if (displayConfigured) {
      JWPLC_Display.setEthLed(ethOk);
      JWPLC_Display.setErrLed(!ethOk);
    }

    Serial.print("ETH: ");
    Serial.print(JWPLC_Ethernet.statusString());

    Serial.print(" | Link: ");
    Serial.print(JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");

    Serial.print(" | IP: ");
    Serial.println(JWPLC_Ethernet.localIP());

    // Si se perdió el link físico luego de estar iniciado,
    // no borramos la IP, pero marcamos el estado visual como no OK.
    // Cuando vuelva el link, la librería reportará OK nuevamente.
  }
}

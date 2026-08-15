#include <SPI.h>
#include <JWPLC_Ethernet.h> // Incluimos para desactivar el DHCP de fondo
#include "src/JWPLC_W5500.h"
#include "src/JWPLC_W5500_Socket.h"

// IP estática en la misma subred que tu PC (192.168.2.X)
uint8_t ip[] = {192, 168, 2, 100};
uint8_t gateway[] = {192, 168, 2, 1};
uint8_t subnet[] = {255, 255, 255, 0};
uint8_t mac[] = {0x02, 0x4A, 0x57, 0x11, 0x22, 0x33}; // Dummy MAC

// Socket 0 para TCP (Puerto 5001)
JWPLC_W5500_Socket tcpSock(0);

int currentMode = 0; // 0 = Menú, 1 = TCP Throughput

void printMenu() {
  Serial.println("\n=============================================");
  Serial.println("   JWPLC_W5500 (RAW DRIVER) BENCHMARK        ");
  Serial.println("=============================================");
  Serial.print("Hardware W5500 Presente: ");
  Serial.println(JWPLC_W5500.hardwarePresent() ? "SI" : "NO");
  Serial.print("Link Físico: ");
  Serial.println(JWPLC_W5500.linkUp() ? "UP" : "DOWN");
  Serial.println("Selecciona una prueba enviando el numero por Serial:");
  Serial.println(" [1] Test de Throughput RAW TCP (Puerto 5001)");
  Serial.println("=============================================\n");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  // TRUCO: Configurar JWPLC_Ethernet nativo con IP estática
  // Esto evita que la tarea de fondo (jwplcSystemTask) intente negociar DHCP 
  // eternamente y bloquee el Mutex SPI por 5 segundos.
  Serial.println("\nDesactivando background DHCP...");
  IPAddress ipAddr(ip[0], ip[1], ip[2], ip[3]);
  IPAddress gwAddr(gateway[0], gateway[1], gateway[2], gateway[3]);
  IPAddress snAddr(subnet[0], subnet[1], subnet[2], subnet[3]);
  JWPLC_Ethernet.setStaticIP(ipAddr, gwAddr, gwAddr, snAddr);
  JWPLC_Ethernet.begin();

  Serial.println("Configurando RAW JWPLC_W5500...");
  
  if (!JWPLC_W5500.begin(5)) { // CS pin 5
    Serial.println("ERROR: No se encontró W5500. Verifica el hardware.");
  }
  
  // Configurar IP, Gateway, Subnet y MAC manualmente (Capa Baja)
  JWPLC_W5500.writeBlock(W5500_GAR, W5500_COMMON_REG, gateway, 4);
  JWPLC_W5500.writeBlock(W5500_SUBR, W5500_COMMON_REG, subnet, 4);
  JWPLC_W5500.writeBlock(W5500_SHAR, W5500_COMMON_REG, mac, 6);
  JWPLC_W5500.writeBlock(W5500_SIPR, W5500_COMMON_REG, ip, 4);
  
  // Ajuste de Buffers (4 = 4KB por socket, 2 = 2KB, etc.) -> Baseline: 2KB
  JWPLC_W5500.setBuffers(4, 4); 
  
  // Inicializar Socket 0 para TCP en modo escucha
  tcpSock.begin(W5500_Sn_MR_TCP, 5001);
  tcpSock.sendCommand(W5500_CR_LISTEN);

  delay(500);
  printMenu();
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '1') {
      currentMode = 1;
      Serial.println("Cambiando a modo: RAW TCP Throughput");
    }
  }

  if (currentMode == 1) {
    runTCPTest();
  }
  
  // Rendir tiempo al SO (RTOS) para evitar la inanición de otras tareas
  delay(1);
}

// ------------------------------------------------------------------
// 1. RAW THROUGHPUT TCP ÚTIL (SOCKET 0)
// ------------------------------------------------------------------
void runTCPTest() {
  uint8_t status = tcpSock.getStatus();
  
  static uint8_t lastStatus = 0xFF;
  if (status != lastStatus) {
    Serial.print("Socket Status Changed: 0x");
    Serial.println(status, HEX);
    lastStatus = status;
  }
  
  static unsigned long startTime = 0;
  static unsigned long bytesRead = 0;
  static bool connected = false;

  if (status == W5500_SOCK_ESTABLISHED) {
    if (!connected) {
      Serial.println("Cliente TCP RAW Conectado. Enviando/Recibiendo a máxima velocidad...");
      connected = true;
      startTime = millis();
      bytesRead = 0;
    }

    JWPLC_W5500.acquireBus(); // Bloquear mutex una sola vez para toda la lectura
    size_t avail = tcpSock.getRXReceivedSize();
    if (avail > 0) {
      uint8_t buffer[2048]; // Nuestro RX buffer interno
      size_t toRead = min(avail, (size_t)2048);
      tcpSock.read(buffer, toRead);
      bytesRead += toRead;
    }
    JWPLC_W5500.releaseBus(); // Liberar mutex
    
    unsigned long elapsed = millis() - startTime;
    if (elapsed >= 5000) {
      float mbps = (bytesRead * 8.0) / (elapsed / 1000.0) / 1000000.0;
      Serial.print("RAW TCP Rx Throughput: ");
      Serial.print(mbps, 3);
      Serial.println(" Mbps (Mbit/s)");
      startTime = millis();
      bytesRead = 0;
    }
  } 
  else if (status == W5500_SOCK_CLOSE_WAIT) {
    tcpSock.disconnect();
    Serial.println("Cliente TCP RAW Desconectado (CLOSE_WAIT).");
    connected = false;
  }
  else if (status == W5500_SOCK_CLOSED) {
    // Si se cierra, volver a escuchar
    tcpSock.begin(W5500_Sn_MR_TCP, 5001);
    tcpSock.sendCommand(W5500_CR_LISTEN);
    if(connected){
      Serial.println("Cliente TCP RAW Desconectado (CLOSED).");
      connected = false;
    }
  }
}

/*
  Alpha13 G10-R R1/R2/R4: banco aislado. COM4 = Ethernet/SD/RTC/FRAM/RTU Master.
  COM14 = Slave ID 2 opcional (g8_p4_slave). Nada de salidas Q,
  escrituras SD/FRAM, reset W5500 GPIO4 ni desactivacion de autoload.
  Sketch experimental sin actuadores ni escrituras SD/FRAM. R3 expone
  snapshots READ-ONLY owner/epoch/pending, NO valida fault injection.
  Compilar SOLO con overlay G10-I + telemetria de laboratorio G10-R.
*/
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_ModbusTCP.h>
#include <JWPLC_ModbusRTU.h>
#include <JWPLC_RS485.h>
#include <JWPLC_W5500_SafeAccess.h>
#include <string.h>
#include <esp_system.h>

static uint16_t registers[4] = {0x1234, 0x2345, 0x3456, 0x4567};
static uint16_t rtuValues[4] = {};
static EthernetUDP udp;
static bool udpReady=false, udpPending=false, active=false, useRtu=false;
static bool rtuReady=false;
static uint32_t bootNonce=0;
static uint32_t startMs=0, lastSample=0, lastReady=0, lastRtu=0, lastReport=0;
static uint32_t hb=0, sdOk=0, sdFail=0, udpRx=0, udpTx=0;
static uint32_t rtuOk=0, rtuFail=0, btnEvents=0, framReads=0;
static char input[90], token[25]={};
static size_t used=0;
static uint8_t packet[96];

static void recordSD() {
  if (!JWPLC_SD.isCardPresent() || !JWPLC_SD.isReady()) { ++sdFail; return; }
  // Open "/" for reading only. Never write, create, or erase a file.
  JWPLCFile root = JWPLC_SD.open("/", FILE_READ);
  if (!root) { ++sdFail; return; }
  if (root.isDirectory()) ++sdOk; else ++sdFail;
  root.close();
}

static void handleCommand(const char* msg) {
  static const char prefix[]="G10R_START:";
  if (strncmp(msg,"G10R_STOP:",10)==0) {
    if (active && strlen(msg+10)==24 && strcmp(msg+10,token)==0) {
      active=false;useRtu=false;
      Serial.print("G10R_STOPPED:");Serial.println(token);
    }
    return;
  }
  // R3: telemetria puntual, solo lectura del overlay experimental G10-R.
  // No provoca SPI_BUSY, no cierra sockets, no altera epoch/lease.
  static const char snapshotPrefix[]="G10R_SOCKETS:";
  if (strncmp(msg,snapshotPrefix,sizeof(snapshotPrefix)-1)==0) {
    const char* requested=msg+sizeof(snapshotPrefix)-1;
    if (!active || strlen(requested)!=24 || strcmp(requested,token)!=0) return;
    jwplc_w5500_candidate::LabSocketSnapshot slots[8];
    const auto status=jwplc_w5500_candidate::labCaptureSockets(slots,8,1);
    if (status!=jwplc_w5500_candidate::LabCaptureOutcome::Captured) {
      Serial.print("G10R_SOCK_STATUS:");Serial.print(token);
      Serial.print(" CODE=");Serial.println((int)status);
      return; // 2 BUS_BUSY nunca es un CLOSED confirmado.
    }
    for (uint8_t i=0;i<8;++i) {
      Serial.print("G10R_SOCK:");Serial.print(token);
      Serial.print(" SID=");Serial.print(slots[i].socketIndex);
      Serial.print(" EPOCH=");Serial.print(slots[i].epoch);
      Serial.print(" ALLOC=");Serial.print(slots[i].allocation);
      Serial.print(" OWNER=");Serial.print(slots[i].ownerCookie);
      Serial.print(" PENDING=");Serial.print(slots[i].pendingCloseAllocation);
      Serial.print(" VALID=");Serial.println(slots[i].valid?1:0);
    }
    return;
  }
  if (active || strncmp(msg,prefix,sizeof(prefix)-1)!=0) return;
  const char* p=msg+sizeof(prefix)-1;
  if (strlen(p)!=30 || p[24]!=':' ||
      strncmp(p+25,"RTU=",4)!=0 || (p[29]!='0' && p[29]!='1')) return;
  for (int i=0;i<24;++i)
    if (!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f'))) return;
  memcpy(token,p,24);token[24]='\0';
  useRtu=(p[29]=='1');
  hb=sdOk=sdFail=udpRx=udpTx=rtuOk=rtuFail=btnEvents=framReads=0;
  lastSample=lastRtu=lastReport=millis();
  active=true;startMs=millis();
  JWPLC_ModbusRTU.resetStats();
  JWPLC_Buttons.clearPendingInput();
  Serial.print("G10R_STARTED:");Serial.print(token);
  Serial.print(" RTU=");Serial.println(useRtu?1:0);
}

static void handleUsb() {
  while (Serial.available()) {
    int c=Serial.read();
    if (c=='\r'||c=='\n') {
      if (used) {input[used]=0;handleCommand(input);used=0;}
    } else if (c>=32 && c<=126 && used+1<sizeof(input)) {
      input[used++]=(char)c;
    } else if (used+1>=sizeof(input)) used=0;
  }
}

static void serviceUDP() {
  if (!JWPLC_Ethernet.isReady()) return;
  if (!udpReady) {
    jwplc_w5500_candidate::BusScope bus(20);
    if (bus) udpReady=(udp.begin(15020)==1);
    return;
  }
  if (udpPending) {
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) return;
    const int done=udp.pollEndPacketAsync();
    if (done!=0) {udpPending=false;if (done==1) ++udpTx;}
    return;
  }
  jwplc_w5500_candidate::BusScope bus(20);
  if (!bus) return;
  int available=udp.parsePacket();
  if (available<=0) return;
  if (available>=(int)sizeof(packet)) { // Drain without echoing malformed traffic.
    while (udp.available()>0) (void)udp.read();
    return;
  }
  int n=udp.read(packet,(size_t)available);
  if (n!=available || n<1) return;
  ++udpRx;
  IPAddress peer=udp.remoteIP();
  uint16_t port=udp.remotePort();
  if (udp.beginPacket(peer,port)!=1) return;
  if (udp.write(packet,(size_t)n)!=(size_t)n) return;
  const int started=udp.beginEndPacketAsync();
  if (started==0) udpPending=true;
  else if (started==1) ++udpTx;
}

static void serviceRTU(uint32_t now) {
  if (!useRtu || !rtuReady) return;
  JWPLC_ModbusRTU.task();
  if (JWPLC_ModbusRTU.masterDone()) {
    const bool match=JWPLC_ModbusRTU.masterSucceeded() &&
      rtuValues[0]==0x1234 && rtuValues[1]==0x2345 &&
      rtuValues[2]==0x3456 && rtuValues[3]==0x4567;
    if (match) ++rtuOk; else ++rtuFail;
    JWPLC_ModbusRTU.clearMasterResult();
    lastRtu=now;
  }
  if (!JWPLC_ModbusRTU.masterBusy() &&
      !JWPLC_ModbusRTU.masterDone() &&
      (uint32_t)(now-lastRtu)>=800) {
    if (!JWPLC_ModbusRTU.requestReadHoldingRegisters(
          2,0,4,rtuValues,1200)) ++rtuFail;
    lastRtu=now;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("G10R_PROTOCOL:R1R2R4_R3SNAPSHOT_V1");
  bootNonce=esp_random();
  Serial.print("G10R_BOOT:NONCE=");Serial.println(bootNonce,HEX);
  // El autoload normal inicia Ethernet y el resto de perifericos.
  JWPLC_Ethernet.useDHCP();
  JWPLC_ModbusTCP.setHoldingRegisters(registers,4);
  const bool tcpConfigured=JWPLC_ModbusTCP.beginServer(1,502);
  rtuReady=JWPLC_ModbusRTU.begin(247,115200,SERIAL_8N1);
  Serial.print("G10R_BOOT:TCP=");
  Serial.print(tcpConfigured?1:0);
  Serial.print(" RTU=");
  Serial.println(rtuReady?1:0);
}

void loop() {
  handleUsb();
  uint32_t now=millis();
  // El core mantiene autoload. Este task no llama service() de Ethernet.
  serviceUDP();
  serviceRTU(now);
  if (!active) {
    if ((uint32_t)(now-lastReady)>=1000) {
      lastReady=now;
      Serial.print("G10R_READY:IP=");
      Serial.print(JWPLC_Ethernet.isReady()?JWPLC_Ethernet.localIP():IPAddress(0,0,0,0));
      Serial.print(" TCP=");Serial.print(JWPLC_ModbusTCP.serverReady()?1:0);
      Serial.print(" UDP=");Serial.print(udpReady?1:0);
      Serial.print(" RTU=");Serial.print(rtuReady?1:0);
      Serial.print(" BOOT=");Serial.print(bootNonce,HEX);
      Serial.print(" LINK=");Serial.print((int)JWPLC_Ethernet.linkStatus());
      Serial.println(" VER=R1R2R4_R3SNAPSHOT_V1");
    }
    return;
  }
  if ((uint32_t)(now-lastSample)>=700) {
    lastSample=now;
    recordSD();
    if (JWPLC_FRAM.size()>0) {
      // Lectura unica de direccion cero; contenido no se imprime ni modifica.
      volatile uint8_t sample=JWPLC_FRAM.read8(0);
      (void)sample;
      ++framReads;
    }
    static const uint8_t keys[]={BTN_LEFT,BTN_UP,BTN_RIGHT,BTN_ESC,BTN_OK,BTN_DOWN};
    for (uint8_t key:keys) {
      if (JWPLC_Buttons.pressed(key)) ++btnEvents;
      if (JWPLC_Buttons.released(key)) ++btnEvents;
    }
  }
  if ((uint32_t)(now-lastReport)>=1000) {
    lastReport=now;
    ++hb;
    Serial.print("G10R_HEARTBEAT:");
    Serial.print(token);
    Serial.print(" HB=");Serial.print(hb);
    Serial.print(" BOOT=");Serial.print(bootNonce,HEX);
    Serial.print(" IP=");
    Serial.print(JWPLC_Ethernet.isReady()?JWPLC_Ethernet.localIP():IPAddress(0,0,0,0));
    // LinkON=1, LinkOFF=2, Unknown=0 (puede ser SPI_BUSY).
    // Unknown nunca equivale a LinkOFF físico.
    Serial.print(" LINK=");Serial.print((int)JWPLC_Ethernet.linkStatus());
    Serial.print(" ETH=");Serial.print(JWPLC_Ethernet.isReady()?1:0);
    Serial.print(" TCP=");Serial.print(JWPLC_ModbusTCP.serverReady()?1:0);
    Serial.print(" SD_OK=");Serial.print(sdOk);
    Serial.print(" SD_FAIL=");Serial.print(sdFail);
    Serial.print(" RTC=");Serial.print(JWPLC_Time.valid()?1:0);
    Serial.print(" FRAM=");Serial.print(framReads);
    Serial.print(" UDP_RX=");Serial.print(udpRx);
    Serial.print(" UDP_TX=");Serial.print(udpTx);
    Serial.print(" RTU_OK=");Serial.print(rtuOk);
    Serial.print(" RTU_FAIL=");Serial.print(rtuFail);
    Serial.print(" BTN_EVENTS=");Serial.println(btnEvents);
  }
}

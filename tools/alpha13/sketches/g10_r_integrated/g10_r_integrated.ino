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
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

// R3: sonda UDP exclusivamente de banco, armada por comando con token.
// El task solo sostiene el mismo mutex W5500 <=80 ms; nunca toca SPI,
// GPIO4, SD, FRAM, Q ni servicios en segundo plano.
static EthernetUDP r3ProbeUdp;
static std::atomic<uint8_t> r3HoldState{0}; // 0 idle; 1 held; 2 released; 3 lock denied
static bool r3Armed=false, r3CloseAttempted=false, r3ClosedWhileHeld=false;
static uint32_t r3ReleasedAt=0;
static bool r3FinishPrinted=false;

static void r3HoldW5500(void*) {
  {
    jwplc_w5500_candidate::BusScope scope(0);
    if (!scope) {
      r3HoldState.store(3, std::memory_order_release);
      vTaskDelete(nullptr);
      return;
    }
    r3HoldState.store(1, std::memory_order_release);
    vTaskDelay(pdMS_TO_TICKS(80));
  }
  r3HoldState.store(2, std::memory_order_release);
  vTaskDelete(nullptr);
}

static void r3Report(const char* state) {
  Serial.print("G10R_R3_UDP:");Serial.print(token);
  Serial.print(" STATE=");Serial.println(state);
}

static void r3Service() {
  if (!r3Armed || r3FinishPrinted) return;
  const uint8_t state=r3HoldState.load(std::memory_order_acquire);
  if (state==3) {
    r3Report("MUTEX_NOT_ACQUIRED");
    r3FinishPrinted=true;
    return;
  }
  if (state==1 && !r3CloseAttempted) {
    // La llamada REAL EthernetUDP::stop() debe publicar ticket durable
    // mientras otra tarea posee el mutex. La confirmación se hace
    // después por G10R_EVENTS + snapshot, no por el estado local.
    r3ProbeUdp.stop();
    r3CloseAttempted=true;
    r3ClosedWhileHeld=(r3HoldState.load(std::memory_order_acquire)==1);
    r3Report(r3ClosedWhileHeld?"CLOSE_DURING_HOLD":"CLOSE_RACE_REVIEW");
  }
  if (state==2) {
    if (!r3ReleasedAt) r3ReleasedAt=millis();
    if ((uint32_t)(millis()-r3ReleasedAt)>=250) {
      r3Report(r3CloseAttempted&&r3ClosedWhileHeld?
               "AWAIT_LIFECYCLE_REVIEW":"INJECTION_INCOMPLETE_REVIEW");
      r3FinishPrinted=true;
    }
  }
}


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
  // R3: una sola contención real de mutex y cierre de UDP temporal.
  // No inicia sin START y token explícito; no repite automáticamente.
  static const char r3Prefix[]="G10R_R3_UDP:";
  if (strncmp(msg,r3Prefix,sizeof(r3Prefix)-1)==0) {
    const char* requested=msg+sizeof(r3Prefix)-1;
    if (!active || strlen(requested)!=24 || strcmp(requested,token)!=0 ||
        r3Armed) return;
    if (!JWPLC_Ethernet.isReady() || r3ProbeUdp.begin(15021)!=1) {
      r3Report("PREPARE_FAILED");return;
    }
    r3Armed=true;
    r3HoldState.store(0,std::memory_order_release);
    r3Report("ARMED");
    BaseType_t created=xTaskCreate(r3HoldW5500,"G10R_R3_UDP",
                                  3072,nullptr,1,nullptr);
    if (created!=pdPASS) {
      r3HoldState.store(3,std::memory_order_release);
      r3ProbeUdp.stop(); // solo un socket de laboratorio, sin cargas
      r3Report("TASK_CREATION_FAILED");
      r3FinishPrinted=true;
    }
    return;
  }
  // R3: historial acotado, sin adquirir mutex SPI y sin escribir hardware.
  static const char eventsPrefix[]="G10R_EVENTS:";
  if (strncmp(msg,eventsPrefix,sizeof(eventsPrefix)-1)==0) {
    const char* p=msg+sizeof(eventsPrefix)-1;
    if (!active || strlen(p)<32 || strncmp(p,token,24)!=0 ||
        strncmp(p+24,":AFTER=",7)!=0) return;
    const char* digits=p+31;
    if (!*digits || strlen(digits)>10) return;
    uint32_t after=0;
    while (*digits) {
      if (*digits<'0'||*digits>'9') return;
      const uint32_t d=(uint32_t)(*digits-'0');
      if (after>(UINT32_MAX-d)/10) return;
      after=after*10+d; ++digits;
    }
    jwplc_w5500_candidate::LabEvent events[8];
    uint8_t count=0; uint32_t newest=0,dropped=0;
    const auto code=jwplc_w5500_candidate::labReadEvents(
        after,events,8,&count,&newest,&dropped);
    Serial.print("G10R_EVT_HEAD:");Serial.print(token);
    Serial.print(" CODE=");Serial.print((int)code);
    Serial.print(" COUNT=");Serial.print(count);
    Serial.print(" NEWEST=");Serial.print(newest);
    Serial.print(" DROPPED=");Serial.println(dropped);
    if (code==jwplc_w5500_candidate::LabEventResult::Read ||
        code==jwplc_w5500_candidate::LabEventResult::HistoryOverrun) {
      for (uint8_t i=0;i<count;++i) {
        const auto& e=events[i];
        Serial.print("G10R_EVT:");Serial.print(token);
        Serial.print(" SEQ=");Serial.print(e.seq);
        Serial.print(" KIND=");Serial.print((int)e.kind);
        Serial.print(" SID=");Serial.print(e.socket);
        Serial.print(" EPOCH=");Serial.print(e.epoch);
        Serial.print(" ALLOC=");Serial.print(e.allocation);
        Serial.print(" OWNER=");Serial.println(e.owner);
      }
    }
    Serial.print("G10R_EVT_END:");Serial.println(token);
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
    Serial.print("G10R_SOCK_BEGIN:");Serial.print(token);
    Serial.print(" T_MS=");Serial.println(millis());
    for (uint8_t i=0;i<8;++i) {
      Serial.print("G10R_SOCK:");Serial.print(token);
      Serial.print(" SID=");Serial.print(slots[i].socketIndex);
      Serial.print(" EPOCH=");Serial.print(slots[i].epoch);
      Serial.print(" ALLOC=");Serial.print(slots[i].allocation);
      Serial.print(" OWNER=");Serial.print(slots[i].ownerCookie);
      Serial.print(" PENDING=");Serial.print(slots[i].pendingCloseAllocation);
      Serial.print(" VALID=");Serial.println(slots[i].valid?1:0);
    }
    Serial.print("G10R_SOCK_END:");Serial.println(token);
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
  r3Armed=r3CloseAttempted=r3ClosedWhileHeld=r3FinishPrinted=false;
  r3ReleasedAt=0;
  r3HoldState.store(0,std::memory_order_release);
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
  r3Service();
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

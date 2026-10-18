/*
  Alpha13 G10-R R1/R2/R4: banco aislado. COM4 = Ethernet/SD/RTC/FRAM/RTU Master.
  COM14 = Slave ID 2 opcional (g8_p4_slave). Nada de salidas Q,
  escrituras SD/FRAM, reset W5500 GPIO4 ni desactivacion de autoload.
  Sketch experimental sin actuadores ni escrituras SD/FRAM. R3 expone
  snapshots READ-ONLY owner/epoch/pending, NO valida fault injection.
  Compilar SOLO con overlay G10-I + telemetria de laboratorio G10-R.
  H0 DIAGNOSTICO UNICO: LINK=-9 NO es estado fisico; R1-R4 NO VALIDOS.
  Stage E: una sola llamada real en primer READY; W5100.cpp instrumentado
  solo en overlay temporal con etapas 31..39, filtradas por task.
  RTC retiene etapa e intento tras WDT; no se repiten tras reinicio.
  No usar como aceptacion de R1-R4.
*/
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_Display.h>
#include <JWPLC_IdleScreen.h>
#include <JWPLC_Display_H3E1_Profile.h>
#include <JWPLC_ModbusTCP.h>
#include <JWPLC_ModbusRTU.h>
#include <JWPLC_RS485.h>
#include <JWPLC_W5500_SafeAccess.h>
#include <JWPLC_Ethernet_DNS.h>
#include <string.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// H3E1 existente: habilitado únicamente para este diagnóstico de laboratorio.
// No altera lógica de refresco ni API pública; añade costo de instrumentación.
extern "C" bool jwplcH3E1ProfilerEnabled(void) { return true; }

static uint16_t registers[4] = {0x1234, 0x2345, 0x3456, 0x4567};
static uint16_t rtuValues[4] = {};
static EthernetUDP udp;
static bool udpReady=false, udpPending=false, active=false, useRtu=false;
static bool rtuReady=false;
static uint32_t bootNonce=0;

// H0 E: reproducir UNA vez el primer READY (el intento 1 genero WDT en D).
// Nueva MAGIC para no contaminar evidencia RTC de la version anterior.
static constexpr uint32_t H0_LINK_MAGIC=0x48304C33UL; // "H0L3"
static constexpr uint32_t H0_READY_MAX_CALLS=1;
static RTC_NOINIT_ATTR volatile uint32_t h0LinkMagic;
static RTC_NOINIT_ATTR volatile uint32_t h0LinkRun;
static RTC_NOINIT_ATTR volatile uint32_t h0LinkAttempt;
static RTC_NOINIT_ATTR volatile uint32_t h0LinkStage;
static RTC_NOINIT_ATTR volatile uint32_t h0LinkStageInverse;
static bool h0LinkProbePending=false;
static TaskHandle_t h0LinkCaller=nullptr; // solo task del primer READY

extern "C" void g10rH0LinkStage(uint8_t stage) {
  if (h0LinkMagic!=H0_LINK_MAGIC || h0LinkRun!=1 ||
      h0LinkCaller==nullptr ||
      xTaskGetCurrentTaskHandle()!=h0LinkCaller) return;
  // No imprimir ni esperar; otras tareas de autoload no pisan esta traza.
  h0LinkStage=(uint32_t)stage;
  h0LinkStageInverse=~(uint32_t)stage;
}

static uint32_t startMs=0, lastSample=0, lastReady=0, lastRtu=0, lastReport=0;
static uint32_t hb=0, sdOk=0, sdFail=0, udpRx=0, udpTx=0;
static uint32_t rtuOk=0, rtuFail=0, btnEvents=0, framReads=0;
static char input[90], token[25]={};
static size_t used=0;
static uint8_t packet[96];

// R3: sonda UDP exclusivamente de banco, armada por comando con token.
// El task solo sostiene el mismo mutex W5500 <=80 ms; nunca toca SPI,
// GPIO4, SD, FRAM, Q ni servicios en segundo plano.
// Bench TCP124: captura acotada sin Serial en callback de Modbus.
struct G10RTcpRow {
  uint32_t seq=0, epoch=0, allocation=0, owner=0, session=0;
  uint16_t tid=0;
  uint8_t sid=255, capture=0;
};
static G10RTcpRow tcpRows[32];
static std::atomic_flag tcpRowsBusy=ATOMIC_FLAG_INIT;
static std::atomic<uint32_t> tcpDropped{0};
static uint32_t tcpSequence=0;
static std::atomic<uint32_t> tcpSession{0};
static std::atomic<uint32_t> tcpSessionStartSeq{0};
extern "C" void g10rLabTcpRequest(uint16_t tid, uint8_t sid) {
  if (!active || sid>=8) return;
  jwplc_w5500_candidate::LabSocketSnapshot slots[8];
  const auto capture=jwplc_w5500_candidate::labCaptureSockets(slots,8,0);
  if (tcpRowsBusy.test_and_set(std::memory_order_acquire)) {
    tcpDropped.fetch_add(1,std::memory_order_relaxed);return;
  }
  if (tcpSequence==UINT32_MAX) {
    tcpDropped.fetch_add(1,std::memory_order_relaxed);
    tcpRowsBusy.clear(std::memory_order_release);return;
  }
  G10RTcpRow e{};
  e.seq=++tcpSequence;e.tid=tid;e.sid=sid;e.session=tcpSession.load(std::memory_order_acquire);
  e.capture=(uint8_t)capture;
  if (capture==jwplc_w5500_candidate::LabCaptureOutcome::Captured &&
      slots[sid].valid && slots[sid].socketIndex==sid) {
    e.epoch=slots[sid].epoch;e.allocation=slots[sid].allocation;
    e.owner=slots[sid].ownerCookie;
  }
  tcpRows[(e.seq-1)%32]=e;
  tcpRowsBusy.clear(std::memory_order_release);
}

// B2: callback de observación DHCP tras liberar el mutex SPI.
// Sólo incrementos atómicos, sin Serial ni transacciones.
static std::atomic<uint32_t> dhcpTicks{0}, dhcpPending{0};
static std::atomic<uint32_t> dhcpRenewOk{0}, dhcpRenewFail{0};
static std::atomic<uint32_t> dhcpRebindOk{0}, dhcpRebindFail{0};
extern "C" void g10rLabDhcpMaintenance(int result, bool pending) {
  dhcpTicks.fetch_add(1,std::memory_order_relaxed);
  if (pending) dhcpPending.fetch_add(1,std::memory_order_relaxed);
  switch (result) {
    case 1: dhcpRenewFail.fetch_add(1,std::memory_order_relaxed);break;
    case 2: dhcpRenewOk.fetch_add(1,std::memory_order_relaxed);break;
    case 3: dhcpRebindFail.fetch_add(1,std::memory_order_relaxed);break;
    case 4: dhcpRebindOk.fetch_add(1,std::memory_order_relaxed);break;
    default:break;
  }
}
static EthernetUDP r3ProbeUdp;
static DNSClient r3ProbeDns;
static IPAddress r3ProbeDnsResult;
static EthernetClient r3ProbeTcp;
// Esta clase es amiga UNICAMENTE del overlay staged de G10-R;
// nunca existe en la API/producto. No envía paquetes DHCP.
class G10RLabDhcpProbe {
public:
  static bool arm(DhcpClass& d) {
    return d._dhcpUdpSocket.begin(15023)==1;
  }
  static void finish(DhcpClass& d) { d.finishAsync(); }
};
static DhcpClass r3ProbeDhcp;
static uint8_t r3Mode=0; // 1 UDP, 2 DNS, 3 TCP, 4 DHCP cancel, 5 DHCP finish.
static std::atomic<uint8_t> r3HoldState{0}; // 0 idle; 1 held; 2 released; 3 lock denied
static std::atomic<uint32_t> r3HoldDurationUs{0}; // duracion REAL, no presupuesto garantizado
static bool r3Armed=false, r3CloseAttempted=false, r3ClosedWhileHeld=false;
static uint32_t r3ReleasedAt=0;
static bool r3FinishPrinted=false;
static uint8_t r3ProbeSocket=255;
static uint32_t r3Epoch=0,r3Allocation=0,r3Owner=0;

static void r3HoldW5500(void*) {
  {
    jwplc_w5500_candidate::BusScope scope(0);
    if (!scope) {
      r3HoldState.store(3, std::memory_order_release);
      vTaskDelete(nullptr);
      return;
    }
    const uint32_t heldFrom=micros();
    r3HoldState.store(1, std::memory_order_release);
    vTaskDelay(pdMS_TO_TICKS(80)); // espera nominal: el scheduler puede retrasar la salida
    r3HoldDurationUs.store((uint32_t)(micros()-heldFrom),
                           std::memory_order_release);
  }
  r3HoldState.store(2, std::memory_order_release);
  vTaskDelete(nullptr);
}

static const char* r3PrefixByMode() {
  if (r3Mode==2) return "G10R_R3_DNS:";
  if (r3Mode==3) return "G10R_R3_TCP:";
  if (r3Mode==4) return "G10R_R3_DHCP_CANCEL:";
  if (r3Mode==5) return "G10R_R3_DHCP_FINISH:";
  return "G10R_R3_UDP:";
}
static void r3Report(const char* state) {
  Serial.print(r3PrefixByMode());Serial.print(token);
  Serial.print(" STATE=");Serial.println(state);
}
static void r3Cleanup() {
  if (r3Mode==2) r3ProbeDns.cancelResolveAsync();
  else if (r3Mode==3) r3ProbeTcp.cancelConnectAsync();
  else if (r3Mode==4) r3ProbeDhcp.cancelDHCP();
  else if (r3Mode==5) G10RLabDhcpProbe::finish(r3ProbeDhcp);
  else r3ProbeUdp.stop();
}

static void r3Service() {
  if (!r3Armed || r3FinishPrinted) return;
  const uint8_t state=r3HoldState.load(std::memory_order_acquire);
  if (state==3) {
    r3Cleanup();  // liberar socket temporal también si falla inyección
    r3Report("MUTEX_NOT_ACQUIRED");
    r3FinishPrinted=true;
    return;
  }
  if (state==1 && !r3CloseAttempted) {
    // La llamada REAL EthernetUDP::stop() debe publicar ticket durable
    // mientras otra tarea posee el mutex. La confirmación se hace
    // después por G10R_EVENTS + snapshot, no por el estado local.
    r3Cleanup();
    r3CloseAttempted=true;
    r3ClosedWhileHeld=(r3HoldState.load(std::memory_order_acquire)==1);
    r3Report(r3ClosedWhileHeld?"CLOSE_DURING_HOLD":"CLOSE_RACE_REVIEW");
  }
  if (state==2) {
    if (!r3ReleasedAt) r3ReleasedAt=millis();
    if ((uint32_t)(millis()-r3ReleasedAt)>=250) {
      if (!r3CloseAttempted) r3Cleanup(); // no dejar huérfano
      const uint32_t us=r3HoldDurationUs.load(std::memory_order_acquire);
      Serial.print("G10R_R3_HOLD:");Serial.print(token);
      Serial.print(" US=");Serial.println(us);
      if (us<60000 || us>120000) r3Report("HOLD_TIMING_REVIEW");
      else r3Report(r3CloseAttempted&&r3ClosedWhileHeld?
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
  // Lectura read-only de eventos TCP: nunca declara lease si SPI estuvo BUSY.
  static const char tcpCmd[]="G10R_TCP_EVENTS:";
  if (strncmp(msg,tcpCmd,sizeof(tcpCmd)-1)==0) {
    const char* p=msg+sizeof(tcpCmd)-1;
    if (!active || strlen(p)!=24 || strcmp(p,token)!=0) return;
    G10RTcpRow batch[32];uint8_t n=0;
    uint32_t dropped=0;bool overflow=false;
    if (tcpRowsBusy.test_and_set(std::memory_order_acquire)) {
      Serial.print("G10R_TCP_STATUS:");Serial.print(token);
      Serial.println(" CODE=BUSY");return;
    }
    dropped=tcpDropped.load(std::memory_order_relaxed);
    overflow=(tcpSequence-tcpSessionStartSeq.load(std::memory_order_acquire)>32);
    const uint32_t oldest=tcpSequence>32?tcpSequence-31:1;
    for (uint32_t seq=oldest;seq<=tcpSequence;++seq) {
      const G10RTcpRow &e=tcpRows[(seq-1)%32];
      if (e.seq==seq && e.session==tcpSession.load(std::memory_order_acquire)) batch[n++]=e;
    }
    tcpRowsBusy.clear(std::memory_order_release);
    Serial.print("G10R_TCP_HEAD:");Serial.print(token);
    Serial.print(" COUNT=");Serial.print(n);
    Serial.print(" DROPPED=");Serial.print(dropped);
    Serial.print(" OVERFLOW=");Serial.println(overflow?1:0);
    for (uint8_t i=0;i<n;++i) {
      const auto &e=batch[i];
      Serial.print("G10R_TCP_OBS:");Serial.print(token);
      Serial.print(" SEQ=");Serial.print(e.seq);
      Serial.print(" TID=");Serial.print(e.tid);
      Serial.print(" SID=");Serial.print(e.sid);
      Serial.print(" CAPTURE=");Serial.print(e.capture);
      Serial.print(" EPOCH=");Serial.print(e.epoch);
      Serial.print(" ALLOC=");Serial.print(e.allocation);
      Serial.print(" OWNER=");Serial.println(e.owner);
    }
    Serial.print("G10R_TCP_END:");Serial.println(token);
    return;
  }
  // R3: una sola contención real de mutex y cierre de UDP temporal.
  // No inicia sin START y token explícito; no repite automáticamente.
  const char* r3Prefix=nullptr;
  uint8_t mode=0;
  if (strncmp(msg,"G10R_R3_UDP:",12)==0) {
    r3Prefix="G10R_R3_UDP:";mode=1;
  } else if (strncmp(msg,"G10R_R3_DNS:",12)==0) {
    r3Prefix="G10R_R3_DNS:";mode=2;
  } else if (strncmp(msg,"G10R_R3_TCP:",12)==0) {
    r3Prefix="G10R_R3_TCP:";mode=3;
  } else if (strncmp(msg,"G10R_R3_DHCP_CANCEL:",20)==0) {
    r3Prefix="G10R_R3_DHCP_CANCEL:";mode=4;
  } else if (strncmp(msg,"G10R_R3_DHCP_FINISH:",20)==0) {
    r3Prefix="G10R_R3_DHCP_FINISH:";mode=5;
  }
  if (r3Prefix) {
    const char* requested=msg+strlen(r3Prefix);
    if (!active || strlen(requested)!=24 || strcmp(requested,token)!=0 ||
        r3Armed) return;
    r3Mode=mode;
    if (!JWPLC_Ethernet.isReady()) {
      r3Report("PREPARE_FAILED");return;
    }
    // El mismo BusScope cubre snapshots previas y apertura: otra tarea
    // no puede reciclar sockets entre estas tres operaciones.
    bool ready=false;
    {
      jwplc_w5500_candidate::BusScope bus(20);
      if (bus) {
        jwplc_w5500_candidate::LabSocketSnapshot before[8],after[8];
        const auto a=jwplc_w5500_candidate::labCaptureSockets(before,8,0);
        if (a==jwplc_w5500_candidate::LabCaptureOutcome::Captured) {
          int openState=-1;
          if (r3Mode==1) {
            openState=(r3ProbeUdp.begin(15021)==1)?0:-1;
          } else if (r3Mode==2) {
            IPAddress server=Ethernet.dnsServerIP();
            if (server!=IPAddress((uint32_t)0)) {
              r3ProbeDns.begin(server);
              openState=r3ProbeDns.beginResolveAsync(
                  "jwplc-g10r.invalid",r3ProbeDnsResult,4000);
            }
          } else if (r3Mode==3) {
            // Red 198.18/15 reservada para benchmarking: no requiere
            // servidor TCP real ni modifica los servidores del PLC.
            openState=r3ProbeTcp.beginConnectAsync(
                IPAddress(198,18,0,1),15022);
          } else if (r3Mode==4 || r3Mode==5) {
            // Prueba REAL de cancel/finish de DhcpClass, SIN llamar
            // beginWithDHCP ni EthernetClass::cancelDHCP.
            // El UDP temporal port 15023 NO hace DHCP DISCOVER.
            openState=G10RLabDhcpProbe::arm(r3ProbeDhcp)?0:-1;
          }
          if (openState>=0) {
            const auto b=jwplc_w5500_candidate::labCaptureSockets(after,8,0);
            if (b==jwplc_w5500_candidate::LabCaptureOutcome::Captured) {
              uint8_t count=0;
              for (uint8_t i=0;i<8;++i) {
                if (after[i].valid && (!before[i].valid ||
                    after[i].allocation!=before[i].allocation)) {
                  ++count;
                  r3ProbeSocket=i;
                  r3Epoch=after[i].epoch;
                  r3Allocation=after[i].allocation;
                  r3Owner=after[i].ownerCookie;
                }
              }
              ready=(count==1 && r3Epoch && r3Allocation && r3Owner);
            }
          }
        }
      }
    }
    if (!ready) {
      r3Cleanup();
      r3Report("PREPARE_FAILED");return;
    }
    Serial.print("G10R_R3_LEASE:");Serial.print(token);
    Serial.print(" SID=");Serial.print(r3ProbeSocket);
    Serial.print(" EPOCH=");Serial.print(r3Epoch);
    Serial.print(" ALLOC=");Serial.print(r3Allocation);
    Serial.print(" OWNER=");Serial.println(r3Owner);
    r3Armed=true;
    r3HoldState.store(0,std::memory_order_release);
    r3HoldDurationUs.store(0,std::memory_order_release);
    r3Report("ARMED");
    BaseType_t created=xTaskCreate(r3HoldW5500,"G10R_R3_UDP",
                                  3072,nullptr,1,nullptr);
    if (created!=pdPASS) {
      r3HoldState.store(3,std::memory_order_release);
      r3Cleanup(); // solo un socket de laboratorio, sin cargas
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
  // No pisar token/estado mientras el task R3 aún posee el bus.
  if (r3Armed && !r3FinishPrinted) return;
  const char* p=msg+sizeof(prefix)-1;
  if (strlen(p)!=30 || p[24]!=':' ||
      strncmp(p+25,"RTU=",4)!=0 || (p[29]!='0' && p[29]!='1')) return;
  for (int i=0;i<24;++i)
    if (!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f'))) return;
  memcpy(token,p,24);token[24]='\0';
  useRtu=(p[29]=='1');
  hb=sdOk=sdFail=udpRx=udpTx=rtuOk=rtuFail=btnEvents=framReads=0;
  lastSample=lastRtu=lastReport=millis();
  tcpSessionStartSeq.store(tcpSequence,std::memory_order_release);
  tcpSession.fetch_add(1,std::memory_order_acq_rel); // sesión atómica.
  active=true;startMs=millis();
  r3Armed=r3CloseAttempted=r3ClosedWhileHeld=r3FinishPrinted=false;
  r3Mode=0;
  r3ReleasedAt=0;
  r3ProbeSocket=255;r3Epoch=r3Allocation=r3Owner=0;
  r3HoldState.store(0,std::memory_order_release);
  r3HoldDurationUs.store(0,std::memory_order_release);
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
  // Etiqueta SOLO del firmware de diagnóstico; no cambia el package.
  JWPLCIdleScreen::setTitle("JWPLC Basic A13");
  Serial.begin(115200);
  Serial.println("G10R_PROTOCOL:R1R2R3R4_FAULT_LAB_V2");
  Serial.println("G10R_H0_E_W5100_STAGES=31_32_33_34_35_36_37_38_39");
  Serial.println("G10R_H0_E_READY_PROBE=ONCE_TASK_FILTER_RTC_NO_R1_R4");
  if (h0LinkMagic!=H0_LINK_MAGIC) {
    h0LinkMagic=H0_LINK_MAGIC;
    h0LinkRun=0;
    h0LinkAttempt=0;
    h0LinkStage=0;
    h0LinkStageInverse=~0UL;
    h0LinkProbePending=true;
    Serial.println("G10R_H0_TRACE=ARMED_SINGLE_READY_ON_INITIAL_BOOT");
  } else {
    const bool stageValid=(h0LinkStageInverse==~h0LinkStage);
    Serial.print("G10R_H0_PREVIOUS:RUN="); Serial.print(h0LinkRun);
    Serial.print(" ATTEMPT="); Serial.print(h0LinkAttempt);
    Serial.print(" LAST_STAGE="); Serial.print(h0LinkStage);
    Serial.print(" VALID="); Serial.println(stageValid?1:0);
    Serial.println("G10R_H0_TRACE=REBOOT_NO_AUTO_REPEAT");
  }
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
  // H0 E: invocar exactamente una vez en primer READY y trazar backend W5100.
  // Si watchdog reinicia, la proxima ejecucion no repite; sin quitar autoload.
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
      Serial.print(" LINK=");
      if (h0LinkProbePending && h0LinkAttempt<H0_READY_MAX_CALLS) {
        // Ubicacion original de LINK. Los tres stores ocurren antes de SPI.
        const uint32_t attempt=h0LinkAttempt+1;
        h0LinkAttempt=attempt;
        h0LinkCaller=xTaskGetCurrentTaskHandle();
        h0LinkRun=1;
        h0LinkStage=1;
        h0LinkStageInverse=~1UL;
        const int physical=(int)JWPLC_Ethernet.linkStatus();
        h0LinkRun=2;
        h0LinkCaller=nullptr;
        Serial.print(physical);
        Serial.print(" H0_ATTEMPT=");Serial.print(attempt);
        Serial.print(" H0_STAGE=");Serial.print(h0LinkStage);
        if (attempt==H0_READY_MAX_CALLS) {
          h0LinkProbePending=false;
          Serial.print(" H0_CAP_REACHED=1");
        }
      } else {
        Serial.print(-9); // centinela: sin consultar PHY
      }
      Serial.println(" VER=R1R2R3R4_FAULT_LAB_V2");
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
    Serial.print(" LINK=-9"); // H0: no leer PHY directamente desde sketch
    Serial.print(" ETH=");Serial.print(JWPLC_Ethernet.isReady()?1:0);
    // Telemetría DHCP, solo lectura de API existente; no fuerza renew/rebind.
    Serial.print(" DHCP_STATE=");Serial.print((int)JWPLC_Ethernet.runtimeState());
    Serial.print(" DHCP_ERR=");Serial.print((int)JWPLC_Ethernet.lastError());
    Serial.print(" DHCP_TICKS=");Serial.print(dhcpTicks.load(std::memory_order_relaxed));
    Serial.print(" DHCP_PEND=");Serial.print(dhcpPending.load(std::memory_order_relaxed));
    Serial.print(" DHCP_RENEW_OK=");Serial.print(dhcpRenewOk.load(std::memory_order_relaxed));
    Serial.print(" DHCP_RENEW_FAIL=");Serial.print(dhcpRenewFail.load(std::memory_order_relaxed));
    Serial.print(" DHCP_REBIND_OK=");Serial.print(dhcpRebindOk.load(std::memory_order_relaxed));
    Serial.print(" DHCP_REBIND_FAIL=");Serial.print(dhcpRebindFail.load(std::memory_order_relaxed));
    Serial.print(" TCP=");Serial.print(JWPLC_ModbusTCP.serverReady()?1:0);
    Serial.print(" SD_OK=");Serial.print(sdOk);
    Serial.print(" SD_FAIL=");Serial.print(sdFail);
    Serial.print(" RTC=");Serial.print(JWPLC_Time.valid()?1:0);
    Serial.print(" FRAM=");Serial.print(framReads);
    Serial.print(" UDP_RX=");Serial.print(udpRx);
    Serial.print(" UDP_TX=");Serial.print(udpTx);
    Serial.print(" RTU_OK=");Serial.print(rtuOk);
    Serial.print(" RTU_FAIL=");Serial.print(rtuFail);
    // Read-only: snapshots mantenidos por autoload, sin explorar el bus.
    Serial.print(" IO_READY=");Serial.print(JWPLC_IO.ready()?1:0);
    Serial.print(" IO_AGE_MS=");Serial.print((uint32_t)(now-JWPLC_IO.lastScanMs()));
    Serial.print(" RTC_AGE_MS=");Serial.print((uint32_t)(now-JWPLC_Time.lastUpdateMs()));
    Serial.print(" TFT_READY=");Serial.print(JWPLCDisplay::isReady()?1:0);
    JWPLCH3E1Stats displayStats{};
    jwplcH3E1Snapshot(&displayStats);
    // release_bus.calls representa refrescos que adquirieron y liberaron TFT.
    Serial.print(" TFT_DRAW=");Serial.print(displayStats.release_bus.calls);
    Serial.print(" TFT_FAIL=");Serial.print(displayStats.spi_acquire_failures);
    Serial.print(" TFT_MAX_US=");Serial.print(displayStats.refresh_total.max_us);
    Serial.print(" TFT_SPI_WAIT_MAX_US=");Serial.print(displayStats.spi_wait.max_us);
    Serial.print(" BTN_EVENTS=");Serial.println(btnEvents);
  }
}

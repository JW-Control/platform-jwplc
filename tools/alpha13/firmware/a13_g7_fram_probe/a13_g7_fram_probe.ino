#include <Arduino.h>
#include <JWPLC_GlobalPeripherals.h>
#include <Preferences.h>
#include <string.h>

static const char kToken[] = "__G7_TOKEN__";
static constexpr uint32_t kBase = 0x1F00; // 128 B final dentro de 8KiB
static constexpr size_t kWindow = 128;
static const char kNs[] = "a13g7fram";
static Preferences g_nv;
static bool g_done = false;
static bool g_nvReady = false;
static uint8_t g_original[kWindow];
static uint8_t g_verify[kWindow];

struct G7Config {
  uint32_t counter;
  float gain;
  uint8_t active;
  uint8_t reserved[3];
};
static_assert(sizeof(G7Config) == 12, "struct layout G7");

static void metric(const char *name, long val) {
  Serial.print(name);
  Serial.print('=');
  Serial.println(val);
  Serial.flush();
}
static bool writeWindow(const uint8_t *src) {
  if (!JWPLC_FRAM.writeEnable(true)) return false;
  const bool wrote=JWPLC_FRAM.write(kBase,src,kWindow);
  const bool disabled=JWPLC_FRAM.writeEnable(false);
  return wrote && disabled;
}
static bool restorePending() {
  if (!g_nv.getBool("pending",false)) return true;
  uint8_t saved[kWindow]={0};
  if (g_nv.getBytesLength("original")!=kWindow ||
      g_nv.getBytes("original",saved,kWindow)!=kWindow) {
    return false;
  }
  if (!writeWindow(saved) ||
      !JWPLC_FRAM.read(kBase,g_verify,kWindow) ||
      memcmp(saved,g_verify,kWindow)!=0) {
    return false;
  }
  if (!g_nv.putBool("pending",false)) return false;
  g_nv.remove("original");
  return true;
}
static bool beginBackup() {
  if (!JWPLC_FRAM.read(kBase,g_original,kWindow)) return false;
  if (g_nv.putBytes("original",g_original,kWindow)!=kWindow) return false;
  return g_nv.putBool("pending",true);
}
static bool runTest() {
  int errors=0;
  // Las operaciones con punteros nulos NO deben tocar memoria.
  if (JWPLC_FRAM.read(kBase,nullptr,1)) ++errors;
  if (JWPLC_FRAM.write(kBase,nullptr,1)) ++errors;
  if (!JWPLC_FRAM.read(kBase,nullptr,0)) ++errors;
  if (!JWPLC_FRAM.write(kBase,nullptr,0)) ++errors;
  if (JWPLC_FRAM.isAddressValid(JWPLC_FRAM.size()-1,2)) ++errors;
  metric("G7_FRAM_NULL_AND_BOUNDS_ERRORS",errors);

  G7Config initial={0x13572468u,3.25f,1,{0x10,0x20,0x30}};
  G7Config readValue={};
  if (!JWPLC_FRAM.writeBlock(kBase,initial,5) ||
      !JWPLC_FRAM.readBlock(kBase,readValue,5) ||
      memcmp(&initial,&readValue,sizeof(initial))!=0) {
    ++errors;
  }
  metric("G7_FRAM_VALID_BLOCK_ERRORS",errors);

  // Corromper payload intencionadamente dentro de nuestra ventana.
  const uint32_t dataAddr=kBase+sizeof(JW_FRAM::BlockHeader);
  uint8_t byte=0;
  if (!JWPLC_FRAM.read(dataAddr,&byte,1)) ++errors;
  byte^=0x5Au;
  const bool armed=JWPLC_FRAM.writeEnable(true);
  const bool changed=armed && JWPLC_FRAM.write(dataAddr,&byte,1);
  const bool disarmed=JWPLC_FRAM.writeEnable(false);
  if (!armed || !changed || !disarmed) ++errors;
  G7Config guard={0xFEEDF00Du,77.0f,0,{1,2,3}};
  const G7Config before=guard;
  const bool corruptAccepted=JWPLC_FRAM.readBlock(kBase,guard,5);
  if (corruptAccepted || memcmp(&guard,&before,sizeof(guard))!=0) ++errors;
  metric("G7_FRAM_CORRUPT_BLOCK_ERRORS",errors);

  // Rechazo por versión incompatible también preserva el objeto destino.
  guard=before;
  if (JWPLC_FRAM.readBlock(kBase,guard,6) ||
      memcmp(&guard,&before,sizeof(guard))!=0) ++errors;
  metric("G7_FRAM_VERSION_ERRORS",errors);
  return errors==0;
}
static void execute() {
  int errors=0;
  if (JWPLC_FRAM.size()!=8192 ||
      !JWPLC_FRAM.isAddressValid(kBase,kWindow)) {
    ++errors;
  }
  const bool beforeRecovered=g_nvReady && restorePending();
  metric("G7_FRAM_STALE_RECOVERY",beforeRecovered?1:0);
  if (!beforeRecovered) ++errors;

  bool backed=false;
  if (errors==0) {
    backed=beginBackup();
    if (!backed) ++errors;
  }
  metric("G7_FRAM_BACKUP_PASS",backed?1:0);
  if (backed) {
    if (!runTest()) ++errors;
  }

  // Siempre intentar restaurar, incluso con errores de prueba.
  const bool restored=g_nvReady && restorePending();
  metric("G7_FRAM_RESTORE_PASS",restored?1:0);
  if (!restored) ++errors;
  metric("G7_FRAM_TOTAL_ERRORS",errors);
  Serial.print("G7_FRAM_RESULT=");
  Serial.println(errors==0?"PASS":"FAIL");
  Serial.print("G7_FRAM_DONE=");Serial.println(kToken);
  Serial.flush();
  g_done=true;
}
void setup() {
  Serial.begin(115200);
  Serial.setTimeout(300);
  g_nvReady=g_nv.begin(kNs,false);
  // En caso de reinicio, NO iniciar nuevas escrituras antes de
  // comprobar la recuperación pendiente en el comando G7_RUN.
}
void loop() {
  if (g_done) { delay(30); return; }
  static uint32_t last=0;
  if ((uint32_t)(millis()-last)>700) {
    last=millis();
    Serial.print("G7_FRAM_READY=");Serial.println(kToken);
    Serial.flush();
  }
  if (!Serial.available()) {delay(3);return;}
  String cmd=Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd==String("G7_FRAM_RUN:")+kToken) execute();
}

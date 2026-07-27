#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_LogicRuntime.h>

static JWPLC_LogicRuntime runtime;
static uint16_t passedTests = 0;
static uint16_t failedTests = 0;

static void expect(const char *name, bool condition)
{
  Serial.print(condition ? "[PASS] " : "[FAIL] ");
  Serial.println(name);

  if (condition)
  {
    ++passedTests;
  }
  else
  {
    ++failedTests;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - API persistente en modo lectura");
  Serial.println("No inicializa E/S y no escribe ni formatea la FRAM.");
  Serial.println();

  JWPLCLogicStorage &storage = runtime.storage();
  const bool beginOk = storage.begin(JWPLC_FRAM);

  expect("storage().begin(JWPLC_FRAM)", beginOk);
  expect("fachada queda lista", beginOk && storage.isReady());
  expect("perfil detectado de 8 KiB",
         beginOk && storage.profile().framBytes == 8192);
  expect("layout de produccion valido",
         beginOk && storage.layout().isValid());
  expect("gestor A/B ocupa 5184 bytes",
         beginOk && storage.layout().programStoreBytes() == 5184);
  expect("limite inicial es 100 bloques",
         beginOk && storage.layout().maxBlocks == 100);
  expect("no existe programa cargado en RAM",
         beginOk && !storage.hasLoadedProgram());

  const LogicProgram emptyProgram = storage.activeProgram();
  expect("activeProgram vacio antes de loadActive",
         emptyProgram.name == nullptr &&
             emptyProgram.blocks == nullptr &&
             emptyProgram.blockCount == 0);

  if (beginOk)
  {
    const LogicProgramStoreStatus &status = storage.status();

    Serial.println();
    Serial.print("FRAM formateada para runtime: ");
    Serial.println(storage.isFormatted() ? "SI" : "NO");
    Serial.print("Slot activo: ");
    if (status.activeSlot == JWPLC_LOGIC_INVALID_SLOT)
    {
      Serial.println("NINGUNO");
    }
    else
    {
      Serial.println(status.activeSlot == 0 ? "A" : "B");
    }
    Serial.print("Secuencia: ");
    Serial.println(status.sequence);
    Serial.print("Program ID: ");
    Serial.println(status.programId);
    Serial.print("Generacion: ");
    Serial.println(status.generation);
    Serial.print("Error fachada: ");
    Serial.println(JWPLCLogicStorage::errorName(storage.lastError()));
    Serial.print("Estado gestor: ");
    Serial.println(LogicProgramStore::errorName(storage.storeError()));
  }

  Serial.println();
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");

  if (failedTests == 0)
  {
    Serial.println("API PERSISTENTE LECTURA: PASS");
  }
  else
  {
    Serial.println("API PERSISTENTE LECTURA: FAIL");
  }
}

void loop()
{
}

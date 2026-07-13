#include <JWPLC_LogicRuntime.h>

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

static void printRegion(const char *name, const LogicStorageRegion &region)
{
  Serial.print(name);
  Serial.print(": 0x");
  Serial.print(region.address, HEX);
  Serial.print("..0x");
  Serial.print(region.lastAddress(), HEX);
  Serial.print(" (");
  Serial.print(region.size);
  Serial.println(" bytes)");
}

static void printLayout(const char *name,
                        const LogicStorageLayout &layout,
                        uint16_t physicalMaxBlocks)
{
  Serial.println();
  Serial.println(name);
  Serial.print("Capacidad total: ");
  Serial.print(layout.totalBytes);
  Serial.println(" bytes");
  printRegion("Superblocks", layout.superblocks);
  printRegion("Slot A", layout.slotA);
  printRegion("Slot B", layout.slotB);
  printRegion("Retentivos", layout.retain);
  printRegion("Reserva", layout.reserved);
  Serial.print("Capacidad util por slot: ");
  Serial.print(layout.slotPayloadBytes());
  Serial.println(" bytes");
  Serial.print("Capacidad fisica del formato: ");
  Serial.print(physicalMaxBlocks);
  Serial.println(" bloques");
  Serial.print("Limite efectivo del build: ");
  Serial.print(layout.maxBlocks);
  Serial.println(" bloques");
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("JWPLC Logic Runtime - mapa persistente de produccion");
  Serial.println("No inicializa E/S ni lee o escribe la FRAM.");

  const LogicStorageLayout &layout8 = JWPLCLogicStorageLayouts::FRAM_8K;
  const LogicStorageLayout &layout32 = JWPLCLogicStorageLayouts::FRAM_32K;

  printLayout("Perfil FRAM 8 KiB",
              layout8,
              JWPLCLogicStorageProfiles::FRAM_8K_PHYSICAL_MAX_BLOCKS);
  printLayout("Perfil FRAM 32 KiB",
              layout32,
              JWPLCLogicStorageProfiles::FRAM_32K_PHYSICAL_MAX_BLOCKS);

  Serial.println();
  expect("mapa de 8 KiB valido", layout8.isValid());
  expect("mapa de 32 KiB valido", layout32.isValid());
  expect("superblocks de 8 KiB ocupan 64 bytes",
         layout8.superblocks.size == 64);
  expect("Slot A de 8 KiB inicia en 0x0040",
         layout8.slotA.address == 0x0040);
  expect("Slot B de 8 KiB inicia en 0x0A40",
         layout8.slotB.address == 0x0A40);
  expect("retentivos de 8 KiB inician en 0x1440",
         layout8.retain.address == 0x1440);
  expect("reserva de 8 KiB inicia en 0x1A40",
         layout8.reserved.address == 0x1A40);
  expect("mapa de 8 KiB termina en 0x1FFF",
         layout8.reserved.lastAddress() == 0x1FFF);
  expect("gestor A/B usa 5184 bytes en perfil 8 KiB",
         layout8.programStoreBytes() == 5184 &&
             LogicProgramStore::requiredCapacity(
                 JWPLCLogicStorageProfiles::FRAM_8K) == 5184);
  expect("payload util de Slot A es 2528 bytes",
         layout8.slotPayloadBytes() == 2528);
  expect("imagen de 100 bloques cabe en Slot A",
         LogicProgramCodec::requiredSize(100) <=
             layout8.slotPayloadBytes());
  expect("imagen de 400 bloques cabe en perfil 32 KiB",
         LogicProgramCodec::requiredSize(400) <=
             layout32.slotPayloadBytes());
  expect("perfil 8 KiB deja 1472 bytes reservados",
         layout8.reserved.size == 1472);
  expect("perfil 32 KiB deja 4032 bytes reservados",
         layout32.reserved.size == 4032);
  expect("capacidad menor a 8 KiB no selecciona layout",
         !JWPLCLogicStorageLayouts::forCapacity(4096).isValid());

  Serial.println();
  Serial.print("Resultado: ");
  Serial.print(passedTests);
  Serial.print(" PASS, ");
  Serial.print(failedTests);
  Serial.println(" FAIL");

  if (failedTests == 0)
  {
    Serial.println("MAPA PERSISTENTE: PASS");
  }
  else
  {
    Serial.println("MAPA PERSISTENTE: FAIL");
  }
}

void loop()
{
}

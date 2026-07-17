#include <JWPLC_LogicRuntime.h>
#include "../../src/edit/RuntimeUIV2EditSession.h"

static LogicV2EnginePrototype engine;
static RuntimeUIV2EditSession editor;

static const LogicV2InputLink LINKS[] = {
    LogicV2InputLink::block(0),
    LogicV2InputLink::block(1, true)};

static const LogicV2BlockRecord BLOCKS[] = {
    {LogicV2BlockType::DigitalInput, 0, 0, 0},
    {LogicV2BlockType::DigitalInput, 0, 0, 1},
    {LogicV2BlockType::And, 0, 2}};

static const LogicV2Program PROGRAM = {
    BLOCKS,
    3,
    LINKS,
    2};

static uint16_t passed = 0;
static uint16_t failed = 0;

static void check(bool condition, const char *name)
{
  Serial.print(condition ? "PASS: " : "FAIL: ");
  Serial.println(name);
  if (condition)
  {
    ++passed;
  }
  else
  {
    ++failed;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  check(engine.loadProgram(PROGRAM, 2, 0), "carga programa base");
  check(engine.start(), "arranca programa base");

  bool inputs[2] = {true, false};
  check(engine.scan(inputs, 2, millis()), "scan inicial");
  check(engine.blockValue(2), "AND true con segunda entrada negada");

  editor.attach(engine);
  check(editor.begin(), "crea borrador RAM");
  check(editor.active(), "sesion activa");
  check(!editor.dirty(), "borrador inicialmente limpio");

  check(editor.setInputInverted(2, 1, false),
        "quita negacion de IN2 en borrador");
  check(editor.dirty(), "borrador marcado como modificado");
  check(editor.validate() == LogicV2PrototypeError::None,
        "borrador valido");
  check(editor.apply(true), "aplica borrador y reinicia motor");

  check(engine.scan(inputs, 2, millis()), "scan tras aplicar");
  check(!engine.blockValue(2), "AND false sin negacion");

  check(editor.begin(), "abre segundo borrador");
  check(editor.setInputSource(2,
                              1,
                              JWPLC_LOGIC_V2_SOURCE_CONST_TRUE,
                              false),
        "cambia IN2 a constante HI");
  check(editor.apply(true), "aplica fuente HI");
  check(engine.scan(inputs, 2, millis()), "scan con HI");
  check(engine.blockValue(2), "AND true con I0.0 y HI");

  Serial.println();
  Serial.print("PASS=");
  Serial.print(passed);
  Serial.print(" FAIL=");
  Serial.println(failed);
}

void loop()
{
  delay(1000);
}

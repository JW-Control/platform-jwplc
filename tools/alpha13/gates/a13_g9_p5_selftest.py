#!/usr/bin/env python3
"""G9 P5: autoprueba read-only parser, limites y APIs sketch, sin hardware."""
from __future__ import annotations
import importlib.util
from pathlib import Path
import sys

def need(ok,reason):
    if not ok:raise RuntimeError(reason)

def rejects(fn,label):
    try:fn()
    except RuntimeError:pass
    else:raise RuntimeError("NEGATIVE_CASE_ACCEPTED_"+label)

def main():
    script=Path(__file__).resolve().parent/"a13_g9_p5_physical_com4.py"
    spec=importlib.util.spec_from_file_location("g9_p5_driver",script)
    need(spec is not None and spec.loader is not None,"SCRIPT_IMPORT_FAILED")
    g=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=g
    spec.loader.exec_module(g)
    need(g.PORT=="COM4" and g.PORT_VID==0x1A86 and g.PORT_PID==0x7523,
         "WRONG_PORT_IDENTITY")
    need(g.PRODUCT_COMMIT=="63e0291a7b1480af846e955559ab175aa78e7300",
         "PRODUCT_COMMIT_WRONG")
    need(g.ARCHIVE_SHA=="58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e",
         "ARCHIVE_SHA_WRONG")
    need(g.CONFIRM=="G9 FLASH COM4 SD RTC BUTTONS","MISSING_OPERATOR_CONFIRMATION")
    token="0123456789abcdef01234567"
    good={"SD_PRESENT":1,"SD_READY":1,"SD_ROOT":1,
          "RTC_PRESENT":1,"RTC_VALID":1,"RTC_LOST":0,
          "BTN_PRESS":1,"BTN_RELEASE":1,"BTN_ID":4}
    good_line=" ".join(k+"="+str(v) for k,v in good.items())
    need(g.parse_fields(good_line)==good,"FIELD_PARSER_GOOD_FAILED")
    need(g.REPORT_RE.fullmatch("G9_P5_RESULT:"+token+" "+good_line),
         "REPORT_SESSION_REGEX_FAILED")
    need(not g.REPORT_RE.fullmatch("G9_P5_RESULT:bad "+good_line),
         "BAD_TOKEN_ACCEPTED")
    need(g.EVENT_RE.fullmatch("G9_P5_PRESS:"+token+" ID=4") and
         g.EVENT_RE.fullmatch("G9_P5_RELEASE:"+token+" ID=4"),
         "BUTTON_EVENT_REGEX_FAILED")
    events=[("PRESS",4),("RELEASE",4)]
    need(g.check_report(good,events),"GOOD_REPORT_FAILED")
    need(g.check_report({**good,"RTC_LOST":1},events),
         "RTC_LOST_DIAGNOSTIC_NOT_PRESERVED")
    print("G9_P5_SELFTEST_GOOD_REPORT=PASS",flush=True)
    cases=[
      ({**good,"SD_PRESENT":0},events,"SD_NO_CARD"),
      ({**good,"SD_READY":0},events,"SD_NOT_READY"),
      ({**good,"SD_ROOT":0},events,"SD_ROOT_NOT_READ"),
      ({**good,"RTC_PRESENT":0},events,"RTC_ABSENT"),
      ({**good,"RTC_VALID":0},events,"RTC_INVALID"),
      ({**good,"BTN_PRESS":0},events,"NO_BUTTON_PRESS"),
      ({**good,"BTN_RELEASE":0},events,"NO_BUTTON_RELEASE"),
      ({**good,"BTN_ID":6},events,"BUTTON_OUT_OF_RANGE"),
      ({**good,"RTC_LOST":3},events,"RTC_LOST_INVALID"),
      (good,[("PRESS",3),("RELEASE",4)],"DIFFERENT_BUTTON"),
      (good,[("RELEASE",4),("PRESS",4)],"BUTTON_ORDER_WRONG"),
      (good,[("PRESS",4)],"BUTTON_RELEASE_EVENT_MISSING"),
    ]
    for field,event,label in cases:
        rejects(lambda f=field,e=event:g.check_report(f,e),label)
    for source,label in [
      (good_line.replace("SD_READY=1","SD_READY=-1"),"SIGNED_FIELD"),
      (good_line+" OTHER=1","UNSUPPORTED_FIELD"),
      (good_line+" RTC_VALID=1","DUPLICATE_FIELD"),
      (good_line.replace("BTN_ID=4","BTN_ID=Q"),"NOT_INTEGER"),
    ]:
        rejects(lambda s=source:g.parse_fields(s),label)
    print("G9_P5_SELFTEST_NEGATIVE_CASES=16_PASS",flush=True)
    s=(g.SKETCH/(g.SKETCH.name+".ino")).read_text(encoding="utf-8")
    required=[
      "#include <JWPLC_GlobalPeripherals.h>",
      'JWPLC_SD.open("/", FILE_READ)',
      "root.isDirectory()",
      "root.close()",
      "JWPLC_Time.present()",
      "JWPLC_Time.valid()",
      "JWPLC_Time.lostPower()",
      "JWPLC_Buttons.pressed(kIds[i])",
      "JWPLC_Buttons.released(kIds[i])",
      "JWPLC_Buttons.clearPendingInput()",
      "G9_P5_START:",
      "G9_P5_READY:COM4:SD_RTC_BUTTONS:READ_ONLY",
      "G9_P5_RESULT:",
      "G9_P5_HEARTBEAT:",
    ]
    for marker in required:
        need(marker in s,"SKETCH_CONTRACT_MISSING_"+marker)
    forbidden=[
      "FILE_WRITE","FILE_APPEND","SD.remove(","SD.mkdir(","SD.rmdir(",
      "JWPLC_SD.remove(","JWPLC_SD.mkdir(","JWPLC_SD.rmdir(",
      "JWPLC_SD.rename(","JWPLC_SD.openNative(","setTime(","adjust(",
      "digitalWrite(","pinMode(","JWPLC_Buttons.update()",
      "JWPLC_FRAM.write(","JWPLC_RTC.set",
    ]
    executable="\n".join(z for z in s.split("\n") if not z.lstrip().startswith("*"))
    for marker in forbidden:
        need(marker not in executable,"UNSAFE_SKETCH_API_FOUND_"+marker)
    driver=script.read_text(encoding="utf-8")
    for marker in [
      '"--libraries",LIBS,SKETCH',
      'CONFIRM="G9 FLASH COM4 SD RTC BUTTONS"',
      'selected(log,"JWPLC_GlobalPeripherals",GLOBAL)',
      'selected(log,"JW_SD",SD)',
      '"--port",PORT',
      'M["upload_COM14"]="NOT_ATTEMPTED"',
      'serial_obj.dtr=False',
      'serial_obj.rts=False',
      'need(observed["heartbeats"]>=3',
      'phase("FINAL_AUDIT","PASS")',
    ]:
        need(marker in driver,"DRIVER_GUARD_MISSING_"+marker)
    print("G9_P5_SELFTEST_READ_ONLY_SKETCH=PASS",flush=True)
    print("G9_P5_SELFTEST=PASS_NO_HARDWARE_ACCESS",flush=True)

if __name__=="__main__":
    main()

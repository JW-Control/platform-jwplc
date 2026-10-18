#!/usr/bin/env python3
"""H0-F: etapas 60..77 DENTRO de W5100.init y deteccion W5200/W5500.

Se aplica DESPUES de H0-E, unicamente a utility/w5100.cpp staged.
No modifica codigo producto, no realiza nuevas transacciones SPI ni Serial.
"""
from __future__ import annotations

import argparse
from pathlib import Path

PATH="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
INIT_SIG="uint8_t W5100Class::init(void)\n{"
INIT_END="\n// Soft reset the WIZnet chip"
W5200_SIG="uint8_t W5100Class::isW5200(void)\n{"
W5500_SIG="uint8_t W5100Class::isW5500(void)\n{"
W5100_SIG="uint8_t W5100Class::isW5100(void)\n{"
METHOD_END="\n}\n"
CHECK="h0W5100Stage("

def split_section(source: str, start: str, end: str) -> tuple[str,str,str]:
    if source.count(start)!=1 or source.count(end)<1:
        raise RuntimeError("H0_F_SOURCE_ANCHOR_DRIFT_"+start[:25])
    pre,tail=source.split(start,1)
    if end not in tail:
        raise RuntimeError("H0_F_END_ANCHOR_DRIFT_"+start[:25])
    body,post=tail.split(end,1)
    return pre,body,post

def replace_one(text: str, old: str, new: str, code: str) -> str:
    if text.count(old)!=1: raise RuntimeError("H0_F_"+code+"_SOURCE_DRIFT")
    return text.replace(old,new,1)

def patched_init(src: str) -> str:
    pre,body,post=split_section(src,INIT_SIG,INIT_END)
    body=replace_one(body,
        "\tif (initialized) return 1;",
        """\th0W5100Stage(60); // init entrado
\tif (initialized) {
\t\th0W5100Stage(61); // retorno sin tocar SPI: ya inicializado
\t\treturn 1;
\t}
\th0W5100Stage(62); // primera inicializacion, antes SPI.begin""",
        "INITIALIZED_GUARD")
    body=replace_one(body,
        """\tSPI.begin();
\tinitSS();
\tresetSS();
\tSPI.beginTransaction(SPI_ETHERNET_SETTINGS);""",
        """\tSPI.begin();
\th0W5100Stage(63); // SPI.begin retorno
\tinitSS();
\th0W5100Stage(64); // initSS retorno
\tresetSS();
\th0W5100Stage(65); // resetSS retorno
\th0W5100Stage(66); // antes SPI.beginTransaction
\tSPI.beginTransaction(SPI_ETHERNET_SETTINGS);
\th0W5100Stage(67); // SPI.beginTransaction retorno""",
        "SPI_SETUP")
    body=replace_one(body,
        "\tif (isW5200()) {",
        "\th0W5100Stage(68); // antes de identificar W5200\n\tif (isW5200()) {",
        "FIRST_CHIP_PROBE")
    body=replace_one(body,
        """\tSPI.endTransaction();
\tinitialized = true;
\treturn 1; // successful init""",
        """\th0W5100Stage(75); // antes de endTransaction final
\tSPI.endTransaction();
\th0W5100Stage(76); // endTransaction retorno
\tinitialized = true;
\th0W5100Stage(77); // inicializacion completada
\treturn 1; // successful init""",
        "FINAL_TRANSACTION")
    return pre+INIT_SIG+body+INIT_END+post

def patched_probe(src: str, signature: str, ending: str,
                  enter: int, before: int, after: int) -> str:
    pre,body,post=split_section(src,signature,ending)
    body=replace_one(body,
        "\tchip = "+("52" if enter==69 else "55")+";",
        "\th0W5100Stage("+str(enter)+"); // entra probe\n\tchip = "+("52" if enter==69 else "55")+";",
        "PROBE_CHIP_"+str(enter))
    body=replace_one(body,
        "\tif (!softReset()) return 0;",
        ("\th0W5100Stage("+str(before)+"); // antes softReset\n"
         "\tif (!softReset()) return 0;\n"
         "\th0W5100Stage("+str(after)+"); // softReset retorno correcto"),
        "PROBE_RESET_"+str(enter))
    return pre+signature+body+ending+post

def instrument(src: str) -> str:
    if src.count(INIT_SIG)!=1 or src.count(W5200_SIG)!=1 or src.count(W5500_SIG)!=1:
        raise RuntimeError("H0_F_EXPECTED_WIZNET_METHODS_MISSING")
    if "h0W5100Stage(60)" in src:
        raise RuntimeError("H0_F_DUPLICATE_PATCH")
    old_count=src.count("SPI.beginTransaction(SPI_ETHERNET_SETTINGS);")
    old_reads=src.count("readPHYCFGR_W5500();")
    output=patched_init(src)
    output=patched_probe(output,W5200_SIG,W5500_SIG,69,70,71)
    output=patched_probe(output,W5500_SIG,W5100_SIG,72,73,74)
    for i in range(60,78):
        if output.count("h0W5100Stage("+str(i)+")")!=1:
            raise RuntimeError("H0_F_STAGE_"+str(i)+"_NOT_EXACTLY_ONCE")
    if (output.count("SPI.beginTransaction(SPI_ETHERNET_SETTINGS);")!=old_count
            or output.count("readPHYCFGR_W5500();")!=old_reads):
        raise RuntimeError("H0_F_CHANGED_SPI_OPERATION_COUNT")
    return output

def compose(files:dict[str,str])->dict[str,str]:
    if len(files)!=15 or PATH not in files:
        raise RuntimeError("H0_F_EXPECTED_15_STAGED_SOURCES")
    patched=instrument(files[PATH])
    result=dict(files)
    result[PATH]=patched
    if len(result)!=15 or any(result[p]!=files[p] for p in files if p!=PATH):
        raise RuntimeError("H0_F_UNEXPECTED_SOURCE_MODIFIED")
    return result

def selftest()->None:
    root=Path(__file__).resolve().parents[3]
    src=(root/PATH).read_text(encoding="utf-8-sig")
    import importlib.util
    prior=root/"tools/alpha13/gates/a13_g10_r_h0_w5100_stage.py"
    spec=importlib.util.spec_from_file_location("h0_f_prior",prior)
    if spec is None or spec.loader is None:
        raise RuntimeError("H0_F_PRIOR_STAGE_NOT_FOUND")
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    base=module.instrument(src)
    amended=instrument(base)
    assert amended.count("h0W5100Stage(")==28  # E:10, F:18
    assert "h0W5100Stage(62)" in amended
    assert "h0W5100Stage(66)" in amended
    assert "h0W5100Stage(75)" in amended
    assert "Serial.print" not in amended
    try:instrument(amended)
    except RuntimeError:pass
    else:raise AssertionError("H0_F_DOUBLE_PATCH_ACCEPTED")
    for name,mutated in (
        ("SPI",base.replace("\tSPI.begin();","\tSPI.otherBegin();",1)),
        ("GUARD",base.replace("if (initialized) return 1;","if (init) return 1;",1)),
        ("W5200",base.replace("\tif (!softReset()) return 0;","\tif (fakeReset()) return 0;",1)),
    ):
        try: instrument(mutated)
        except RuntimeError: pass
        else: raise AssertionError("H0_F_MUTATION_ACCEPTED_"+name)
    print("G10R_H0_F_INIT_STAGE_60_TO_77=PASS")
    print("G10R_H0_F_NO_EXTRA_SPI_OR_SERIAL=PASS")
    print("G10R_H0_F_SOURCE_NEGATIVES=4_REJECTED")

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--selftest",action="store_true",required=True)
    p.parse_args()
    selftest()

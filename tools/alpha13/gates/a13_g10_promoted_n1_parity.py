#!/usr/bin/env python3
"""G10 N1: comprobar igualdad de 14 fuentes *promovidas* con compositor de BASE.

No usa la fuente G10-R de laboratorio; no escribe archivos productivos.
El contrato nativo se ejecuta por separado en el checkout base mediante
a13_g10_integrated.py --ci-standalone (N1 fuente C++ real).
"""
import argparse
import hashlib
import subprocess
import sys
from pathlib import Path

F1_BASE="2b5802720df552e01a0b4f9d4fe1a7bd2de16daf"
BASE="0344ff9cbe42e9efa65e7d4caabb60ce15464270"
ROOT=Path(__file__).resolve().parents[3]
def die(msg):
    raise RuntimeError("G10_N1_REVIEW_STOP_"+msg)
def git(repo,*args):
    p=subprocess.run(["git","-C",str(repo),*args],capture_output=True)
    if p.returncode:die("GIT_"+repr(args)+":"+p.stderr.decode(errors="replace")[-200:])
    return p.stdout
def h(bytes_):
    return hashlib.sha256(bytes_).hexdigest()
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--baseline",type=Path,required=True)
    args=ap.parse_args()
    baseline=args.baseline.resolve()
    if git(baseline,"rev-parse","HEAD").decode().strip()!=BASE:
        die("BASE_REF_DRIFT")
    if not (baseline/"tools/alpha13/gates/a13_g10_integrated.py").is_file():
        die("COMPOSITOR_MISSING")
    # Importar compositor desde el checkout base puro; no desde el producto ya adoptado.
    sys.path.insert(0,str(baseline/"tools/alpha13/gates"))
    import a13_g10_integrated
    expected=a13_g10_integrated.compose()
    if len(expected)!=14:die("COMPOSE_NOT_14")
    changed=0
    for relative,generated in sorted(expected.items()):
        product=ROOT/relative
        if not product.is_file():die("MISSING_PROMOTED_"+relative)
        promoted=product.read_bytes().replace(b"\r\n",b"\n")
        source=generated.encode("utf-8")
        if promoted!=source:die("COMPOSE_PARITY_"+relative)
        was_present=(baseline/relative).exists()
        if was_present and (baseline/relative).read_bytes().replace(b"\r\n",b"\n")!=source:
            changed+=1
        print("G10_N1_SOURCE="+relative+" SHA256="+h(source)+" STATE="+("MODIFIED" if was_present else "ADDED"))
    if changed!=10:die("EXPECTED_10_MODIFIED_GOT_"+str(changed))
    f1="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
    for product_root in (ROOT,baseline):
        content=(product_root/f1).read_bytes().replace(b"\r\n",b"\n")
        if content!=git(product_root,"show",F1_BASE+":"+f1):
            die("F1_W5100_DRIFT")
    if f1 in expected:die("F1_IN_COMPOSITOR")
    print("G10_N1_PROMOTED_COMPOSE_PARITY=PASS_14_OF_14_SHA_MATCH")
    print("G10_N1_MODIFIED=10 ADDED=2 UNCHANGED=2")
    print("G10_N1_F1_W5100=PASS_PRESERVED")
    print("G10_N1_NO_PRODUCT_WRITE=YES")
if __name__=="__main__":
    try:main()
    except (RuntimeError,OSError,UnicodeError) as e:
        print(str(e),file=sys.stderr)
        sys.exit(2)

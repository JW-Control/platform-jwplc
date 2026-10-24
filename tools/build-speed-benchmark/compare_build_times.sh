#!/usr/bin/env bash
# =============================================================================
# Compara tiempos de compilación entre dos versiones del package JWPLC en
# Linux (nube, WSL o un runner de CI). Equivale a Compare-JWPLCBuildBenchmark.ps1
# pero arma las dos versiones desde git, sin instalar nada en Arduino15:
#
#   base      = un ref de git (por defecto origin/release/v2.1.x = alpha.13 publicada)
#   candidata = el árbol de trabajo actual del repo
#
# Cada versión se enlaza como plataforma local (jwplc_base / jwplc_head) en un
# sketchbook propio y vacío, así ninguna librería externa tapa a las del package.
#
#   Frío     = caché de build aislada y vacía + build-path nuevo
#   Caliente = misma caché y build-path; sólo cambia la fecha del .ino
#
# Uso:
#   tools/alpha15/regenerate_libJWPLC_TFT.sh setup     # una vez: arduino-cli + tools
#   tools/build-speed-benchmark/compare_build_times.sh [opciones]
#
# Opciones:
#   --base-ref REF        (origin/release/v2.1.x)   --base-label TXT (2.1.0-alpha.13)
#   --head-label TXT      (2.1.0-alpha.14)
#   --targets "basic core" (basic)                  --sketches "01_empty 02_io_basic"
#   --reps N              (3)                       --out DIR
#
# Deja results.csv y COMPARE.md (tabla con la mediana) en --out.
# =============================================================================
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WORK="${JWPLC_BUILD_DIR:-$HOME/jwplc-build}"
BASE_REF="origin/release/v2.1.x"
BASE_LABEL="2.1.0-alpha.13"
HEAD_LABEL="2.1.0-alpha.14"
TARGETS="basic"
SKETCHES="01_empty 02_io_basic"
REPS=3
OUT=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --base-ref) BASE_REF="$2"; shift 2 ;;
    --base-label) BASE_LABEL="$2"; shift 2 ;;
    --head-label) HEAD_LABEL="$2"; shift 2 ;;
    --targets) TARGETS="$2"; shift 2 ;;
    --sketches) SKETCHES="$2"; shift 2 ;;
    --reps) REPS="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    *) echo "opción desconocida: $1"; exit 2 ;;
  esac
done

export PATH="$WORK/bin:$PATH"
command -v arduino-cli >/dev/null ||
  { echo "ERROR: falta arduino-cli; correr antes tools/alpha15/regenerate_libJWPLC_TFT.sh setup"; exit 1; }
[[ -d "$WORK/data/packages/jwplc/tools" ]] ||
  { echo "ERROR: faltan los tools del package en $WORK/data; correr el setup"; exit 1; }

OUT="${OUT:-$WORK/bench_$(date +%Y%m%d_%H%M%S)}"
BENCH="$OUT/env"
mkdir -p "$BENCH/user/hardware/jwplc_base" "$BENCH/user/hardware/jwplc_head" "$BENCH/sketches" "$OUT/logs"

# Versión base desde git (worktree desprendido) y candidata desde el árbol actual.
BASE_TREE="$BENCH/base-tree"
if [[ ! -d "$BASE_TREE" ]]; then
  git -C "$REPO" worktree add --quiet --detach "$BASE_TREE" "$BASE_REF" >/dev/null
fi
trap 'git -C "$REPO" worktree remove --force "$BASE_TREE" >/dev/null 2>&1 || true' EXIT
ln -sfn "$BASE_TREE/JWPLC/2.1.0" "$BENCH/user/hardware/jwplc_base/esp32"
ln -sfn "$REPO/JWPLC/2.1.0" "$BENCH/user/hardware/jwplc_head/esp32"

# Sketchbook vacío propio; los datos (tools del packager jwplc) se reutilizan.
export ARDUINO_DIRECTORIES_DATA="$WORK/data"
export ARDUINO_DIRECTORIES_DOWNLOADS="$WORK/dl"
export ARDUINO_DIRECTORIES_USER="$BENCH/user"

for s in $SKETCHES; do
  [[ -d "$REPO/tools/build-speed-benchmark/sketches/$s" ]] || { echo "ERROR: sketch $s no existe"; exit 1; }
  rm -rf "$BENCH/sketches/$s"
  cp -r "$REPO/tools/build-speed-benchmark/sketches/$s" "$BENCH/sketches/$s"
done

fqbn() {
  local ns="$1" target="$2"
  [[ "$target" == "core" ]] && echo "$ns:esp32:jwplcbasiccore" || echo "$ns:esp32:jwplcbasic"
}

# compile_timed <log> <args...> -> imprime "ms exit"
compile_timed() {
  local log="$1"; shift
  local t0 t1 rc=0
  t0=$(date +%s%N)
  arduino-cli compile -j 0 "$@" > "$log" 2>&1 || rc=$?
  t1=$(date +%s%N)
  echo "$(( (t1 - t0) / 1000000 )) $rc"
}

CSV="$OUT/results.csv"
echo "version,repetition,target,sketch,phase,ms,exit,flash_bytes" > "$CSV"

echo "BASE=$BASE_LABEL ($BASE_REF $(git -C "$BASE_TREE" rev-parse --short HEAD))"
echo "HEAD=$HEAD_LABEL ($(git -C "$REPO" rev-parse --short HEAD)$(git -C "$REPO" diff --quiet || echo '+cambios'))"
echo "ARDUINO_CLI=$(arduino-cli version | head -1)"

for rep in $(seq 1 "$REPS"); do
  for v in base head; do
    label="$BASE_LABEL"; [[ "$v" == "head" ]] && label="$HEAD_LABEL"
    for target in $TARGETS; do
      for s in $SKETCHES; do
        id="${v}_${target}_${s}_r${rep}"
        cache="$BENCH/cache/$id"; build="$BENCH/build/$id"
        rm -rf "$cache" "$build"; mkdir -p "$cache" "$build"
        export ARDUINO_BUILD_CACHE_PATH="$cache"
        args=(--fqbn "$(fqbn "jwplc_$v" "$target")" --build-path "$build" "$BENCH/sketches/$s")

        read -r ms rc < <(compile_timed "$OUT/logs/${id}_cold.log" "${args[@]}")
        flash=$(grep -oE 'Sketch uses [0-9]+' "$OUT/logs/${id}_cold.log" | grep -oE '[0-9]+' || echo 0)
        echo "$label,$rep,$target,$s,cold,$ms,$rc,$flash" >> "$CSV"
        printf '%-16s r%s %-5s %-12s frío     %6.1f s (exit %s)\n' "$label" "$rep" "$target" "$s" "$(echo "$ms/1000" | bc -l)" "$rc"

        touch "$BENCH/sketches/$s/$s.ino"
        read -r ms rc < <(compile_timed "$OUT/logs/${id}_warm.log" "${args[@]}")
        echo "$label,$rep,$target,$s,warm_touch,$ms,$rc,$flash" >> "$CSV"
        printf '%-16s r%s %-5s %-12s caliente %6.1f s (exit %s)\n' "$label" "$rep" "$target" "$s" "$(echo "$ms/1000" | bc -l)" "$rc"
        rm -rf "$cache" "$build"
      done
    done
  done
done

python3 - "$CSV" "$OUT/COMPARE.md" "$BASE_LABEL" "$HEAD_LABEL" "$REPS" "$(arduino-cli version | head -1)" "$(nproc)" <<'PY'
import csv, statistics, sys
src, dst, base, head, reps, cli, cores = sys.argv[1:]
rows = list(csv.DictReader(open(src, encoding="utf-8")))
fails = [r for r in rows if r["exit"] != "0"]
def med(v, t, s, p):
    xs = [int(r["ms"]) for r in rows if (r["version"], r["target"], r["sketch"], r["phase"]) == (v, t, s, p) and r["exit"] == "0"]
    return f"{statistics.median(xs) / 1000:.1f} s".replace(".", ",") if xs else "FALLO"
def flash(v, t, s):
    xs = [r["flash_bytes"] for r in rows if (r["version"], r["target"], r["sketch"]) == (v, t, s) and r["exit"] == "0"]
    return f"{int(xs[0]):,}".replace(",", " ") if xs else "-"
keys = []
for r in rows:
    k = (r["target"], r["sketch"])
    if k not in keys:
        keys.append(k)
out = [f"# Benchmark de compilación: {base} frente a {head}", "",
       f"- {cli}; {cores} núcleos; {reps} repeticiones (mediana); fallos: {len(fails)}", "",
       f"| Placa | Sketch | Frío {base} | Frío {head} | Caliente {base} | Caliente {head} | Flash {base} | Flash {head} |",
       "|---|---|---|---|---|---|---|---|"]
for t, s in keys:
    out.append(f"| {t} | {s} | {med(base, t, s, 'cold')} | {med(head, t, s, 'cold')} | "
               f"{med(base, t, s, 'warm_touch')} | {med(head, t, s, 'warm_touch')} | {flash(base, t, s)} B | {flash(head, t, s)} B |")
out += ["", "Frío = caché de build aislada y vacía + build-path nuevo. Caliente = misma caché y build-path;",
        "sólo cambia la fecha del `.ino` (como pulsar Compilar otra vez en el IDE)."]
open(dst, "w", encoding="utf-8").write("\n".join(out) + "\n")
print("\n".join(out))
sys.exit(1 if fails else 0)
PY
echo "RESULTADOS=$OUT"

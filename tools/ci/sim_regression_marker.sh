#!/usr/bin/env bash
# Evita compilar y simular dos veces el mismo árbol del package (SIM_REGRESSION).
#
# El gate SIM_REGRESSION (JWPLC Sim) tarda ~30-40 min. Corre en el CI del PR
# y, antes de este script, volvía a correr en la Release sobre un commit con
# el MISMO árbol (el merge de «…/main» → release/v2.1.x no cambia archivos).
# Cuando el gate pasa, el CI sube un artefacto «marcador» cuyo nombre es el
# árbol git + el commit de jwplc-sim usado; la Release (y el siguiente CI)
# buscan ese marcador y, si existe, no repiten el barrido.
#
# Uso (en un paso de Actions, tras actions/checkout):
#   tools/ci/sim_regression_marker.sh <sim_ref>
# Escribe en $GITHUB_OUTPUT:
#   name=sim-regression-pass-<árbol>-<sim_ref[0:12]>
#   reuse=true|false
#   run_url=<run que dejó el marcador> (solo si reuse=true)
# Variables: GH_TOKEN (con actions:read), GITHUB_REPOSITORY.
# Solo cuenta marcadores de runs de ramas de este mismo repositorio (no de
# forks) y no vencidos. Cualquier error de la consulta => reuse=false.
set -euo pipefail

SIM_REF="${1:?falta sim_ref}"
TREE="$(git rev-parse 'HEAD^{tree}')"
NAME="sim-regression-pass-${TREE}-${SIM_REF:0:12}"
OUT="${GITHUB_OUTPUT:-/dev/stdout}"

echo "name=$NAME" >> "$OUT"
echo "ARBOL=$TREE"
echo "MARCADOR=$NAME"

RUN_URL=""
if REPO_ID="$(gh api "repos/$GITHUB_REPOSITORY" --jq .id 2>/dev/null)"; then
  RUN_URL="$(gh api -X GET "repos/$GITHUB_REPOSITORY/actions/artifacts" \
      -f name="$NAME" -f per_page=20 \
      --jq "[.artifacts[]
             | select(.expired == false)
             | select(.workflow_run.head_repository_id == $REPO_ID)]
            | first
            | if . == null then \"\" else
                \"https://github.com/$GITHUB_REPOSITORY/actions/runs/\(.workflow_run.id)\" end" \
      2>/dev/null || true)"
fi

if [[ -n "$RUN_URL" ]]; then
  echo "reuse=true" >> "$OUT"
  echo "run_url=$RUN_URL" >> "$OUT"
  echo "SIM_REGRESSION=REUSED ($RUN_URL)"
else
  echo "reuse=false" >> "$OUT"
  echo "SIM_REGRESSION=RUN (no hay marcador para este árbol)"
fi

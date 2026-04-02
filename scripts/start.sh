#!/usr/bin/env bash
# OmniBot — run backend + Vite dashboard (macOS / Linux)
# Run from the repository root after install:  chmod +x scripts/start.sh && ./scripts/start.sh
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BACKEND="$REPO_ROOT/app/backend"
PY="$BACKEND/.venv/bin/python"

if [[ ! -x "$PY" ]]; then
  echo "Backend venv not found. Run ./scripts/install.sh first."
  exit 1
fi

DASHBOARD_URL="http://127.0.0.1:5173"
BACKEND_URL="http://127.0.0.1:8000"

echo ""
echo "Starting OmniBot ..."
echo ""

cleanup() {
  if [[ -n "${BACKEND_PID:-}" ]] && kill -0 "$BACKEND_PID" 2>/dev/null; then
    kill "$BACKEND_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

(
  cd "$BACKEND"
  exec "$PY" app.py
) &
BACKEND_PID=$!

sleep 2

echo "Backend PID $BACKEND_PID (FastAPI on port 8000)."
echo ""
echo "Dashboard:  $DASHBOARD_URL"
echo "API:        $BACKEND_URL"
echo ""
echo "Opening the dashboard shortly after Vite starts. Press Ctrl+C to stop both processes."
echo ""

(
  sleep 6
  if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$DASHBOARD_URL" >/dev/null 2>&1 || true
  elif command -v open >/dev/null 2>&1; then
    open "$DASHBOARD_URL" >/dev/null 2>&1 || true
  fi
) &

cd "$REPO_ROOT/app/frontend"
npm run dev

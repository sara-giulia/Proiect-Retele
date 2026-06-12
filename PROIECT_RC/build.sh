#!/bin/bash
# Script pentru compilarea proiectului VirtualSoc

set -e

mkdir -p bin

echo "[1/2] compilare server..."
gcc src/server.c -o bin/server -lpthread -lsqlite3 -lcrypto -lssl
echo "    server compilat -> bin/server"

echo "[2/2] compilare client (Qt)..."
qmake client.pro
make
echo "    client compilat -> bin/client"

echo ""
echo "Build terminat. Ruleaza:"
echo "  ./bin/server          (in alt terminal)"
echo "  ./bin/client 127.0.0.1 2908"

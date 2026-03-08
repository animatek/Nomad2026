#!/bin/bash
# Run Nomad2026 with ALL output visible

cd /mnt/SPEED/CODE/Nomad2026

echo "==================================================================="
echo "  Nomad2026 - Full Debug Output"
echo "==================================================================="
echo ""

./build/Nomad2026_artefacts/Release/Nomad2026 2>&1

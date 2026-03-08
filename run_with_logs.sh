#!/bin/bash
# Run Nomad2026 with debug logging visible

cd /mnt/SPEED/CODE/Nomad2026

echo "==================================================================="
echo "  Nomad2026 - Debug Mode"
echo "==================================================================="
echo "All debug messages will appear below."
echo "Look for messages like:"
echo "  - 'Patch synchronizer enabled'"
echo "  - 'Sent CableInsert:'"
echo "  - 'Sent ModuleMove:'"
echo "  - 'Sent StorePatch:'"
echo "==================================================================="
echo ""

./build/Nomad2026_artefacts/Release/Nomad2026 2>&1 | grep -E "(DBG|Sent|synchronizer|StorePatch|Cable|Module)"

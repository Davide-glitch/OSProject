#!/bin/bash
echo "Compiling treasure_manager.c..."

gcc treasure_manager.c -o treasure_manager -lm

if [ $? -eq 0 ]; then
    echo "Compilation of treasure_manager successful!"
    chmod +x treasure_manager
else
    echo "Compilation of treasure_manager failed. Please check for errors."
    exit 1
fi

echo "Compiling treasure_hub.c..."
gcc treasure_hub.c -o treasure_hub -lm

if [ $? -eq 0 ]; then
    echo "Compilation of treasure_hub successful!"
    chmod +x treasure_hub
else
    echo "Compilation of treasure_hub failed. Please check for errors."
    exit 1
fi

echo "Compiling treasure_monitor.c..."
gcc treasure_monitor.c -o treasure_monitor -lm

if [ $? -eq 0 ]; then
    echo "Compilation of treasure_monitor successful!"
    chmod +x treasure_monitor
else
    echo "Compilation of treasure_monitor failed. Please check for errors."
    exit 1
fi

echo "All compilations successful! Use ./treasure_manager for direct management or ./treasure_hub for the interactive interface."

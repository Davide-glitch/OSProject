#!/bin/bash
echo "Compiling treasure_manager.c..."

gcc treasure_manager.c -o run -lm


if [ $? -eq 0 ]; then
    echo "Compilation successful! Executable created: run. Use ./run to run the program."
    chmod +x run
else
    echo "Compilation failed. Please check for errors."
    exit 1
fi

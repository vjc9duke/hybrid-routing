#!/bin/bash

CONFIG_FILE="runfiles/customconfig"  # Change this to your actual file
BINARY="./src/booksim"        # Change this to your actual binary
OUTPUT_FILE="uniform.txt"
PYTHON_SCRIPT="update_excel.py"

K_VALUE=("4" "5" "6" "7" "8" "9" "10")
TRAFFIC_PATTERNS=("uniform")
ROUTING_FUNCTIONS=("custom" "dor")
TIMEOUT_DURATION=120

trap "echo 'Script interrupted. Exiting...'; exit 1" SIGINT


for routing in "${ROUTING_FUNCTIONS[@]}"; do
  CSV_FILE="${routing}_scale.csv"
  echo -n "K" > "$CSV_FILE"  # First column label
    for rate in $(seq 0.01 0.01 0.70); do
        echo -n ",$rate" >> "$CSV_FILE"  
    done
  echo "" >> "$CSV_FILE"
  sed -i "0,/^routing_function = .*/s//routing_function = $routing;/" "$CONFIG_FILE"
  for K in "${K_VALUE[@]}"; do  
      echo -n "$K" >> "$CSV_FILE"  
      sed -i "0,/^k  = .*/s//k  = $K;/" "$CONFIG_FILE"
    for rate in $(seq 0.01 0.01 0.70); do  # Increment by 0.01 up to 0.20
        # Modify only the first occurrence of injection_rate in the config file
        sed -i "0,/^injection_rate = .*/s//injection_rate = $rate;/" "$CONFIG_FILE"
    
        echo "$BINARY $CONFIG_FILE > $OUTPUT_FILE"
    
        # Run the binary with the modified config file
        timeout $TIMEOUT_DURATION $BINARY "$CONFIG_FILE" > "$OUTPUT_FILE"
        EXIT_STATUS=$?
    
        if [ $EXIT_STATUS -eq 124 ]; then
                echo "Simulation timed out for Traffic: $traffic, Routing: $routing, Injection Rate: $rate. Skipping..."
                break
        fi
        
        echo "Executed binary" 
    
        # Check if the simulation aborted due to latency exceeding 500 cycles
        if grep -q "Average latency for class 0 exceeded 500 cycles. Aborting simulation" "$OUTPUT_FILE"; then
            echo "Simulation aborted at injection rate $rate due to high latency."
            break
        fi
    
        # Extract only the LAST occurrence of "Packet latency average"
        latency=$(awk '/Packet latency average/ {last=$5} END {print last}' "$OUTPUT_FILE")
    
        echo "Injection Rate: $rate, Packet Latency Average: $latency"
        
        echo "$rate $K $latency"
        
        python3 "$PYTHON_SCRIPT" "$CSV_FILE" "$K" "$rate" "$latency"
    done
    echo "" >> "$CSV_FILE"  # Add a newline after each routing function
  done
done
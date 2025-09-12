#!/bin/bash

# Data Center Traffic Workload Test Script
# This script runs the leaf-spine topology simulation with different realistic traffic workloads

echo "Data Center DCTCP with Realistic Traffic Workloads"
echo "=================================================="

# Default parameters
SIMULATION_TIME=10
ECN_CONFIG="SECN1"
FLOW_RATE=200

# Function to run simulation with specific parameters
run_simulation() {
    local workload=$1
    local load=$2
    local ecn=$3
    local time=$4
    local rate=$5
    
    echo ""
    echo "Running simulation:"
    echo "- Workload: $workload"
    echo "- Load: ${load}%"
    echo "- ECN Config: $ecn"
    echo "- Simulation Time: ${time}s"
    echo "- Flow Rate: $rate flows/sec"
    echo ""
    
    ./ns3 run "leaf-spine-topology --workloadType=$workload --networkLoad=0.$load --ecnConfig=$ecn --simulationTime=$time --flowArrivalRate=$rate --enableRealisticTraffic=true"
    
    echo "Simulation completed. Check output above for results."
    echo "----------------------------------------"
}

# Check if ns3 exists
if [ ! -f "./ns3" ]; then
    echo "Error: ns3 build script not found. Please run from ns-3 root directory."
    exit 1
fi

# Build the project first
echo "Building project..."
./ns3 build

if [ $? -ne 0 ]; then
    echo "Build failed. Please fix compilation errors."
    exit 1
fi

echo "Build successful!"
echo ""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --all)
            RUN_ALL=true
            shift
            ;;
        --workload)
            WORKLOAD="$2"
            shift 2
            ;;
        --load)
            LOAD="$2"
            shift 2
            ;;
        --ecn)
            ECN_CONFIG="$2"
            shift 2
            ;;
        --time)
            SIMULATION_TIME="$2"
            shift 2
            ;;
        --rate)
            FLOW_RATE="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --all              Run all test configurations"
            echo "  --workload TYPE    Workload type (WEB_SEARCH or DATA_MINING)"
            echo "  --load PERCENT     Network load (60, 70, 80, 90)"
            echo "  --ecn CONFIG       ECN configuration (SECN1 or SECN2)"
            echo "  --time SECONDS     Simulation time"
            echo "  --rate FLOWS       Flow arrival rate per second"
            echo "  --help             Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0 --workload WEB_SEARCH --load 70"
            echo "  $0 --all"
            echo "  $0 --workload DATA_MINING --load 80 --ecn SECN2"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information."
            exit 1
            ;;
    esac
done

if [ "$RUN_ALL" = true ]; then
    echo "Running comprehensive test suite..."
    echo "This will take a while (multiple simulations)..."
    
    # Test different workloads and loads
    for workload in "WEB_SEARCH" "DATA_MINING"; do
        for load in 60 70 80 90; do
            for ecn in "SECN1" "SECN2"; do
                run_simulation $workload $load $ecn $SIMULATION_TIME $FLOW_RATE
            done
        done
    done
    
    echo ""
    echo "All test simulations completed!"
    
else
    # Run single simulation with specified or default parameters
    WORKLOAD=${WORKLOAD:-"WEB_SEARCH"}
    LOAD=${LOAD:-70}
    
    run_simulation $WORKLOAD $LOAD $ECN_CONFIG $SIMULATION_TIME $FLOW_RATE
fi

echo ""
echo "To analyze results, check the simulation output above."
echo "For more detailed analysis, consider enabling packet tracing:"
echo "  ./ns3 run \"leaf-spine-topology --workloadType=WEB_SEARCH --networkLoad=0.7\" > results.txt"

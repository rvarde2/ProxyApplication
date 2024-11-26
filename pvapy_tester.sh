#!/bin/bash

# Check if two arguments are provided
if [ $# -ne 2 ]; then
    echo "Usage: $0 <IP_ADDRESS:PORT> <producer/consumer>"
    exit 1
fi

# Assign arguments to variables
export EPICS_PVA_NAME_SERVER=$1
MODE=$2
CHANNEL_NAME=pvapy:image

# Check the mode and display appropriate message
if [ "$MODE" == "producer" ]; then
    pvapy-ad-sim-server -cn $CHANNEL_NAME -nx 128 -ny 128 -dt uint8 -rt 60 -fps 10000 -rp 10000
elif [ "$MODE" == "consumer" ]; then
    pvapy-hpc-consumer \
    --input-channel $CHANNEL_NAME \
    --control-channel consumer:*:control \
    --status-channel consumer:*:status \
    --output-channel consumer:*:output \
    --processor-class pvapy.hpc.userDataProcessor.UserDataProcessor \
    --report-period 10
else
    echo "Invalid mode. Please use 'producer' or 'consumer'."
    exit 1
fi

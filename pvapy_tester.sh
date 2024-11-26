#!/bin/bash

# Check if two arguments are provided
if [ $# -ne 3 ]; then
    echo "Usage: $0 <producer ip> <port> <producer/consumer>"
    exit 1
fi

MODE=$3
CHANNEL_NAME=pvapy:image
FRAME_RATE=1
DURATION=10
REPORT_RATE=1
export EPICS_PVA_BROADCAST_PORT=1000

# Check the mode and display appropriate message
if [ "$MODE" == "producer" ]; then
    export EPICS_PVA_SERVER_PORT=$2
    pvapy-ad-sim-server -cn $CHANNEL_NAME -nx 128 -ny 128 -dt uint8 -rt $DURATION -fps $FRAME_RATE -rp $REPORT_RATE
elif [ "$MODE" == "consumer" ]; then
    export EPICS_PVA_NAME_SERVER=$1:$2
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

#!/bin/bash

setpoint=${1:-20.0}

name=beerduino
device_id=`particle list ${name} | grep ${name} | sed 's/.*\[\(.*\)\].*/\1/'`
access_token=`particle config identify | grep "Access token" | sed 's/Access token: \(.*\)/\1/'`

curl https://api.particle.io/v1/devices/${device_id}/setpoint -d "access_token=${access_token}" -d "args=${setpoint}"
echo

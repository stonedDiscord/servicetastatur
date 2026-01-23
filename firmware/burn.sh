#!/bin/bash

# Check if file exists and get its size


cat service_tastatur_v3.3.u3 service_tastatur_v3.3.u3 > c_8.bin
cat c_8.bin c_8.bin > c_64.bin
rm c_8.bin
cat c_64.bin c_64.bin > c_128.bin
rm c_64.bin

minipro -p "W27C512@DIP28" -w c_128.bin
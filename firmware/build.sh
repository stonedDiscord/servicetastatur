rm .pio/build/at89s52/firmware.hex
rm service_tastatur_v3.3.u3
pio run
objcopy -Iihex -Obinary .pio/build/at89s52/firmware.hex service_tastatur_v3.3.u3

if [ ! -f service_tastatur_v3.3.u3 ]; then
    echo "Error: service_tastatur_v3.3.u3 not found"
    exit 1
fi

current_size=$(stat -c%s service_tastatur_v3.3.u3)
if [ $current_size -lt 8192 ]; then
    echo "Padding service_tastatur_v3.3.u3 from $current_size to 8192 bytes"
    dd if=/dev/zero bs=1 count=$((8192 - current_size)) >> service_tastatur_v3.3.u3
fi
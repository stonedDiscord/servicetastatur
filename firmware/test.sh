./build.sh

rm servicet.zip
mkdir roms
7z a servicet.zip service_tastatur_v3.3.u3

cp servicet.zip /run/media/stoned/schrott/Roms/mame/roms/

cd /run/media/stoned/schrott/msys64/src/mame/
/run/media/stoned/schrott/msys64/src/mame/mame servicet

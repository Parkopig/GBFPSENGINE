@echo off
set GBDK=C:\gbdk
%GBDK%\bin\lcc -Wa-l -Wl-m -Wl-j -Wl-yt0 -Wl-yo4 -Wl-ya0    -o build/raycaster.gb    main.c enemy.c overlay.c zombie_sheet.c zombie_near.c zombie_far.c zombie_attack.c health.c 
echo Build complete.
pause

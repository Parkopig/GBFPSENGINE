//set PATH=C:\gbdk\bin;%PATH%
//lcc -Wa-l -Wl-m -Wl-j -Wl-yt0 -Wl-yo4 -Wl-ya0    -o build/raycaster.gb    main.c enemy.c overlay.c zombie_sheet.c zombie_near.c zombie_far.c zombie_attack.c health.c


PROJECT = raycaster
SOURCES = main.c
CC = lcc

CFLAGS = -Wa-l -Wl-m -Wl-j

$(PROJECT).gb: $(SOURCES)
    $(CC) $(CFLAGS) -o $(PROJECT).gb $(SOURCES)

clean:
    del *.o
    del *.gb
    del *.ihx
    del *.lst
    del *.map
    del *.sym
#/bin/sh
# gcc -shared `pkg-config hildon-1 libhildondesktop-1 --libs --cflags` desktop-cmd-exec.c -o desktop-cmd-exec.so
gcc -shared `pkg-config hildon-1 libhildondesktop-1 conic --libs --cflags` desktop-cmd-exec.c -o desktop-cmd-exec.so
cp desktop-cmd-exec.so `pkg-config libhildondesktop-1 --variable=hildondesktoplibdir`
cp desktop-cmd-exec.desktop `pkg-config libhildondesktop-1 --variable=hildonhomedesktopentrydir`
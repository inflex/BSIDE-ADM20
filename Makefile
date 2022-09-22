# 
# VERSION CHANGES
#

# the ?=  sets a default value if not defined already via ENV
FAKE_SERIAL ?= 0
BV=$(shell (git rev-list HEAD --count))
BD=$(shell (date))

LOCATION=/usr/local
CFLAGS=-O -DBUILD_VER="$(BV)"  -DBUILD_DATE=\""$(BD)"\" -DFAKE_SERIAL=$(FAKE_SERIAL)
LIBS=
WINLIBS=-lgdi32 -lcomdlg32 -lcomctl32 -lmingw32
WINCC=i686-w64-mingw32-g++
# -fpermissive is needed to stop the warnings about casting stoppping the build
# -municode eliminates the WinMain@16 link error when we're using wWinMain
#WINFLAGS=-municode -static-libgcc -fpermissive -static-libstdc++
WINFLAGS=-municode -static-libgcc -static-libstdc++

WINOBJ=bside-adm20.exe
OFILES=

default: 
	@echo
	@echo "   For OBS command line tool: make bside-adm20"
	@echo
	@echo "   To make a GUI test, export FAKE_SERIAL=1 && make bside-adm20"
	@echo

.c.o:
	${CC} ${CFLAGS} $(COMPONENTS) -c $*.c

all: ${OBJ} 

bside-adm20: ${OFILES} bside-adm20.cpp 
	@echo Build Release $(BV)
	@echo Build Date $(BD)
#	ctags *.[ch]
#	clear
	${WINCC} ${CFLAGS} ${WINFLAGS} $(COMPONENTS) bside-adm20.cpp ${OFILES} -o bside-adm20.exe ${LIBS} ${WINLIBS}

strip: 
	strip *.exe

install: ${OBJ}
	cp bside-adm20 ${LOCATION}/bin/

clean:
	rm -f *.o *core ${OBJ} ${WINOBJ}

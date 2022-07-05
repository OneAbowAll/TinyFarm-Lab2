# definizione del compilatore e dei flag di compilazione
# che vengono usate dalle regole implicite
CC=gcc
CFLAGS=-g -Wall -O -std=c99
LDLIBS=-lm -lrt -pthread

# eseguibili da costruire
EXECS=farm

.PHONY: clean

all: $(EXECS)

farm: farm.o xerrori.o xsocket.o

#teoricamente make dovrebbe capire da solo come costruire xsocket.o, quindi questi non dovrebbero servire
xsocket.o: xsocket.c xsocket.h

xerrori.o: xerrori.c xerrori.h

# target che cancella eseguibili e file oggetto
clean:
	rm -f $(EXECS) *.o  *.zip
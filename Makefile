HEADERS = osmo-fl2k.h
INCLUDES=-I/usr/include/libusb-1.0/
LDFLAGS=-lusb-1.0 -pthread -lm

default: fl2k-psk

fl2k-psk.o: fl2k-psk.c $(HEADERS)
	g++ -std=c++11 -c fl2k-psk.c -o fl2k-psk.o $(INCLUDES)

libosmo-fl2k.o: libosmo-fl2k.c $(HEADERS)
	gcc -c libosmo-fl2k.c -o libosmo-fl2k.o  $(INCLUDES)

fl2k-psk: fl2k-psk.o libosmo-fl2k.o
	g++ -std=c++11 -ggdb fl2k-psk.o libosmo-fl2k.o -o fl2k-psk $(LDFLAGS)

clean:
	-rm -f fl2k-psk.o
	-rm -f libosmo-fl2k.o
	-rm -f fl2k-psk

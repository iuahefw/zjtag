CC       = gcc
LIBS     = -lftd2xx -lusb -ldl -lrt -lpthread -Wl,--gc-sections
OBJ      = obj/zjtag.o obj/busbasp.o obj/ftdixx.o obj/j-link.o obj/libusb.o obj/stmhid.o
INCS     = -I"./src"
BIN      = zjtag
CFLAGS   = $(INCS) -O2 -Wall -ffunction-sections -fdata-sections -static -s
RM       = rm -f

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LIBS)

obj/zjtag.o: src/zjtag.c
	$(CC) -c src/zjtag.c -o obj/zjtag.o $(CFLAGS)

obj/busbasp.o: src/busbasp.c
	$(CC) -c src/busbasp.c -o obj/busbasp.o $(CFLAGS)

obj/ftdixx.o: src/ftdixx.c
	$(CC) -c src/ftdixx.c -o obj/ftdixx.o $(CFLAGS)

obj/j-link.o: src/j-link.c
	$(CC) -c src/j-link.c -o obj/j-link.o $(CFLAGS)

obj/libusb.o: src/libusb.c
	$(CC) -c src/libusb.c -o obj/libusb.o $(CFLAGS)

obj/stmhid.o: src/stmhid.c
	$(CC) -c src/stmhid.c -o obj/stmhid.o $(CFLAGS)

clean:
	${RM} $(OBJ) $(BIN)

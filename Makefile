all: nushu_sender.o nushu_receiver.o nushu_hider.o


KHDRS=/usr/src/linux/include

nushu_sender.o: sender.o crypt.o d3des.o misc.o
	ld -r -o nushu_sender.o sender.o crypt.o d3des.o misc.o

nushu_receiver.o: receiver.o crypt.o d3des.o misc.o
	ld -r -o nushu_receiver.o receiver.o crypt.o d3des.o misc.o

d3des.o: d3des.c d3des.h
misc.o: misc.c
	gcc -c misc.c -I $(KHDRS) -D__OPTIMIZE__

crypt.o: crypt.c nushu.h
	gcc -c crypt.c -I $(KHDRS) -D__OPTIMIZE__

sender.o: sender.c nushu.h 
	gcc -c sender.c -I $(KHDRS) -finline -D__OPTIMIZE__

receiver.o: receiver.c nushu.h
	gcc -c receiver.c -I $(KHDRS) -finline -D__OPTIMIZE__

nushu_hider.o: pfpacket_cheater.o lkh.o
	ld -r -o nushu_hider.o pfpacket_cheater.o lkh.o
pfpacket_cheater.o: pfpacket_cheater.c
	gcc -c pfpacket_cheater.c -I $(KHDRS) -finline -D__OPTIMIZE__
 
lkh.o: lkh.c lkh.h
	gcc -c lkh.c -I $(KHDRS)

clean:
	rm -f *.o

	



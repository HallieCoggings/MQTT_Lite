all : broker sub pub

broker : broker.c
	gcc -Wall broker.c -o broker.exe

sub : sub.c
	gcc -Wall sub.c -o sub.exe

pub : pub.c
	gcc -Wall pub.c -o pub.exe

clear:
	rm *.exe

makefile: Matrix.c Semaphore.c Master.c Pawn.c Player.c
	gcc -c -std=c89 -pedantic Matrix.c -o Matrix.o
	gcc -c -std=c89 -pedantic Semaphore.c -o Semaphore.o
	gcc -c -std=c89 -pedantic Player.c -o Player.o
	gcc -c -std=c89 -pedantic Pawn.c -o Pawn.o
	gcc -std=c89 -pedantic Master.c Matrix.o Semaphore.o Player.o Pawn.c -o Master
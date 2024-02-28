# Makefile to make 3d_magic picture generator.

CC=cc
OBJS=rwgif.o 3d.o scene.o
PROG=3d
CFLAGS= -O
#CFLAGS= -g
LIBS= -lm

3d_magic:	$(OBJS)
		$(CC) $(CFLAGS) -o $(PROG) $(OBJS) $(LIBS)
		strip $(PROG)

clean:
		/bin/rm *.o 3d

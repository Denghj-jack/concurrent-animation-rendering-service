CC = cc
CFLAGS = -fno-sanitize=all -fPIC -Wvla -Werror -fsanitize=address -g

OBJS = canvas.o sprite.o placement.o

animate.o: $(OBJS)
	ld -r -o animate.o $(OBJS)

canvas.o: canvas.c animate_internal.h animate.h
	$(CC) $(CFLAGS) -c canvas.c -o canvas.o

sprite.o: sprite.c animate_internal.h animate.h
	$(CC) $(CFLAGS) -c sprite.c -o sprite.o

placement.o: placement.c animate_internal.h animate.h
	$(CC) $(CFLAGS) -c placement.c -o placement.o

clean:
	rm -f *.o
default: DSSH.c
	gcc -o dssh DSSH.c

run:
	./dssh

clean:
	rm ./dssh

CC = gcc
CFLAGS = -std=c23 -Wall -Wextra -Wpedantic

program: main.c
	${CC} ${CFLAGS} main.c -o program

test: program
	python test.py

.PHONY: test
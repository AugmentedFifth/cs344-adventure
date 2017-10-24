comitoz.buildrooms: comitoz.buildrooms.c
	gcc -o comitoz.buildrooms comitoz.buildrooms.c -O -g -Werror -Wextra -Wall -save-temps -Wshadow -fmudflap -ftrapv -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wstrict-overflow=5 -Wwrite-strings -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wconversion -Wunreachable-code -Wformat=2 -Winit-self

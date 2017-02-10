all: test.exe

test.exe: test.c
	wcl -0 -s -d0 -lr -ms -we -wx -fm=test.map -os test.c

clean: .symbolic
	if exist test.exe del test.exe

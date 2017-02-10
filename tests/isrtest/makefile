all: test.exe

test.exe: test.c
	wcl -0 -s -zp1 -d0 -lr -ms -we -wx -k2048 -fm=test.map -os test.c

clean: .symbolic
	if exist test.exe del test.exe

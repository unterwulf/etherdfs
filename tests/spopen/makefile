all: test.com

test.com: test.c
	wcl -0 -s -d0 -lr -mt -we -wx -os test.c

clean: .symbolic
	if exist test.com del test.com

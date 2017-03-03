all: test.com

test.com: test.c
	wcl -0 -lr -mt -we -wx -d0 test.c

clean: .symbolic
	if exist test.com del test.com

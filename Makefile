CC=gcc
#CC=g++

#CPPFLAGS=[-fpermissive] -Wwrite-strings
#gcc -shared -fPIC $(@:dll=o) -static-libgcc -o $@

CPPFLAGS= 

all:sxml.a sxml.dll test
sxml.o:
	$(CC) $(CPPFLAGS) -c sxml.c -o sxml.o -g 

sxml.a:sxml.o
	ar -rcs $@ $^
sxml.dll:sxml.c
	gcc -shared -static-libgcc $^ -o $@
#	gcc -shared -fPIC -static-libgcc $^ -o $@
	
test:test.c sxml.a
	$(CC) $(CPPFLAGS) $^ -o $@ -g
	
.PHONY:clean
clean:
	@rm -rf *.o *.a *.exe

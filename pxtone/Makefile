libpxtone.a: *.cpp *.h
	g++ -O3 -c -fPIC *.cpp
	ar rcs libpxtone.a *.o

.PHONY: clean
clean:
	rm -f *.a
	rm -f *.o

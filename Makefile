.PHONY: all
all: main

main: *.cpp *.hpp midifile/lib/libmidifile.a
	g++ -g -std=c++1z *.cpp -o main -L../pxtone/lib -L./midifile/lib -lpxtone -lmidifile -I../pxtone -I./midifile/include

clean:
	rm -f main
	rm -f vgcore.*

midifile/lib/libmidifile.a: libmidifile
	@# Do nothing

.PHONY: libmidifile
libmidifile:
	$(MAKE) -C ./midifile library

.PHONY: all
all: main

main: convert.cpp main.cpp pitch_bend.cpp pttypes.cpp *.hpp midifile/lib/libmidifile.a pxtone/libpxtone.a
	g++ -g -std=c++1z *.cpp -o main -L./pxtone -L./midifile/lib -lpxtone -lmidifile -I./midifile/include

clean:
	rm -f main
	rm -f vgcore.*
	$(MAKE) -C ./midifile clean
	$(MAKE) -C ./pxtone clean

pxtone/libpxtone.a: libpxtone
	@# Do nothing

.PHONY: libpxtone
libpxtone:
	$(MAKE) -C ./pxtone

midifile/lib/libmidifile.a: libmidifile
	@# Do nothing

.PHONY: libmidifile
libmidifile:
	$(MAKE) -C ./midifile library

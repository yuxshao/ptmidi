main: *.cpp *.hpp
	g++ -g -std=c++1z *.cpp -o main -L../pxtone/lib -L../midifile/lib -lpxtone -lmidifile -I../pxtone -I../midifile/include

clean:
	rm -f main
	rm -f vgcore.*

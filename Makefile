
renderText: main.o
	gcc -g -o renderText main.o -L/usr/local/Cellar/freetype/2.8/lib/ -lfreetype

main.o: main.cpp
	gcc -g -c -I/usr/local/Cellar/freetype/2.8/include/freetype2/ main.cpp

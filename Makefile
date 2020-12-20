
renderText: main.o
	gcc -g -o renderText main.o -L/usr/local/Cellar/freetype/2.10.4/lib/ -lfreetype

main.o: main.cpp
	gcc -g -c -I/usr/local/Cellar/freetype/2.10.4/include/freetype2/ main.cpp


all:
	g++ -O3 main.cpp -o alien_game -lpthread -lncurses

clean:
	rm -f alien_game
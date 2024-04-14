#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

struct termios orig_termios;

//Restore terminal attributes
void disableRawMode(){
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

//RawMode: the terminal send each character it gets in it to the computer (computer keeps track of your moves <:) )
void enableRawMode(){
	tcgetattr(STDIN_FILENO, &orig_termios);
	atexit(disableRawMode);
	struct termios raw = orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON); 
	tcsetattr(STDIN_FILENO, TCSAFLUSH,&raw);
}


int main(){
	enableRawMode();

	char c;
	while(read(STDIN_FILENO, &c, 1)==1 && c != 'q');

	return 0;
}
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

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
	raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP |IXON | ICRNL); //cntrl+s & cntrl+q //ICRNL: cntrl+m & Enter
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); //ISIG: cntrl+c & cntrl+z //IEXTEN: cntrl+o & ctrl+v 
	raw.c_cflag |= ~(CS8);
	raw.c_oflag &= ~(OPOST);
	tcsetattr(STDIN_FILENO, TCSAFLUSH,&raw);
}


int main(){
	enableRawMode();

	char c;
	while(read(STDIN_FILENO, &c, 1)==1 && c != 'q')
	{
		if(iscntrl(c))
			printf("%d\r\n", c);
		else
			printf("%d ('%c')\r\n",c,c);	
	}
		

	return 0;
}
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>

#define CTRL_KEY(k) ((k) & 0x1f)

struct editorConfig{
	struct termios orig_termios;
	int screenrows;
	int screencols;
};

struct editorConfig E;


//exit the program whenever an error occurs
void die(const char *s){
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO,"\x1b[H",3);
	perror(s);
	exit(1);
}

//Restore terminal attributes
void disableRawMode(){
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios)==-1)
		die("tcsetattr");
}

//RawMode: the terminal send each character it gets in it to the computer (computer keeps track of your moves <:) )
void enableRawMode(){
	if(tcgetattr(STDIN_FILENO, &E.orig_termios)==-1) die("tcgetattr");
	atexit(disableRawMode);
	struct termios raw = E.orig_termios;
	raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP |IXON | ICRNL); //cntrl+s & cntrl+q //ICRNL: cntrl+m & Enter
	raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); //ISIG: cntrl+c & cntrl+z //IEXTEN: cntrl+o & ctrl+v 
	raw.c_cflag |= ~(CS8);
	raw.c_oflag &= ~(OPOST);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH,&raw)==-1) die("tcsetattr");
}

char editorReadKey(){
	char c;
	int nread;
	while((nread=(read(STDIN_FILENO, &c, 1)))!=1){
		if(nread == -1) exit(1);
	}
	return c;
}

void editorProcessKeypress(){
	char c = editorReadKey();
	switch (c){
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO,"\x1b[H",3);
			exit(0);
			break;
	}
}

void editorDrawRows(){
	int y;
	for(y=0;y<E.screenrows;y++){
		write(STDOUT_FILENO,"~",1);
		if(y < E.screenrows -1)
			write(STDOUT_FILENO, "\r\n",2);
	}	
}

void editorRefreshScreen(){
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO,"\x1b[H",3); //reposition the cursor at top-left corner
	editorDrawRows();
	write(STDOUT_FILENO,"\x1b[H",3);
}


int getCusorPosition(int *rows, int *cols){
	if(write(STDOUT_FILENO, "\x1b[6n",4)!=4) return -1; // query the cursor position

	char buf[32];
	unsigned int i = 0;

	while(i<sizeof(buf)-1){
		if(read(STDIN_FILENO, &buf[i], 1)!=1) break;
		if(buf[i]=='r') break;
		i++;
	}

	buf[i] = '\0';
	if(buf[0]!='\x1b' || buf[1]!='[') return -1;
	if(sscanf(&buf[2],"%d;%d",rows, cols) != 2) return -1;
	
	return 0;
}

int getWindowSize(int *rows, int *cols){
	struct winsize ws;
	if((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==-1) || (ws.ws_col == 0)){
		if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12)!=12) return -1;
		return getCusorPosition(rows, cols);
	}
	else{
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

void initEditor(){
	if(getWindowSize(&E.screenrows, &E.screencols)==-1) die("getWindowSize");
}


int main() {
  enableRawMode();
  initEditor();

  while (1) {
  	editorRefreshScreen();
  	editorProcessKeypress();
  }
  return 0;
}
	

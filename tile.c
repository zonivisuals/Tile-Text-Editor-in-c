#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

#define CTRL_KEY(k) ((k) & 0x1f)
#define TILE_VERSION "0.0.1"

struct editorConfig{
	int cx,cy;
	struct termios orig_termios;
	int screenrows;
	int screencols;
};

struct editorConfig E;

struct abuf{
	char *data;
	int len;
};

#define ABUF_INIT {NULL,0};

void abAppend(struct abuf *ab, const char *s, int len){
	char *new = realloc(ab->data, ab->len + len);
	if (!new) return;
	//update the buff
	memcpy(&new[ab->len],s,len);/*[ab->len]behaves like offset to add the new data to the old one*/
	ab->data = new;
	ab->len += len;
}

void freeAb(struct abuf *ab){
	free(ab->data);
}

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

	if(c == '\x1b'){
		char seq[3];
		if(read(STDIN_FILENO, &seq[0], 1)!=1) return '\x1b';
		if(read(STDIN_FILENO, &seq[1], 1)!=1) return '\x1b';

		if(seq[0] == '['){
			switch(seq[1]){
				case 'A': return 'z';
				case 'B': return 's';
				case 'C': return 'd';
				case 'D': return 'q';
			}
		}
		return '\x1b';

	}
	else return c;
}

void editorMoveCursor(char key){
	switch(key){
		case 'z':
			E.cx--;
			break;
		case 'q':
			E.cy--;
			break;
		case 'd':
			E.cy++;
			break;
		case 's':
			E.cx++;
			break;
	}
}

void editorProcessKeypress(){
	char c = editorReadKey();
	switch (c){
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO,"\x1b[H",3);
			exit(0);
			break;
		case 'z':
		case 'q':
		case 'd':
		case 's':
			editorMoveCursor(c);
			break;
	}
}

void editorDrawRows(struct abuf *ab){
	int y;
	for(y=0;y<E.screenrows;y++){
		if(y == E.screenrows/3){
			char welcome[80];
			char copyright[50];
			int welcomelen = snprintf(welcome, sizeof(welcome), "WELCOME TO TILE VERSION -- %s\n\r",TILE_VERSION);
			int copyrightlen = snprintf(copyright,sizeof(copyright),"BUILT BY ZONI");
			
			if(welcomelen > E.screencols) welcomelen = E.screencols;
			if(copyrightlen > E.screencols) copyrightlen = E.screencols;
			int colPadding = (E.screencols - welcomelen) / 2;

			while(colPadding--) abAppend(ab," ",1);
			abAppend(ab,welcome,welcomelen);
			colPadding = (E.screencols - copyrightlen) / 2;
			while(colPadding--) abAppend(ab," ",1);
			abAppend(ab,copyright,copyrightlen);
		}
		abAppend(ab,"\x1b[K",3);
		if(y < E.screenrows -1) abAppend(ab, "\r\n",2);
	}	
}

void editorRefreshScreen(){
	struct abuf ab = ABUF_INIT;
	
	abAppend(&ab,"\x1b[?25l",6);
	abAppend(&ab,"\x1b[H",3); //reposition the cursor at top-left corner
	
	editorDrawRows(&ab);
	
	char buf[50];
	snprintf(buf,sizeof(buf),"\x1b[%d;%dH",E.cx+1,E.cy+1);
	abAppend(&ab,buf,strlen(buf));


	abAppend(&ab,"\x1b[?25h",6);

	write(STDOUT_FILENO, ab.data, ab.len);
  	freeAb(&ab);
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
	E.cx = 0;
	E.cy = 0;
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
	
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

enum editorKey{
	ARROW_LEFT = 1000,
	ARROW_UP,
	ARROW_RIGHT,
	ARROW_DOWN,
	DEL_KEY,
	HOME_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

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

int editorReadKey(){
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
			if(seq[1]>='0' && seq[1]<='9'){
				if(read(STDIN_FILENO,&seq[2],1)!=1) return '\x1b';
				if(seq[2]=='~'){
					switch(seq[1]){
						case '1':
						case '7':
							return HOME_KEY;
						case '3':
							return DEL_KEY;
						case '4':
						case '8':
							return END_KEY;
						case '5':
							return PAGE_UP;
						case '6':
							return PAGE_DOWN;
					}
				}
			}
			else{
				switch(seq[1]){
				case 'A': return ARROW_UP;
				case 'B': return ARROW_DOWN;
				case 'C': return ARROW_RIGHT;
				case 'D': return ARROW_LEFT;
				case 'F': return END_KEY;
				case 'H': return HOME_KEY;
					
				}
			}
			
		}
		else if(seq[0]=='O'){
			switch(seq[1]){
				case 'H': return HOME_KEY;
				case 'F': return END_KEY;
			}
		}
		return '\x1b';
	}
	else return c;
}

void editorMoveCursor(int key){
	switch(key){
		case ARROW_UP:
			if(E.cy>0) E.cy--;	
			break;
		case ARROW_LEFT:
			if(E.cx>0) E.cx--;
			break;
		case ARROW_RIGHT:
			if(E.cx<E.screencols-1) E.cx++;
			break;
		case ARROW_DOWN:
			if(E.cy<E.screenrows-1) E.cy++;
			break;
	}
}

void editorProcessKeypress(){
	int c = editorReadKey();
	switch (c){
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO,"\x1b[H",3);
			exit(0);
			break;

		case HOME_KEY:
			E.cx = 0;
			break;
		case END_KEY:
			E.cx = E.screencols -1;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
		{
			int times = E.screenrows;
			while(times--){
				editorMoveCursor((c==PAGE_UP) ? ARROW_UP : ARROW_DOWN);
			}
		}
		break;
		case ARROW_UP:
		case ARROW_LEFT:
		case ARROW_RIGHT:
		case ARROW_DOWN:
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
	snprintf(buf,sizeof(buf),"\x1b[%d;%dH",E.cy+1,E.cx+1);
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
	
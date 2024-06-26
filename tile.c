/**INCLUDES**/
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>

/**DEFINES*/
#define ABUF_INIT {NULL,0};
#define CTRL_KEY(k) ((k) & 0x1f)
#define TILE_VERSION "0.0.1"
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE


/**DATA*/
enum editorKey{
	ARROW_LEFT = 1000,
	ARROW_UP,
	ARROW_RIGHT,
	ARROW_DOWN,
	HOME_KEY,
	DEL_KEY,
	END_KEY,
	PAGE_UP,
	PAGE_DOWN
};

typedef struct erow{
	char *chars;
	char *render;
	int rsize;
	int size;
}erow;

struct editorConfig{
	int cx,cy;
	struct termios orig_termios;
	int screenrows;
	int screencols;
	int rowoff;
	int coloff;
	erow *row;
	int numRows;
};

struct editorConfig E;

struct abuf{
	char *data;
	int len;
};


/**FUNCTIONS*/
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
	if(tcgetattr(STDIN_FILENO, &E.orig_termios)==-1)die("tcgetattr");
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

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numRows) ? NULL : &E.row[E.cy];
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      }
      else if(E.cy > 0){
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
        if(row && E.cx < row->size) E.cx++;
        else if(row && E.cx == row->size){
          E.cy++;
          E.cx = 0;
        }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy < E.numRows) {
        E.cy++;
      }
      break;
  }
  row = (E.cy >= E.numRows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row -> size: 0;
  if(E.cx > rowlen) E.cx = rowlen;
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

void editorScroll(){
	if(E.cy < E.rowoff) E.rowoff = E.cy;
	if(E.cy >= E.rowoff + E.screenrows) E.rowoff = E.cy - E.screenrows + 1;
	if(E.cx < E.coloff) E.coloff = E.cx;
	if(E.cx >= E.coloff + E.screencols) E.coloff = E.cx - E.screencols + 1;
}

void editorDrawRows(struct abuf *ab) {
  int y;
  int filerow;
  for (y = 0; y < E.screenrows; y++) {
	filerow = y + E.rowoff;
    if (filerow >= E.numRows) {
      if (E.numRows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
          "WELCOME TO TILE VERSION -- %s", TILE_VERSION);

        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].size - E.coloff;
      if (len < 0) len = 0;
	  if(len > E.screencols) len = E.screencols;
      abAppend(ab, &E.row[filerow].chars[E.coloff], len);
    }
    abAppend(ab, "\x1b[K", 3);
    if (y < E.screenrows - 1) {
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen(){
	editorScroll();
	struct abuf ab = ABUF_INIT;
	
	abAppend(&ab,"\x1b[?25l",6);
	//abAppend(&ab,"\x1b[H",3); //reposition the cursor at top-left corner
	
	editorDrawRows(&ab);
	
	char buf[50];
	snprintf(buf,sizeof(buf),"\x1b[%d;%dH",(E.cy - E.rowoff)+1,(E.cx - E.coloff)+1);
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

void editorUpdateRow(erow *row){
	free(row->render);
	row->render = malloc(row->size + 1);
	int j, i = 0;
	for(j = 0;j<row->size;j++){
		row->render[i++] = row->chars[j];
	}
	row->render[i] = '\0';
	row->size = i;
}

void editorAppendRow(char *s, size_t len){
	E.row = realloc(E.row, sizeof(erow)*(E.numRows + 1));
	int at = E.numRows;
	E.row[at].size = len;
	E.row[at].chars = malloc(len+1);
	memcpy(E.row[at].chars, s, len);
	E.row[at].chars[len] = '\0';

	E.row[at].rsize = 0;
	E.row[at].render = NULL;
	editorUpdateRow(&E.row[at]);
	E.numRows ++;
}

void editorOpen(char *filename){
	FILE *fp = fopen(filename, "r");
	if(!fp)die("fopen");

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while((linelen = getline(&line, &linecap, fp)) != -1){
		if(linelen != -1){
			while(linelen > 0 && (line[linelen-1]=='\n' || line[linelen -1] == '\r'))
				linelen --;
			editorAppendRow(line, linelen);
		}
	}

	free(line);
	fclose(fp);
}

void initEditor(){
	E.cx = 0;
	E.cy = 0;
	E.numRows = 0;
	E.row = NULL;
	E.rowoff = 0;
	E.coloff = 0;
	if(getWindowSize(&E.screenrows, &E.screencols)==-1) die("getWindowSize");
}




int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
	if(argc >= 2){
		editorOpen(argv[1]);
	}

    while (1) {
  		editorRefreshScreen();
  		editorProcessKeypress();
  	}
  	return 0;
}
	
/*
 * BSIDE-ADM20 helper for FlexBV
 *
 * V0.1 - October 4, 2018
 * 
 *
 * Written by Paul L Daniels (pldaniels@gmail.com)
 *
 */

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <X11/Xlib.h>

#define FL __FILE__,__LINE__

/*
 * Should be defined in the Makefile to pass to the compiler from
 * the github build revision
 *
 */
#ifndef BUILD_VER 
#define BUILD_VER 000
#endif

#ifndef BUILD_DATE
#define BUILD_DATE " "
#endif

#define SSIZE 1024

#define DATA_FRAME_SIZE 22
#define ee ""
#define uu "\u00B5"
#define kk "k"
#define MM "M"
#define mm "m"
#define nn "n"
#define pp "p"
#define dd "\u00B0"
#define oo "\u03A9"

struct serial_params_s {
	char *device;
	int fd, n;
	int cnt, size, s_cnt;
	struct termios oldtp, newtp;
};

struct meter_param {
	char mode[20];
	char units[20];
	int dividers[8];
	char prefix[8][2];
};

struct glb {
	uint8_t debug;
	uint8_t quiet;
	uint8_t show_mode;
	uint16_t flags;
	char *com_address;
	char *output_file;

	struct serial_params_s serial_params;

};

/*
 * A whole bunch of globals, because I need
 * them accessible in the Windows handler
 *
 * So many of these I'd like to try get away from being
 * a global.
 *
 */
struct glb *glbs;

bool fileExists(const char *filename) {
	struct stat buf;
	return (stat(filename, &buf) == 0);
}


char digit( unsigned char dg ) {

	int d;
	char g;

	switch ((dg) & 0x7F) {
		case 0x5F: g = '0'; d = 0; break;
		case 0x06: g = '1'; d = 1; break;
		case 0x6B: g = '2'; d = 2; break;
		case 0x2F: g = '3'; d = 3; break;
		case 0x36: g = '4'; d = 4; break;
		case 0x3D: g = '5'; d = 5; break;
		case 0x7D: g = '6'; d = 6; break;
		case 0x07: g = '7'; d = 7; break;
		case 0x7F: g = '8'; d = 8; break;
		case 0x3F: g = '9'; d = 9; break;
		case 0x79: g = 'E'; d = 0; break;
		case 0x58: g = 'L'; d = 0; break;
		default: g = ' ';
					//		default: fprintf(stderr,"Unknown 7-segment data '%d / %02x'\r\n", dg, dg);
	}
	//	fprintf(stderr,"%d / %c", d, dg);
	return g;
}

/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220248
  Function Name	: init
  Returns Type	: int
  ----Parameter List
  1. struct glb *g ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int init(struct glb *g) {
	g->debug = 0;
	g->quiet = 0;
	g->flags = 0;
	g->com_address = NULL;
	g->output_file = NULL;

	return 0;
}

void show_help(void) {
	fprintf(stdout,"BSIDE ADM20 Multimeter decoder helper for FlexBV\r\n"
			"By Paul L Daniels / pldaniels@gmail.com\r\n"
			"Build %d / %s\r\n"
			"\r\n"
			" [-p <comport#>] [-s <serial port config>] [-d] [-q]\r\n"
			"\r\n"
			"\t-h: This help\r\n"
			"\t-p <comport>: Set the com port for the meter, eg: -p /dev/ttyUSB0\r\n"
			"\t-s <[9600|4800|2400|1200]:[7|8][o|e|n][1|2]>, eg: -s 2400:8n1\r\n"
			"\t-o <output file> ( used by FlexBV to read the data )\r\n"
			"\t-d: debug enabled\r\n"
			"\t-q: quiet output\r\n"
			"\t-v: show version\r\n"
			"\r\n"
			"\r\n"
			"\texample: bside-adm20 -p /dev/ttyUSB0\r\n"
			, BUILD_VER
			, BUILD_DATE 
			);
} 


/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220258
  Function Name	: parse_parameters
  Returns Type	: int
  ----Parameter List
  1. struct glb *g,
  2.  int argc,
  3.  char **argv ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int parse_parameters(struct glb *g, int argc, char **argv ) {
	int i;

	for (i = 0; i < argc; i++) {
		if (argv[i][0] == '-') {
			/* parameter */
			switch (argv[i][1]) {
				case 'p':
					/*
					 * com port can be multiple things in linux
					 * such as /dev/ttySx or /dev/ttyUSBxx
					 */
					i++;
					if (i < argc) {
						g->serial_params.device = argv[i];
					} else {
						fprintf(stdout,"Insufficient parameters; -p <com port>\n");
						exit(1);
					}
					break;

				case 'o':
					/* 
					 * output file where this program will put the text
					 * line containing the information FlexBV will want 
					 *
					 */
					i++;
					if (i < argc) {
						g->output_file = argv[i];
					} else {
						fprintf(stdout,"Insufficient parameters; -o <output file>\n");
						exit(1);
					}
					break;

				case 'd': g->debug = 1; break;

				case 'q': g->quiet = 1; break;

				case 'v':
							 fprintf(stdout,"Build %d\r\n", BUILD_VER);
							 exit(0);
							 break;

				case 's':
							 break;

				default: break;
			} // switch
		}
	}

	return 0;
}



/*
 * Default parameters are 2400:8n1, given that the multimeter
 * is shipped like this and cannot be changed then we shouldn't
 * have to worry about needing to make changes, but we'll probably
 * add that for future changes.
 *
 */
void open_port(struct serial_params_s *s) {
	int r; 

	s->fd = open( s->device, O_RDWR | O_NOCTTY |O_NDELAY );
	if (s->fd <0) {
		perror( s->device );
	}

	fcntl(s->fd,F_SETFL,0);
	tcgetattr(s->fd,&(s->oldtp)); // save current serial port settings 
	tcgetattr(s->fd,&(s->newtp)); // save current serial port settings in to what will be our new settings
	cfmakeraw(&(s->newtp));
	s->newtp.c_cflag = B2400 | CS8 |  CREAD | CRTSCTS ; // Adjust the settings to suit our BSIDE-ADM20 / 2400-8n1

	r = tcsetattr(s->fd, TCSANOW, &(s->newtp));
	if (r) {
		fprintf(stderr,"%s:%d: Error setting terminal (%s)\n", FL, strerror(errno));
		exit(1);
	}
}

uint8_t a2h( uint8_t a ) {
	a -= 0x30;
	if (a < 10) return a;
	a -= 7;
	return a;
}
/*-----------------------------------------------------------------\
  Date Code:	: 20180127-220307
  Function Name	: main
  Returns Type	: int
  ----Parameter List
  1. int argc,
  2.  char **argv ,
  ------------------
  Exit Codes	:
  Side Effects	:
  --------------------------------------------------------------------
Comments:

--------------------------------------------------------------------
Changes:

\------------------------------------------------------------------*/
int main ( int argc, char **argv ) {

	GC gc;
	XGCValues values;
	unsigned long valuemask = GCCapStyle|GCJoinStyle;
	XFontStruct *font_info;
	char *font_name = "-*-terminus-*-r-*-*-32-*";
	Window win;
	Display *display;
	int x11_fd;
	fd_set in_fds;
	struct timeval tv;
	XEvent xev;


	char linetmp[SSIZE]; // temporary string for building main line of text
	char prefix[SSIZE]; // Units prefix u, m, k, M etc
	char units[SSIZE];  // Measurement units F, V, A, R
	char mmmode[SSIZE]; // Multimeter mode, Resistance/diode/cap etc

	//uint8_t dfake[] = { 0xf0, 0x11, 0x02, 0x00, 0x44, 0x33, 0x44, 0x36, 0x00, 0x05 }; // 2.7965V [ DC Volts ]
	//uint8_t dfake[] = { 0xf0, 0x11, 0x04, 0x02, 0x44, 0x33, 0x44, 0x36, 0x00, 0x05 }; // 27.965kOhms [ Resistance ]
	//uint8_t dfake[] = { 0xf0, 0x11, 0x04, 0x02, 0x44, 0x33, 0x44, 0x36, 0x10, 0x05 }; // -27.965kOhms [ Resistance ]

	uint8_t d[SSIZE];
	uint8_t dt[SSIZE];      // Serial data packet
	int dt_loaded = 0;	// set when we have our first valid data
	uint8_t dps = 0;     // Number of decimal places
	struct glb g;        // Global structure for passing variables around
	int i = 0;           // Generic counter
	char temp_char;        // Temporary character
	char tfn[4096];

	/* this variable will be used to store the "default" screen of the  */
	/* X server. usually an X server has only one screen, so we're only */
	/* interested in that screen.                                       */
	int screen_num;

	/* these variables will store the size of the screen, in pixels.    */
	int screen_width;
	int screen_height;

	/* this variable will be used to store the ID of the root window of our */
	/* screen. Each screen always has a root window that covers the whole   */
	/* screen, and always exists.                                           */
	Window root_window;

	/* these variables will be used to store the IDs of the black and white */
	/* colors of the given screen. More on this will be explained later.    */
	unsigned long white_pixel;
	unsigned long black_pixel;


	/* open the connection to the display "simey:0". */
	display = XOpenDisplay(NULL);
	if (display == NULL) {
		fprintf(stderr, "Cannot connect to X server %s\n", "simey:0");
		exit (-1);
	}
	/* check the number of the default screen for our X server. */
	screen_num = DefaultScreen(display);

	/* find the width of the default screen of our X server, in pixels. */
	screen_width = DisplayWidth(display, screen_num);

	/* find the height of the default screen of our X server, in pixels. */
	screen_height = DisplayHeight(display, screen_num);

	/* find the ID of the root window of the screen. */
	root_window = RootWindow(display, screen_num);

	/* find the value of a white pixel on this screen. */
	white_pixel = WhitePixel(display, screen_num);

	/* find the value of a black pixel on this screen. */
	black_pixel = BlackPixel(display, screen_num);

	glbs = &g;

	/*
	 * Initialise the global structure
	 */
	init(&g);

	/*
	 * Parse our command line parameters
	 */
	parse_parameters(&g, argc, argv);

	if (g.output_file) snprintf(tfn,sizeof(tfn),"%s.tmp",g.output_file);

	/*
	 * Handle the COM Port
	 */
	open_port(&g.serial_params);

	/*
	 * Set up X11
	 *
	 */
	win = XCreateSimpleWindow(display,
			RootWindow(display, screen_num),
			50, 50,
			300, 100,
			1, BlackPixel(display, screen_num),
			WhitePixel(display, screen_num));

	// You don't need all of these. Make the mask as you normally would.
	XSelectInput(display, win, 
			//ExposureMask | KeyPressMask | KeyReleaseMask | PointerMotionMask |
			//ButtonPressMask | ButtonReleaseMask  | StructureNotifyMask 
			ExposureMask | PointerMotionMask |
			ButtonPressMask | ButtonReleaseMask  | StructureNotifyMask 
			);

	font_info = XLoadQueryFont(display, font_name);
	if (!font_info) {
		fprintf(stderr,"%s:%d: Unable to load font '%s'\r\n", FL, font_name);
		exit(1);
	}

	values.cap_style = CapButt;
	values.join_style = JoinBevel;
	gc = XCreateGC(display, win, valuemask, &values);
	if (gc < 0) {
		fprintf(stderr, "XCreateGC: \n");
		exit(1);
	}

	XSetFont(display, gc, font_info->fid);
	XMapWindow(display, win);
	XFlush(display);
	x11_fd = ConnectionNumber(display);

	/*
	 *
	 * Parent will terminate us... else we'll become a zombie
	 * and hope that the almighty PID 1 will reap us
	 *
	 */
	while (1) {
		char line1[1024];
		char *p, *q;
		double v = 0.0;
		int end_of_frame_received = 0;
		int num_ready_fds;
		ssize_t bytes_read = 0;

		FD_ZERO(&in_fds);
		FD_SET(x11_fd, &in_fds);
		tv.tv_usec = 250000;
		tv.tv_sec = 0;
		/*
		num_ready_fds = select(x11_fd +1, &in_fds, NULL, NULL, &tv);
		if (num_ready_fds > 0) {
			fprintf(stderr,"%s:%d: Events received\r\n", FL );
		} else if (num_ready_fds == 0) {
			fprintf(stderr,"%s:%d: Timer fired off\r\n", FL);
		} else {
			fprintf(stderr,"%s:%d: Error occurred reading file descriptor count \r\n", FL);
		}
		*/

		linetmp[0] = '\0';

		while (XPending(display)) {
			XNextEvent(display, &xev);
			if(xev.type==Expose) {
				XFillRectangle(display, win, DefaultGC(display, screen_num), 0, 0, 300, 100);
			}
			/* exit on key press */
			if(xev.type==KeyPress)
				break;

			// Handle Windows Close Event
			if(xev.type==ClientMessage)
				break;
		} // while pending X events.

		/*
		 * Time to start receiving the serial block data
		 *
		 * We initially "stage" here waiting for there to
		 * be something happening on the com port.  Soon as
		 * something happens, then we move forward to trying
		 * to read the data.
		 *
		 */

		if (g.debug) { fprintf(stdout,"DATA START: "); }
		end_of_frame_received = 0;
		i = 0;
		do {
			char temp_char;
			bytes_read = read(g.serial_params.fd, &temp_char, 1);
			if (bytes_read) {
				d[i] = temp_char;
				if (g.debug) { fprintf(stdout,"%02x ", d[i]); }

				i++;

				if (temp_char == 0x55) {
					end_of_frame_received = 1;
					break;
				}
			}
		} while ((bytes_read > 0) && (i < sizeof(d)) && (!end_of_frame_received));

		if (g.debug) { fprintf(stdout,":END [%d bytes]\r\n", i); }

		/*
		 * Validate the received data
		 *
		 */
		if (i != DATA_FRAME_SIZE) {
			if (g.debug) { fprintf(stdout,"Invalid number of bytes, expected %d, received %d, loading previous frame\r\n", DATA_FRAME_SIZE, i); }
			if (dt_loaded) memcpy(d, dt, sizeof(d));
		} else {
			memcpy(dt, d, sizeof(d)); // make a copy.
			dt_loaded = 1;
		}

		/*
		 * Initialise the strings used for units, prefix and mode
		 * so we don't end up with uncleared prefixes etc
		 * ( see https://www.youtube.com/watch?v=5HUyEykicEQ )
		 *
		 * Prefix string initialised to single space, prevents 
		 * annoying string width jump (on monospace, can't stop
		 * it with variable width strings unless we draw the 
		 * prefix+units separately in a fixed location
		 *
		 */
		snprintf(prefix, sizeof(prefix), " ");
		units[0] = '\0';
		mmmode[0] = '\0';

		/*
		 * Decode our data.
		 *
		 * While the data sheet gives a very nice matrix for the RANGE and FUNCTION values
		 * it's probably more human-readable to break it down in to longer code on a per
		 * function selection.
		 *
		 * linetmp : contains the value we want show
		 * mmode   : contains the meter mode (resistance, volts, amps etc)
		 *  L"\u00B0C" = 'C
		 *  L"\u00B0F" = 'F
		 *  L"\u2126"  = ohms char
		 *  L"\u00B5"  = mu char (micro)
		 *
		 */


		if (d[16] & 0x80) snprintf(mmmode,sizeof(mmmode),"REL");
		if (d[16] & 0x20) snprintf(mmmode,sizeof(mmmode),"AUTO");

		if (d[17] & 0x40) snprintf(mmmode,sizeof(mmmode),"hFE");
		if (d[17] & 0x20) snprintf(mmmode,sizeof(mmmode),"%%");
		if (d[17] & 0x08) snprintf(mmmode,sizeof(mmmode),"MIN");
		if (d[17] & 0x20) snprintf(mmmode,sizeof(mmmode),"min-max");
		if (d[17] & 0x20) snprintf(mmmode,sizeof(mmmode),"MAX");
		if (d[17] & 0x20) snprintf(mmmode,sizeof(mmmode),"USB");

		if (d[18] & 0x80) snprintf(units,sizeof(units),"F");
		if (d[18] & 0x40) snprintf(prefix,sizeof(prefix),"n");
		if (d[18] & 0x20) snprintf(prefix,sizeof(prefix),"%s",uu);
		if (d[18] & 0x02) snprintf(units,sizeof(units),"%sF",dd);
		if (d[18] & 0x01) snprintf(units,sizeof(units),"%sC",dd);

		if (d[19] & 0x80) snprintf(units,sizeof(units),"Hz");
		if (d[19] & 0x40) snprintf(units,sizeof(units),"%s",oo);
		if (d[19] & 0x20) snprintf(prefix,sizeof(prefix),"k");
		if (d[19] & 0x10) snprintf(prefix,sizeof(prefix),"M");
		if (d[19] & 0x08) snprintf(units,sizeof(units),"V");
		if (d[19] & 0x04) snprintf(units,sizeof(units),"A");
		if (d[19] & 0x02) snprintf(prefix,sizeof(prefix),"m");
		if (d[19] & 0x01) snprintf(prefix,sizeof(prefix),"%s",uu);

		{
			char value[128];
			int dp = 0;
			/*
				value = digit(&d[7]) *1000;
				value += digit(&d[6]) *100;
				value += digit(&d[5]) *10;
				value += digit(&d[4]) *1;
				*/
			/*
				switch (dp) {
				case 1: value /= 10; break;
				case 2: value /= 100; break;
				case 3: value /= 1000; break;
				case 4: value /= 10000; break;
				}
				*/
			snprintf(linetmp,sizeof(linetmp), "%s%c%s%c%s%c%s%c%s%s"
					, d[8]&0x08?"-":" "
					, digit(d[7])
					, d[6]&0x80?".":""
					, digit(d[6])
					, d[5]&0x80?".":""
					, digit(d[5])
					, d[4]&0x80?".":""
					, digit(d[4])
					, prefix
					, units
					);
		}

		/*
		 *
		 * END OF DECODING
		 */


		snprintf(line1, sizeof(line1), "%-40s", linetmp);
		//		snprintf(line2, sizeof(line2), "%-40s", mmmode);
		//		snprintf(line3, sizeof(line3), "V.%03d", BUILD_VER);

		if (!g.quiet) fprintf(stdout,"%s\r",line1); fflush(stdout);

		XSetForeground(display, gc, black_pixel);
		XFillRectangle(display, win, DefaultGC(display, screen_num), 0, 0, 300, 100);
		XSetForeground(display, gc, white_pixel);
		XDrawString(display, win, gc, 10, 40, line1, strlen (line1));

		if (g.output_file) {
			/*
			 * Only write the file out if it doesn't
			 * exist. 
			 *
			 */
			if (!fileExists(g.output_file)) {
				FILE *f;
				fprintf(stderr,"%s:%d: output filename = %s\r\n", FL, g.output_file);
				f = fopen(tfn,"w");
				if (f) {
					fprintf(f,"%s", linetmp);
					fprintf(stderr,"%s:%d: %s => %s\r\n", FL, linetmp, tfn);
					fclose(f);
					rename(tfn, g.output_file);
				}
			}
		}

	} // while(1)

	if (g.serial_params.fd) close(g.serial_params.fd);

	return 0;

}

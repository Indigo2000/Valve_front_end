/*  Valve User Interface Program   */

#include "gfxlib1.h"
#include <io.h>
#include <dos.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include "async.h"
#include "error.h"

#define  SOLID    1
#define  WHITE    15
#define  BLACK    0
#define  PURPLE   5
#define  RED      4
#define  GREY     7
#define  BLUE     1
#define 	TRUE  	1
#define 	FALSE 	0

short qwerty_input(void);

char text[255], * eos;                 /* Text input string */

char enteraccel[32]="Enter Accelaration:\0";
char enterspeed[32]="Enter Slew Speed:\0";
char enterdead[32]="Enter Deadband Range:\0";
char enterup[40]="Enter New Upper Limit (low limit-100):\0";
char enterlo[40]="Enter New Lower Limit (0-up limit):\0";
char enterpass[40]="Enter PIN:\0";
char enternewpass[40]="Enter new PIN:\0";
char enteroldpass[40]="Enter old PIN:\0";
char enterbspeed[40]="Enter base speed:\0";		/*questions*/

int quit=FALSE,bspeed,speed,dead,up,lo,up_disp=275,lo_disp=0,temppass,pass=1,olddisp=100;

long int actpos, compos, accel;

long int c;

async s(9600, even, 7, 1);		/*initialise comms*/

const unsigned int ier = 1;
const unsigned int iir = 2;
const unsigned int lcr = 3;
const unsigned int mcr = 4;
const unsigned int lsr = 5;
const unsigned int msr = 6;
const unsigned int scr = 7;

const unsigned char dr = 0x01;
const unsigned char thre = 0x20;

const unsigned char rx_id = 0x04;
const unsigned char rx_mask = 0x07;
const unsigned char rx_int = 0x01;
const unsigned char re_mask = 0x0e;
const unsigned char eoi = 0x20;
const unsigned char mc_int = 0x08;
const unsigned int imr = 0x21;
const unsigned int icr = 0x20;
const unsigned int vector_base = 0x08;

const unsigned char stopbit1 = 0x00;
const unsigned char stopbit2 = 0x04;
const unsigned char databit5 = 0x00;
const unsigned char databit6 = 0x01;
const unsigned char databit7 = 0x02;
const unsigned char databit8 = 0x03;
const unsigned char no_parity = 0x00;
const unsigned char odd_parity = 0x08;
const unsigned char even_parity = 0x18;
const unsigned char mark_parity = 0x28;
const unsigned char space_parity = 0x38;

const unsigned int port_addr[] = {0x03f8, 0x02f8, 0x03e8, 0x02e8};
const unsigned int port_intr[] = {4, 3, 4, 3};

const unsigned int buffer_size = 256;

void interrupt(*vector_save) (...);
char buffer[buffer_size];
unsigned int start_buf;
unsigned int end_buf;
unsigned int base = 0;
unsigned int inte;


void interrupt irq(...)
{
	 unsigned char q;
	 disable();
	 if ((inportb(base + iir) & rx_mask) == rx_id) {
		  if (((end_buf + 1) % buffer_size) == start_buf)
				error("buffer overflow in irq");
		  q = inportb(base + lsr);
		  buffer[end_buf] = inportb(base);
		  if ((q & re_mask) == 0)
				end_buf = (end_buf + 1) % buffer_size;
	 }
	 outportb(icr, eoi);
	 enable();
}

int com_exist()
{
	 const unsigned char test_byte1 = 0x0F;
	 const unsigned char test_byte2 = 0xF1;
	 unsigned char m, l, b1, b2;
	 m = inportb(base + mcr);
	 l = inportb(base + lcr);
	 outportb(base + mcr, 0x10);
	 outportb(base + lcr, 0x80);
	 b1 = inportb(base);
	 b2 = inportb(base + 1);
	 outportb(base, 0x04);
	 outportb(base + 1, 0x00);
	 outportb(base + lcr, 0x03);
	 outportb(base, test_byte1);
	 delay(20);
	 if (inportb(base) != test_byte1)
		  return 0;
	 outportb(base, test_byte2);
	 delay(20);
	 if (inportb(base) != test_byte2)
		  return 0;
	 outportb(base + lcr, 0x80);
	 outportb(base, b1);
	 outportb(base + 1, b2);
	 outportb(base + lcr, l);
	 outportb(base + mcr, m);
	 return 1;
}

void set_baud(unsigned long int baud)
{
	 unsigned long int y;
	 outportb(base + lcr, inportb(base + lcr) | 0x80);
	 y = 115200L / baud;
	 outportb(base, (unsigned char) (y & 0xff));
	 outportb(base + 1, (unsigned char) ((y >> 8) & 0xff));
	 outportb(base + lcr, inportb(base + lcr) & 0x7f);
}

void set_parity(data_parity parity, unsigned int data_bits,
					  unsigned int stop_bits)
{
	 unsigned char y;
	 switch (parity) {
	 case none:
		  y = no_parity;
		  break;
	 case even:
		  y = even_parity;
		  break;
	 case odd:
		  y = odd_parity;
		  break;
	 case mark:
		  y = mark_parity;
		  break;
	 case space:
		  y = space_parity;
		  break;
	 default:
		  error("invalid parity in set_parity");
	 };
	 switch (data_bits) {
	 case 5:
		  y |= databit5;
		  break;
	 case 6:
		  y |= databit6;
		  break;
	 case 7:
		  y |= databit7;
		  break;
	 case 8:
		  y |= databit8;
		  break;
	 default:
		  error("invalid data bits in set_parity");
	 };
	 switch (stop_bits) {
	 case 1:
		  y |= stopbit1;
		  break;
	 case 2:
		  y |= stopbit2;
		  break;
	 default:
		  error("invalid stop bits in set_parity");
	 };
	 outportb(base + lcr, y);
}

void i_enable()
{
	 unsigned char c;
	 disable();
	 vector_save = getvect(vector_base + inte);
	 setvect(vector_base + inte, irq);
	 c = inportb(base + mcr) | mc_int;
	 outportb(base + mcr, c);
	 outportb(base + ier, rx_int);
	 c = inportb(imr) & ~(0x01 << inte);
	 outportb(imr, c);
	 enable();
}

void i_disable()
{
	 unsigned char c;
	 disable();
	 c = inportb(imr) | (0x01 << inte);
	 outportb(imr, c);
	 outportb(base + ier, 0x00);
	 c = inportb(base + mcr) & ~mc_int;
	 outportb(base + mcr, c);
	 setvect(vector_base + inte, vector_save);
	 enable();
}

void open_comms(port com_port, unsigned int baud, data_parity parity,
					  unsigned int data_bits, unsigned int stop_bits)
{
	 if ((com_port < com1) || (com_port > com4))
		  error("com port out of range in open_comms");
	 else {
		  base = port_addr[com_port];
		  inte = port_intr[com_port];
		  if (com_exist()) {
				start_buf = 0;
				end_buf = 0;
				set_baud(baud);
				set_parity(parity, data_bits, stop_bits);
				i_enable();
		  } else
				error("no com port in async::async");
	 }
}

async::async(port com_port, unsigned int baud, data_parity parity,
				 unsigned int data_bits, unsigned int stop_bits)
{
	 if (base)
		  error("port already open in async::async");
	 open_comms(com_port, baud, parity, data_bits, stop_bits);
}

async::async(unsigned int baud, data_parity parity,
				 unsigned int data_bits, unsigned int stop_bits)
{
	 if (base)
		  error("port already open in async::async");
	 port c = com1;
	 while (c <= com4) {
		  base = port_addr[c];
		  if (com_exist()) {
				open_comms(c, baud, parity, data_bits, stop_bits);
				return;
		  }
		  c = (port) (c + 1);
	 }
	 error("no com port in async::async");
}

async::~async()
{
	 i_disable();
	 base = 0;
}

char async::get_char()
{
	 char c;
	 if (end_buf == start_buf)
		  return -1;
	 c = buffer[start_buf];
	 start_buf = (start_buf + 1) % buffer_size;
	 return c;
}

void async::put_char(char c)
{
	 while (!(inportb(base + lsr) & thre));
	 outportb(base, c);
}


const unsigned char esc = 0x1B; // ASCII Escape character


short qwerty_input(void) /* Returns true if there is anything in keyboard buffer */
{
	return((short)kbhit());
}


void test_open()
{
			  /*Not Required*/
}

void test_error()
{
			/*Not Required*/

}

void fillbox(int lx,int ly,int rx,int ry,int colour, int bordercolour)
{/*Fills a box with a border*/
int polycood[8];                 /* Four pairs of x,y coodinates */

	setcolor(bordercolour);                 /*Draw border*/
	moveto(lx,ly);
	lineto(lx,ry);
	lineto(rx,ry);
	lineto(rx,ly);
	lineto(lx,ly);                   /* Rectangle (or square) */
	setfillstyle(SOLID,colour);      /* Solid fill */
	polycood[0]=lx;  polycood[1]=ly;
	polycood[2]=lx;  polycood[3]=ry;
	polycood[4]=rx;  polycood[5]=ry;
	polycood[6]=rx;  polycood[7]=ly; /* Array of cood making up a rectangle */
	fillpoly(4,polycood);            /* Fill inside the rectangle */
}

int get_act_pos()
{/*gets actual position*/
int ptr,ch=0,i;

	s.put_char(0x0d);
	delay(10);

	do{
		ch = s.get_char();                    /*test to see if comms are working*/
		if (ch!=0x0d){
			s.put_char(0x0d);
			delay(10);
			ch = s.get_char();}

		if (ch!=0x0d){								/*if they're not working, warn the user*/
			fillbox(11,365+2*14,313,469,BLACK,BLACK);
			setcolor(WHITE);
			outtextxy(12,365+5*14,"Checking...");
			return 1;
						}
		else fillbox(11,365+2*14,313,469,BLACK,BLACK);
										/*if comms are working, black out the screen with the warning bit on it*/
	 }
	 while (ch!=0x0d);

	 s.put_char('1');
	 s.put_char('o');
	 s.put_char('a');
	 s.put_char(0x0d);
	 s.put_char(0x0a);		/*send output actual command*/

	 delay(10);
			do{	  do{

					ch = (int)s.get_char();
					delay(10);

					}while (ch==-1);


							 }
							while (ch!=58);

		ptr=0;
		do
			{
							 do{
							 ch = (int)s.get_char();

								}while (ch==-1);




						 if ((ptr<17) &&  ((ch>='0')&&(ch<='9')) )
						 {
						 text[ptr]=ch; /*build a string of the response*/
						 text[ptr+1]=0;
						 ptr=ptr+1;}

								  }
			while (ch!=0x0d);

			return 0;
		}

int get_status ()
{/*get status*/
	 int ptr,ch=0,i;



	 s.put_char(0x0d);
			 delay(10);

do{
		 ch = s.get_char();
		 if (ch!=0x0d){s.put_char(0x0d);
		 delay(10);
		 ch = s.get_char();}

						if (ch!=0x0d){

								fillbox(11,365+2*14,313,469,BLACK,BLACK);
									setcolor(WHITE);
									outtextxy(12,365+5*14,"No Comms Connection!");
																		return 1;
																		}
									else fillbox(11,365+2*14,313,469,BLACK,BLACK);
		 }
		 while (ch!=0x0d);


	 s.put_char('1');
	 s.put_char('o');
	 s.put_char('s');                  /*status query*/
	 s.put_char(0x0d);
		  s.put_char(0x0a);

		  delay(10);
		 do{ do{	ch = (int)s.get_char();
		  delay(10);

		  }while (ch==-1);
	}
							while (ch!=58);

			ptr=0;
		do
			{
					 do{
							 ch = (int)s.get_char();

								}while (ch==-1);




						 if ((ptr<17) &&  ((ch>='0')&&(ch<='9')) )
						 {
						 text[ptr]=ch; /*build a string of the response*/
						 text[ptr+1]=0;
						 ptr=ptr+1;}

							  }
			while (ch!=0x0d);

			return 0;
}

int get_com_pos ()
{/*get command position*/
	 int ptr,ch=0,i;



	 s.put_char(0x0d);
			 delay(10);

do{
		 ch = s.get_char();
		 if (ch!=0x0d){s.put_char(0x0d);
		 delay(10);
		 ch = s.get_char();}

						if (ch!=0x0d){

								fillbox(11,365+2*14,313,469,BLACK,BLACK);
									setcolor(WHITE);
									outtextxy(12,365+5*14,"No Comms Connection!");
																		return 1;
																		}
										else fillbox(11,365+2*14,313,469,BLACK,BLACK);
		 }
		 while (ch!=0x0d);

	 s.put_char('1');
	 s.put_char('o');
	 s.put_char('c');     /*command query*/
	 s.put_char(0x0d);
	 s.put_char(0x0a);

		  delay(10);
		  do{ do{	ch = (int)s.get_char();
				delay(10);

				}while (ch==-1);


		  }
							while (ch!=58);

			ptr=0;
		do
			{
					 do{
							 ch = (int)s.get_char();

								}while (ch==-1);




						 if ((ptr<17) &&  ((ch>='0')&&(ch<='9')) )
						 {
						 text[ptr]=ch; /*build a string of the response*/
						 text[ptr+1]=0;
						 ptr=ptr+1;}

							  }
			while (ch!=0x0d);

			return 0;
}


void showact(int actpos )
{/* put actual position in percent on the screen*/
	itoa(actpos,text,10);
	fillbox(590,345+14,628,366,BLACK,BLACK);      /*Cover old position text*/
	setcolor(WHITE);
	outtextxy(590,345+14,text);                 /*show new position text*/
}

void showcom(int compos )
{/* put command position in percent on the screen*/
	fillbox(590,345,628,352,BLACK,BLACK);      /*Cover old position text*/
	setcolor(WHITE);
	itoa(compos,text,10);
	outtextxy(590,345,text);                 /*show new position text*/
}

void gettext(char description[32],long int low, long int high)
{
	/* Gets a string of numbers in graphics mode at x,y*/

	int ptr,key, g;
		do
			{
			fillbox(11,365+2*14,313,469,BLACK,BLACK);
			setcolor(WHITE);
			ptr=0;
			outtextxy(12,370+3*14,description);    /* Pop the question*/
				do
					{
						key=(int)getch();

						if (key==0) {
											key=(int)getch();
											key=0;               /* Ignore special keys */
										}
						if ((ptr<17) && ( (key=='.') || ((key>='0')&&(key<='9')) ))
										{
											text[ptr]=key; /*Enter sixteen digit number*/
											text[ptr+1]=0;
											setcolor(WHITE);
											outtextxy(12,365+5*14,text);
											ptr=ptr+1;
										}
						else
							{
							if (key==8) {
											setcolor(BLACK); /* Handle backspace */
											outtextxy(12,365+5*14,text);
											text[ptr-1]=0;
											ptr=ptr-1;
											if (ptr<0) ptr=0;
											setcolor(WHITE);
											outtextxy(12,365+5*14,text);
				}

										}
			  }
			while (key!=13);
	fillbox(11,365+2*14,313,469,BLACK,BLACK);
	setcolor(WHITE);
	g=strtod(text, &eos);
	if ((g>high) || (g<low)) /*if user has entered an incorrect value-tell them*/
		{
			outtextxy(11,365+3*14,"Incorrect value entered");
			delay(1500);
			}
		}
	while ((g>high)||(g<low));
}

void getpass(char description[32])
{
	/* Gets a string of characters in graphics mode at x,y*/

	int ptr,key, g;
			fillbox(11,365+2*14,313,469,BLACK,BLACK);
			setcolor(WHITE);
			ptr=0;
			outtextxy(12,370+3*14,description);    /* Pop the question*/
				do
					{
						key=(int)getch();

						if (key==0) {
											key=(int)getch();
											key=0;               /* Ignore special keys */
										}
						if ((ptr<17) && ( (key=='.') || ((key>='0')&&(key<='9')) ))
										{
											text[ptr]=key; /*Enter the PIN*/
											text[ptr+1]=0;
											setcolor(WHITE);
											outtextxy(12+10*ptr,365+5*14,"*");/*show a '*'*/
											ptr=ptr+1;
										}
						else
							{
							if (key==8) {
											/* Handle backspace */
											fillbox(11,365+5*14,313,469,BLACK,BLACK);
											ptr=0;
											setcolor(WHITE);

											}
										}
			  }
			while (key!=13);
	fillbox(11,365+2*14,313,469,BLACK,BLACK);
	setcolor(WHITE);
}

void scale (int top, int bottom)
{/*show picture of scale for where valve is*/
  fillbox(325,246,628,259,BLACK,BLACK);     /*cover up old scale*/
  setcolor(WHITE);
  outtextxy(324+bottom+0*(top-bottom),247,"0%");
  if (top-bottom>42)              /*don't show "50%" if there isn't space!*/
  outtextxy(324+bottom+0.5*(top-bottom),247,"50%");
  outtextxy(324+bottom+(top-bottom),247,"100%");
  outtextxy(324+bottom+0.1*(top-bottom),247,".");
  outtextxy(324+bottom+0.2*(top-bottom),247,".");
  outtextxy(324+bottom+0.3*(top-bottom),247,".");
  outtextxy(324+bottom+0.4*(top-bottom),247,".");
  outtextxy(324+bottom+0.5*(top-bottom),247,".");
  outtextxy(324+bottom+0.6*(top-bottom),247,".");
  outtextxy(324+bottom+0.7*(top-bottom),247,".");
  outtextxy(324+bottom+0.8*(top-bottom),247,".");
  outtextxy(324+bottom+0.9*(top-bottom),247,".");  /*Show Scale*/
}

void disp(int a, int b, int c, int d, int e, int f, int g, int h)
{/*seven segment displays*/
	if (g==1)fillbox (437,110,462,115,RED,RED);        /* g */
	if (a==1)fillbox (437,80,462,85,RED,RED);          /* a  */
	if (d==1)fillbox (437,140,462,145,RED,RED);        /* d */
	if (e==1)fillbox (437,119,442,136,RED,RED);        /* e */
	if (f==1)fillbox (437,89,442,106,RED,RED);         /* f */
	if (c==1)fillbox (457,119,462,136,RED,RED);        /* c */
	if (b==1)fillbox (457,89,462,106,RED,RED);         /* b */
}

void value (int x, int lo_disp, int up_disp)
{/*shows picture of where valve is*/
	fillbox(324,259,629,319,BLACK,WHITE);
	fillbox (324+x+lo_disp,318,330+x+lo_disp,260,GREY,GREY);
	fillbox (324+x+lo_disp,285,628,290,GREY,GREY);
	fillbox (324+up_disp,318,628,260,GREY,GREY);
	fillbox (324,318,324+lo_disp,260,GREY,GREY);
	fillbox(324,318,324,260,WHITE,WHITE);/*Show valve position picture*/
}

void ssdisp (int cnt, int tmp, int bsy, int rdy, int cl, int op, int lo, int hi, int fl, int sw)
{/*Show seven segment displays*/
	if (cnt==1)
	{
	fillbox(475,150,425,75,BLACK,WHITE);
	disp(0,0,0,0,1,1,0,0);
						/*Current*/
	}
	if (tmp==1)
	{
	fillbox(475,150,425,75,BLACK,WHITE);
	disp(0,0,0,1,1,1,1,0);
					  /*temperature*/
	}
	if (bsy==1)
	{
	fillbox(475,150,425,75,BLACK,WHITE);
	disp(0,0,1,1,1,1,1,0);
									  /*moving*/
	}
	if (rdy==1)
	{
	fillbox(475,150,425,75,BLACK,WHITE);
	disp(0,0,0,0,1,0,1,0);
													/*Ready*/
	}
	if (cl==1)
	{
	fillbox(475,150,425,75,BLACK,WHITE);
	disp(1,0,0,1,1,1,0,0);
												/* Closed*/
	}
	if (op==1)
	{
	fillbox(475,150,425,75,BLACK,WHITE);
	disp(1,1,1,1,1,1,0,0);
										/*open*/
	}
	if (lo==1)
	{
	fillbox(475,150,425,75,BLACK,WHITE);
	disp(0,0,0,1,1,1,0,0);
								/*low*/
	}
	if (hi==1)
	{
	fillbox(475,150,425,75,BLACK,WHITE);
	disp(0,1,1,0,1,1,1,0);
									  /*High*/
	}
	if (fl==1)
	{
	fillbox(475,150,425,75,BLACK,WHITE);
	disp(1,0,0,0,1,1,1,0);

	}                                       /*fail*/
	if (sw==1)
	{
	fillbox(475,150,425,75,BLACK,WHITE);
	disp(1,0,1,1,0,1,1,0);
											/*switch*/
	}
}


void procinit(void)
/*draws the initial screen*/
{
  fillbox(0,0,639,479,PURPLE,WHITE);      /* Main border */
  fillbox(10,245,314,470,BLACK,WHITE);   /*Top Box*/
  fillbox(10,10,629,235,BLACK,WHITE);   /* Command box */
  fillbox(324,245,629,319,BLACK,WHITE); /* Valve box */
  fillbox(324,329,629,470,BLACK,WHITE); /* Current value box */
  fillbox(125,25,510,215,GREY,BLACK);   /*valve box*/
  fillbox(150,50,485,190,GREY,BLUE);    /*blue bit on it*/

  setcolor(BLACK);
  outtextxy(48,2,"Mclennan Servo Supplies Ltd. valve User Interface Program Version 1.03");
  outtextxy(324,237,"Closed");
  outtextxy(593,237,"Open");
  outtextxy(415,237,"Valve Position");
  outtextxy(10,237,"Main Menu:");
  outtextxy(324,321,"Current Settings");   /* Box headings */

  setcolor(GREY);
  moveto(324,258);
  lineto(630,258);  /*line between scale and position displays*/

  setcolor(BLUE);
  outtextxy(183,35,"Mclennan valve Valve Controller");
  outtextxy(153,60,"I - Drive Current Overload");
  outtextxy(153,60+12,"t - Overtemperature in Drive");
  outtextxy(153,60+2*12,"b - Busy (Moving)");
  outtextxy(153,60+3*12,"r - Ready");
  outtextxy(153,60+4*12,"C - Valve Closed Limit Switch");
  outtextxy(153,60+5*12,"0 - Valve Open Limit Switch");
  outtextxy(153,60+6*12,"L - Low Current Loop");
  outtextxy(153,60+7*12,"H - High Current Loop");
  outtextxy(153,60+8*12,"F - Failure to Hit Limit Switch");
  outtextxy(153,60+9*12,"S - Switches Wired Incorrectly"); /*writing on valve*/

  setcolor(WHITE);
  outtextxy(329,345,"Command Position  ");
  outtextxy(329,346+14,"Actual Position  ");
  outtextxy(329,347+2*14,"Deadband  ");         /*display value headings*/
  outtextxy(329,348+3*14,"Upper Range  ");
  outtextxy(329,349+4*14,"Lower Range  ");
  outtextxy(329,351+6*14,"Slew Speed  ");
  outtextxy(329,350+5*14,"Acceleration  ");
  outtextxy(329,352+7*14,"Base Speed  ");

  outtextxy(14,255+0*14,"Command keys are:");
  outtextxy(14,255+2*14,"D - Set Deadband");
  outtextxy(14,255+3*14,"U - Set Upper Range");
  outtextxy(14,255+4*14,"L - Set Lower Range");
  outtextxy(14,255+5*14,"A - Set Acceleration Rate");
  outtextxy(14,255+6*14,"S - Set Slew Speed");
  outtextxy(14,255+7*14,"B - Set Base Speed");
  outtextxy(14,255+8*14,"P - Set PIN");
  outtextxy(14,255+9*14,"Q - Quit the program");		/*display commands*/

  itoa(dead,text,10);
  outtextxy(590,347+2*14,text);
  itoa(lo,text,10);
  outtextxy(590,349+4*14,text);
  itoa(speed,text,10);
  outtextxy(590,351+6*14,text);
  itoa(bspeed,text,10);
  outtextxy(590,352+7*14,text);
  ltoa(accel,text,10);
  outtextxy(590,350+5*14,text);
  itoa(up,text,10);
  outtextxy(590,348+3*14,text);				/*display initial values*/

  outtextxy(130,30,"0");
  outtextxy(500,30,"0");
  outtextxy(130,205,"0");
  outtextxy(500,205,"0");						/*draw screws on valve!!!*/

  scale (up_disp,lo_disp);       /*Show scale*/
  value ((actpos*2.75*(up-lo))/100,lo_disp,up_disp);	/*show initial position*/
  olddisp=-1;    /*nullify old display value*/
}

int query()
{/*load current values from valve flash*/
int ptr,ch=0,x=1,i;



			  /*version 1.02 has delay(1000)here*/

do{    delay(10);
		 ch = s.get_char();
		 delay(10);                       /*test for comms connection*/
		 if (ch!=0x0d)s.put_char(0x0d);
		 delay(10);
		 ch = s.get_char();

						if (ch!=0x0d){
											/*tell the user there's no connection*/
								fillbox(11,2*365,313,469,BLACK,BLACK);
									setcolor(WHITE);
									outtextxy(12,365+5*14,"No Comms Connection!");
																		return 1;
																		}
		 }
		 while (ch!=0x0d);
s.put_char('1');
s.put_char('q');
s.put_char('a');        /*send the qa command*/
s.put_char(0x0d);
delay(10);
do{
do{
		 ch = s.get_char();
		 delay(10);
				 }
		 while (ch!=':');

		ptr=0;
		do
			{
					 do{
							 ch = (int)s.get_char();
							 delay(10);
									}while (ch==-1);

						 if ((ptr<17) &&  ((ch>='0')&&(ch<='9')) )
						 {
						 text[ptr]=ch; /*make a string from the response*/
						 text[ptr+1]=0;
						 ptr=ptr+1;}
							  }
			while (ch!=0x0d);
			switch(x){/*find out the values for the appropriate variables. These are done based on the order they are sent in*/
			case 1:{dead=strtod(text, &eos);break;}
			case 2:{speed=strtod(text, &eos);break;}
			case 3:{accel=strtod(text, &eos)*1000;break;}
			case 4:{up=(strtod(text, &eos)/4095)*100;break;}
			case 5:{lo=(strtod(text, &eos)/4095)*100;break;}
			case 6:{bspeed=strtod(text, &eos);break;}
			case 7: break;
			}
			x=x+1;
			}
			while (x<7);

			return 0;
}


void set_upper()
{/*all the set functions work on the same principle. They could have easily been combined*/
	/*into one function, but they weren't*/

int ch, error=0;

	 s.put_char('1');
	 s.put_char('p');
	 s.put_char('o');
	 s.put_char(0x0d);		/*send the PASS OK command*/
	 do{
		 ch = s.get_char();
									}
		 while (ch!=':');

	 s.put_char('s');         /*send the set command*/
	 s.put_char('u');
	 s.put_char(text[0]);
	 s.put_char(text[1]);
	 s.put_char(text[2]);
	 s.put_char(0x0d);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');

		 do{
		 do{
		 ch = s.get_char();
		 }
		 while (ch==-1);
		 if (ch==75) error=1;        /*check response to see if OK or ERROR */
	 }while (ch!=0x0d);
	 if (error==0){
						setcolor(WHITE);
						outtextxy(12,365+5*14,"ERROR!");
						delay(1000);
						query();					/*if error, find out what the current stored values are*/
						procinit();          /*re-draw screen*/
						}

	 }

void set_lower()
{
int ch, error=0;


	 s.put_char('1');
	 s.put_char('p');
	 s.put_char('o');
	 s.put_char(0x0d);
	 delay(10);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');

	 s.put_char('1');
	 s.put_char('s');
	 s.put_char('l');
	 s.put_char(text[0]);
	 s.put_char(text[1]);
	 s.put_char(text[2]);
	 s.put_char(0x0d);
	 delay(10);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');

		 do{
		 do{
		 ch = s.get_char();
		 }
		 while (ch==-1);
		 if (ch==75) error=1;
	 }while (ch!=0x0d);
	 if (error==0){
						setcolor(WHITE);
						outtextxy(12,365+5*14,"ERROR!");
						delay(1000);
						query();
						procinit();
						}

}

void set_dead()
{
int ch, error=0;


	 s.put_char('1');
	 s.put_char('p');
	 s.put_char('o');
	 s.put_char(0x0d);
	 delay(10);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');


	 s.put_char('1');
	 s.put_char('s');
	 s.put_char('d');
	 s.put_char(text[0]);
	 s.put_char(text[1]);
	 s.put_char(text[2]);
	 s.put_char(0x0d);
	 delay(10);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');

		 do{
		 do{
		 ch = s.get_char();
		 }
		 while (ch==-1);
		 if (ch==75) error=1;
	 }while (ch!=0x0d);
	 if (error==0){
						setcolor(WHITE);
						outtextxy(12,365+5*14,"ERROR!");
						delay(1000);
						query();
						procinit();
						}


	 }



void set_speed()
{
int ch, error=0;


	 s.put_char('1');
	 s.put_char('p');
	 s.put_char('o');
	 s.put_char(0x0d);
	 delay(10);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');


	 s.put_char('1');
	 s.put_char('s');
	 s.put_char('s');
	 s.put_char(text[0]);
	 s.put_char(text[1]);
	 s.put_char(text[2]);
	 s.put_char(text[3]);
	 s.put_char(text[4]);
	 s.put_char(0x0d);
	 delay(10);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');

		 do{
		 do{
		 ch = s.get_char();
		 }
		 while (ch==-1);
		 if (ch==75) error=1;
	 }while (ch!=0x0d);
	 if (error==0){
						setcolor(WHITE);
						outtextxy(12,365+5*14,"ERROR!");
						delay(1000);
						query();
						procinit();
						}

	 }


void set_bspeed()
{
int ch, error=0;


	 s.put_char('1');
	 s.put_char('p');
	 s.put_char('o');
	 s.put_char(0x0d);
	 delay(10);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');


	 s.put_char('1');
	 s.put_char('s');
	 s.put_char('b');
	 s.put_char(text[0]);
	 s.put_char(text[1]);
	 s.put_char(text[2]);
	 s.put_char(text[3]);
	 s.put_char(text[4]);
	 s.put_char(0x0d);
	 delay(10);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');
			 do{
		 do{
		 ch = s.get_char();
		 }
		 while (ch==-1);
		 if (ch==75) error=1;
		 //else error=0;
	 }while (ch!=0x0d);
	 if (error==0){

						setcolor(WHITE);
						outtextxy(12,365+5*14,"ERROR!");
						delay(1000);
						query();
						procinit();
						}

	 }

void set_accel()
{
int ch, error=0;


	 s.put_char('1');
	 s.put_char('p');
	 s.put_char('o');
	 s.put_char(0x0d);
	 delay(10);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');

  ltoa(accel,text,10);


	 s.put_char('1');
	 s.put_char('s');
	 s.put_char('a');
	 s.put_char(text[0]);
	 s.put_char(text[1]);
	 s.put_char(text[2]);
	 s.put_char(0x0d);
	 delay(10);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');

		 do{
		 do{
		 ch = s.get_char();
		 }
		 while (ch==-1);
		 if (ch==75) error=1;
	 }while (ch!=0x0d);
	 if (error==0){
						setcolor(WHITE);
						outtextxy(12,365+5*14,"ERROR!");
						delay(1000);
						query();
						procinit();
						}

	 }


void setpass()
{
int ch, error=0;



	 s.put_char('1');
	 s.put_char('n');
	 s.put_char('p');
	 s.put_char(text[0]);
	 s.put_char(text[1]);
	 s.put_char(text[2]);
	 s.put_char(text[3]);
	 s.put_char(0x0d);
	 delay(10);
	 do{
		 ch = s.get_char();
		 }
		 while (ch!=':');

		 do{
		 do{
		 ch = s.get_char();
		 }
		 while (ch==-1);
		 if (ch==75) error=1;
	 }while (ch!=0x0d);
	 if (error==0){
						setcolor(WHITE);
						outtextxy(12,365+5*14,"ERROR!");
						delay(1000);
						query();
						procinit();
						}

	 }

void checkpin()
{
/*this checks that the PIN is OK*/
int ptr,ch=0;


	 temppass=0;
	 s.put_char('1');
	 s.put_char('p');
	 s.put_char('i');
	 s.put_char(text[0]);
	 s.put_char(text[1]);        /*PIN number*/
	 s.put_char(text[2]);
	 s.put_char(text[3]);
	 s.put_char(0x0d);
	  s.put_char(0x0a);
	 delay(10);

	 do{
		 ch = s.get_char();
		 delay(10);
		 }
		 while (ch!=':');

do{
		 do{
		 ch = (int)s.get_char();
		 delay(10);
		 }
		 while (ch==-1);

		  if (ch!=0x0d){
			if (ch=='1')temppass=1;     /*if password is OK set temppass=1*/
			else temppass=0;
				}
							  }
		while (ch!=0x0d);
	 }

void badpin()
{
int ptr,ch=0;
										/*send a bad PIN to reset privilige level on PM600*/
	 s.put_char('1');
	 s.put_char('p');
	 s.put_char('i');
	 s.put_char('0');
	 s.put_char('0');
	 s.put_char('0');
	 s.put_char('0');
	 s.put_char(0x0d);
	  s.put_char(0x0a);
	 delay(10);

	 do{
		 ch = s.get_char();
		 delay(10);
		 }
		 while (ch!=0x0d);

}

int main(void)
{
	int display, oldact=0;
	char key=NULL;
	int r=10;
	double x;
	double y=0;
	double inpos;
	double steps=4095, offset=0;
	int ptr,ch=0,callib=0;

	graphics_on();		/*turn on graphics*/
	up=5000;          /*set up to a ludicrous value which will only be changed when query() happens*/
	outtextxy(12,365+4*14,"Searching Comms Connection");
	delay(750);
	do{ if (query()){/*if query doesn't work, start countdown to terminating program*/
		 setcolor(WHITE);
		 outtextxy(12,365+7*14,"Terminating in:");
		 fillbox(151,365+7*14,170,365+8*14,BLACK,BLACK);
		 itoa(r,text,10);
		 setcolor(WHITE);
		 outtextxy(151, 365+7*14, text);
		 delay(1000); /*version 1.02 doesn't have this line*/
		 r=r-1;
		 if (r<0) return 0;}
		 }while (up==5000);
	x = up;
	y = (x/100)*275;
	up_disp= y;
	if (lo==up) x=101;                          /*calculate scale after values received*/
	else x = lo;
	y = (x/100)*275;
	lo_disp= y;

	procinit();                         /*set up graphics.....*/
												/*display commands etc.*/
	actpos=0;
	value ((actpos*2.75*(up-lo))/100,lo_disp,up_disp); /*show graphic position*/
  do
  {

  do
			{

					if (qwerty_input())
					{                     /*if the user hits a command key....*/
					key=getch();

					if (key=='D' || 'L' || 'A' ||'S' || 'U' || 'Q' || 'P' || 'B')quit=TRUE;
					/*if any of these keys is hit, go on to perform the task*/
					}
					else {
			 if (actpos<101)          /*keep actual value below 100%*/
				{
							get_act_pos();         /*get actual pos*/
							inpos = strtod(text, &eos);      /*convert inpos to a number*/
							y=(inpos/(steps-offset))*100;      /*turn into a percentage*/

							actpos=y*(up_disp/(up_disp-lo_disp));
								/*convert input position to a percentage*/
								showact(actpos);                    /*show actual position*/
							if (actpos!=oldact){/*if position is the same as the last one, don't redraw the screen*/

							value ((actpos*2.75*(up-lo))/100,lo_disp,up_disp); /*show graphic position*/
                      }
							oldact=actpos; /*register present value*/

							get_status();	/*get display status*/

							display=strtod(text, &eos);
							if(display!=olddisp){ /*if display is the same as last time, don't redraw*/
							if (callib==1){   /*if a callibration jut happened, re-check vales*/
												query();
												x = up;
												y = (x/100)*275;
												up_disp= y;
												if (lo==up) x=101;
												else x = lo;
												y = (x/100)*275;
												lo_disp= y;

												procinit();
																		 /*and re-draw everything*/
												actpos=100;
												value ((actpos*2.75*(up-lo))/100,lo_disp,up_disp); /*show graphic position*/
												callib=0;}
							display=strtod(text, &eos);/*convert display character received to a decimal*/
							switch(display)
							{
							case 0:{ssdisp(0,0,0,1,0,0,0,0,0,0);break;}
							case 1:{ssdisp(0,0,1,0,0,0,0,0,0,0);break;}
							case 2:{ssdisp(0,0,0,0,0,1,0,0,0,0);break;}
							case 3:{ssdisp(0,0,0,0,0,0,1,0,0,0);break;}
							case 4:{ssdisp(0,0,0,0,0,0,0,1,0,0);break;}
							case 5:{ssdisp(0,0,0,0,0,0,0,0,1,0);break;}    /*show appropriate display*/
							case 6:{ssdisp(0,0,0,0,0,0,0,0,0,1);break;}
							case 7:{ssdisp(0,0,0,0,1,0,0,0,0,0);break;}
							case 8:{ssdisp(0,1,0,0,0,0,0,0,0,0);break;}
							case 9:{ssdisp(1,0,0,0,0,0,0,0,0,0);break;}
							case 10:{fillbox(475,150,425,75,BLACK,WHITE);
										disp(0,0,0,0,0,0,1,0);callib=1;break;}
							 }                   }
							 olddisp=display;  /*register old display*/

							 get_com_pos(); /*get command position*/

							 x=strtod(text, &eos);
							 if (x<0)x=0;
							 if(x>4095)x=4095;
							 compos=x/4095*100; /*convert to a percentage*/
							 showcom(compos); /*show it*/


							quit=FALSE;}

						  /*stay doing this loop until the user presses a key*/

									}
										}
  while (quit==FALSE);                               /*....leave this loop*/

  key=key & 0xDF;

  if (key==0)
	{
	key=(int)getch();
	key=0;               /* Ignore special keys */
	}

	switch (key)
		{

		 case 'D' : {     /*set the deadband*/
						getpass(enterpass);   /*get user input*/
						checkpin();           /*check to see if PIN is OK*/

						fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
						setcolor(WHITE);
						if (temppass==pass)  /*if PIN is OK...*/
						{
						  gettext(enterdead,0,100);		/*get user input*/
						  fillbox(589,346+2*14,628,355+2*14,BLACK,BLACK);/*delete old value*/
						  setcolor(WHITE);
						  outtextxy(590,346+2*14,text); /*display new value*/
						  fillbox(11,365+2*14,313,469,BLACK,BLACK); /*cover up question*/
						  set_dead();    /*set the value on the valve*/
						  dead = strtod(text, &eos);      /*convert to a number*/

						  badpin();  /*send a bad PIN to reset the privilege level*/
						  }
						  else{ /*if PIN is wrong*/
								outtextxy(12,365+5*14,"Incorrect PIN!");
								delay (1000);
								fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
								setcolor(WHITE);
								}
						  quit=FALSE;            /*reset the quit variable*/
						  break;
						  }

		 case 'S':  {     /*set the slew speed*/
						getpass(enterpass);   /*get user input*/
						checkpin();				/*check to see if PIN is OK*/

						fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
						setcolor(WHITE);
						if (temppass==pass)/*if PIN is OK...*/
						{
						  gettext(enterspeed,0,10001);       /*get user input*/
						  fillbox(590,351+6*14,628,360+6*14,BLACK,BLACK); /*delete old value*/
						  setcolor(WHITE);
						  outtextxy(590,351+6*14,text); /*display new value*/
						  fillbox(11,365+2*14,313,469,BLACK,BLACK); /*cover up question*/
						  set_speed();   /*set the value on the valve*/
						  speed = strtod(text, &eos);     /*convert to a number*/

						  badpin();  /*send a bad PIN to reset the privilege level*/
						  }
						  else{ /*if PIN is wrong*/
								outtextxy(12,365+4*14,"Incorrect PIN!");
								delay (1000);
								fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
								setcolor(WHITE);
								}
						  quit=FALSE;            /*reset the quit variable*/
						  break;
						  }

		 case 'A' : {     /*set the acceleration*/
						getpass(enterpass);   /*get user input*/
						checkpin();           /*check to see if PIN is OK*/

						fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
						setcolor(WHITE);
						if (temppass==pass) /*if PIN is OK...*/
						{
						  gettext(enteraccel,1000,100000);    /*get user input*/
						  fillbox(589,350+5*14,628,359+5*14,BLACK,BLACK); /*delete old value*/
						  setcolor(WHITE);
						  outtextxy(590,350+5*14,text);
						  accel = strtod(text, &eos)/1000;
						  set_accel();    /*set the value on the valve*/
							  /*convert to a number*/



						  fillbox(11,365+2*14,313,469,BLACK,BLACK); /*cover up question*/
						  badpin(); /*send a bad PIN to reset the privilege level*/
						  }
						  else{ /*if PIN is wrong*/
								outtextxy(12,365+4*14,"Incorrect PIN!");
								delay (1000);
								fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
								setcolor(WHITE);
								}
						  quit=FALSE;            /*reset the quit variable*/
						  break;
						  }


		case 'U':   {     /*set the upper limit*/
						getpass(enterpass);   /*get user input*/
						checkpin();           /*check to see if PIN is OK*/

						fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
						setcolor(WHITE);
						if (temppass==pass) /*if PIN is OK...*/
						{
						  gettext(enterup,lo+1,100);     /*get user input*/
						  fillbox(589,347+3*14,628,355+3*14,BLACK,BLACK); /*delete old value*/
						  setcolor(WHITE);
						  outtextxy(590,347+3*14,text); /*display new value*/
						  set_upper();	/*set the value on the valve*/
						  x = up;
						  y = (x/100)*275;
						  up_disp= y;                          /*calculate new scale*/
						  fillbox(11,365+2*14,313,469,BLACK,BLACK);  /*cover up question*/
						  scale (up_disp,lo_disp);             /*show new scale*/
						  value ((actpos*2.75*(up-lo))/100,lo_disp,up_disp); /*show graphic position*/

						  up = strtod(text, &eos);       /*convert to a number*/

						  badpin(); /*send a bad PIN to reset the privilege level*/
						}
													  /*reset the quit variable*/
						  else{ /*if PIN is wrong*/
								outtextxy(12,365+4*14,"Incorrect PIN!");
								delay (1000);
								fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
								setcolor(WHITE);
								}
								quit=FALSE;
						  break;
						  }

		case 'L':   {      /*set the lower limit*/
						getpass(enterpass);   /*get user input*/
						checkpin();           /*check to see if PIN is OK*/

						fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
						setcolor(WHITE);
						if (temppass==pass) /*if PIN is OK...*/
						{
						  gettext(enterlo,0,up-1);   /*get user input*/
						  lo = strtod(text, &eos);   /*convert to a number*/
						  fillbox(589,348+4*14,628,355+4*14,BLACK,BLACK);  /*delete old value*/
						  setcolor(WHITE);
						  outtextxy(590,348+4*14,text); /*display new value*/
						  set_lower();   /*set the value on the valve*/
						  x = lo;
						  y = (x/100)*275;
						  lo_disp= y;                          /*calculate new scale*/
						  fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
						  scale (up_disp,lo_disp);               /*show new scale*/
						  value ((actpos*2.75*(up-lo))/100,lo_disp,up_disp); /*show graphic position*/


						  badpin();      /*send a bad PIN to reset the privilege level*/
						  }
						  else{/*if PIN is wrong*/
								outtextxy(12,365+4*14,"Incorrect PIN!");
								delay (1000);
								fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
								setcolor(WHITE);
								}
						  quit=FALSE;                       /*reset the quit variable*/
						  break;

						  }
		case 'P':{  getpass(enteroldpass);   /*get user input*/
						checkpin();              /*check to see if PIN is OK*/

						fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
						setcolor(WHITE);
						if (temppass==pass) /*if PIN is OK...*/
						{

						getpass(enternewpass);
						setpass(); /*set the value on the valve*/

						fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
						setcolor(WHITE);

						badpin();   /*send a bad PIN to reset the privilege level*/

						 }
						else{
								outtextxy(12,365+4*14,"Incorrect PIN!");
								delay (1000);
								fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
								setcolor(WHITE);
								}

						quit=FALSE;                     /*reset the quit variable*/
						  break;
					}

					case 'B':  {     /*set the slew speed*/
						getpass(enterpass);   /*get user input*/
						checkpin();           /*check to see if PIN is OK*/

						fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
						setcolor(WHITE);
						if (temppass==pass) /*if PIN is OK...*/
						{
						  gettext(enterbspeed,0,speed);       /*get user input*/
						  fillbox(589,363+6*14,628,372+6*14,BLACK,BLACK); /*delete old value*/
						  setcolor(WHITE);
						  outtextxy(590,351+7*14,text); /*display new value*/
						  fillbox(11,365+2*14,313,469,BLACK,BLACK); /*cover up question*/
						  set_bspeed();          /*set the value on the valve*/
						  bspeed = strtod(text, &eos);     /*convert to a number*/

						  badpin();    /*send a bad PIN to reset the privilege level*/
						  }
						  else{ /*if PIN is wrong*/
								outtextxy(12,365+4*14,"Incorrect PIN!");
								delay (1000);
								fillbox(11,365+2*14,313,469,BLACK,BLACK);   /*cover up question*/
								setcolor(WHITE);
								}
						  quit=FALSE;            /*reset the quit variable*/
						  break;
						  }

		 }

  }
  while (key != 'Q');      /*until user presses 'q' for quit*/

  graphics_off();
  clrscr();  /* Clear the screen */


  return 0;
}


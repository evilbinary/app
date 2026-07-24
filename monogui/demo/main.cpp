// main.cpp

#include "main.h"

// Variables of X Window
// static Display *display;
static int screen_num;
// static Window window;
// static XSetWindowAttributes attribute;
// static GC gc;
// static Visual* visual;
static int bitmap_depth;
// static Pixmap lcdpix;
static unsigned short imgbuf[LCDW*LCDH];

// Varialbes of TGUI System
static CLCD             g_LCD;
static CTApp            g_App;
static CMainWindow      g_MainWindow;

void lcd_display();

int main2( int argc, char** argv )
{
  // ��ʼ��Һ����
  g_LCD.LCDInitVirtualLCD( lcd_display );

  // ����������
  g_MainWindow.Create(NULL,TWND_TYPE_DESKTOP,TWND_STYLE_NORMAL,TWND_STATUS_NORMAL,
		      0,0,SCREEN_W,SCREEN_H,0);

  // Ӧ�ó���ĳ�ʼ��
  g_App.Create( &g_LCD, &g_MainWindow );

  // �趨��д����Ķ˿�(����1)
  g_App.EnableGraffiti( 1 );

  // ��X����������
//   char* display_name = NULL;
//   if( (display = XOpenDisplay(display_name)) == NULL )
//     {
//       printf( "Cannot connect to X server %s \n", XDisplayName(display_name) );
//       exit( -1 );
//     }

  // ���ȱʡ��screen_num
//   screen_num = DefaultScreen( display );
//   visual = DefaultVisual(display, screen_num);
//   bitmap_depth = DefaultDepth(display, screen_num);
//   if(bitmap_depth != 16)
//     {
//       fprintf(stderr, "Only support 16bpp X server\n");
//       exit(-1);
//     }

//   attribute.backing_store	= Always;
//   attribute.background_pixel	= BlackPixel(display, screen_num);

//   // ��������
//   window = XCreateWindow(display,
// 			 RootWindow(display, screen_num),
// 			 0, 0, LCDW, LCDH,
// 			 2, bitmap_depth,
// 			 InputOutput,
// 			 visual,
// 			 CWBackPixel | CWBackingStore,
// 			 &attribute );

//   // ����λͼ
//   lcdpix = XCreatePixmap(display, RootWindow(display, screen_num), LCDW, LCDH, bitmap_depth);

//   // ����GC
//   gc = XCreateGC( display, window, 0, NULL );

//   // ��ʾ����
//   XMapWindow( display, window );

//   // ѡ�񴰿ڸ���Ȥ���¼�
//   XSelectInput( display, window, 
// 		ExposureMask      |
// 		KeyPressMask      |
// 		ButtonPressMask   |
// 		ButtonReleaseMask |
// 		Button1MotionMask |
// 		StructureNotifyMask );

  // �����¼�ѭ��
//   XEvent event;
//   while( 1 )
//     {
//       // ȡ�ö����е��¼�
//       XNextEvent( display, &event );
//       switch( event.type )
// 	{
// 	case Expose:
// 	  // �ع��¼�,����Ӧ�ػ�
// 	  // ȡ�����һ���ع��¼�
// 	  if( event.xexpose.count != 0 )
// 	    break;

// 	  // ����ͼ��
// 	  g_MainWindow.Paint(&g_LCD);
// 	  lcd_display();
// 	  break;
// 	case ConfigureNotify:
// 	  XResizeWindow(display,window,LCDW,LCDH);
// 	  break;
// 	case ButtonPress:
// 	case ButtonRelease:
// 	  if( event.xbutton.button == Button1 )
// 	    {
// 	      int nMessageType = 0;
// 	      switch( event.type )
// 		{
// 		case ButtonPress:    nMessageType=TMSG_LBUTTONDOWN; break;
// 		case ButtonRelease:  nMessageType=TMSG_LBUTTONUP;   break;
// 		}
// 	      // ��ϵͳ��Ϣת�͸�HSGUI����Ϣ����
// 	      g_App.MouseMessage( nMessageType, event.xbutton.x, event.xbutton.y );
// 	    }
// 	  break;
// 	case MotionNotify:
// 	  // ��ϵͳ��Ϣת�͸�HSGUI����Ϣ����
// 	  g_App.MouseMessage( TMSG_MOUSEMOVE, event.xbutton.x, event.xbutton.y );
// 	  break;
// 	case KeyPress:
// 	  char buffer[1024];
// 	  KeySym keysym;
// 	  XComposeStatus compose;
// 	  XLookupString( &event.xkey, buffer, 1024, &keysym, &compose );
// 	  printf("Key Value is: 0X%x\n",keysym);
// 	  g_App.KeyMessage( keysym );
// 	  break;
// 	default:
// 	  break;
// 	}

//       // TGUI����
//       g_App.RunInEmulator();
//     }


}

void lcd_display()
{
//   XImage* image;
  int x, y;

  unsigned char* fb;
  int w, h;
	fb = g_LCD.LCDGetFB( &w, &h );
    for( y=0; y<h; y++ )
    {
      for( x=0; x<w; x++ )
	{

		

	  if( g_LCD.LCDGetPixel( fb, w, h, x, y ) )
	  	screen_put_pixel(x, y,0x000000);
	//     imgbuf[x+y*LCDW] = COLORWHITE;
	  else
	  	screen_put_pixel(x, y,0xffffff);
	    // imgbuf[x+y*LCDW] = COLORBLACK;
	}
    }
//   fb = g_LCD.LCDGetFB( &w, &h );
  
//   for( y=0; y<h; y++ )
//     {
//       for( x=0; x<w; x++ )
// 	{
// 	  if( g_LCD.LCDGetPixel( fb, w, h, x, y ) )
// 	    imgbuf[x+y*LCDW] = COLORWHITE;
// 	  else
// 	    imgbuf[x+y*LCDW] = COLORBLACK;
// 	}
//     }
  
//   image = XCreateImage(display, visual, 16, ZPixmap, 0, (char *)imgbuf,
// 		       LCDW, LCDH, 16, 0);
//   XPutImage(display, lcdpix, gc, image, 0, 0, 0, 0, LCDW, LCDH);
//   XCopyArea(display, lcdpix, window, gc, 0, 0, LCDW, LCDH, 0, 0);
//   XFree(image);
}



#define true 1
#define false 0

#ifdef X86
#define WIDTH 1024
#define HEIGHT 768
#else

#define WIDTH 480
#define HEIGHT 272
#endif

int main(int argc, char* args[]) {
  SDL_Window* window;
  SDL_Renderer* renderer;
  // Initialize SDL.
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    SDL_Log("SDL_Init(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
    return 1;
  }

  // Create the window where we will draw.
  window = SDL_CreateWindow("SDL_RenderClear", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
  SDL_assert(window);
  // We must call SDL_CreateRenderer in order for draw calls to affect this
  // window.
  renderer = SDL_CreateRenderer(window, -1, 0);

  // Select the color for drawing. It is set to red here.
  SDL_SetRenderDrawColor(renderer, 0,0, 0, 0);

  // Clear the entire screen to our selected color.
  SDL_RenderClear(renderer);

  // Up until now everything was drawn behind the scenes.
  // This will show the new, red contents of the window.
  SDL_RenderPresent(renderer);

  SDL_StartTextInput();
  // Give us time to see the window.
  // SDL_Delay(5000);


  g_LCD.LCDInitVirtualLCD( lcd_display );

  // ����������
  g_MainWindow.Create(NULL,TWND_TYPE_DESKTOP,TWND_STYLE_NORMAL,TWND_STATUS_NORMAL,
		      0,0,SCREEN_W,SCREEN_H,0);

  // Ӧ�ó���ĳ�ʼ��
  g_App.Create( &g_LCD, &g_MainWindow );

  // �趨��д����Ķ˿�(����1)
  g_App.EnableGraffiti( 1 );


  int is_quit = 0;
  while (!is_quit) {
    SDL_Event event;
    if (SDL_PollEvent(&event)) {
      printf("sdl event %d\n", event.type);

      switch (event.type) {
        case SDL_QUIT:
          is_quit = 1;
          break;
        case SDL_MOUSEMOTION:
          printf("SDL mouse motion: x=%d,y=%d\n", event.motion.x,
                 event.motion.y);
          break;
        case SDL_MOUSEBUTTONDOWN:
          if (event.button.button == SDL_BUTTON_LEFT) {
            printf("SDL button left down\n");
            SDL_ShowCursor(false);
          } else if (event.button.button == SDL_BUTTON_MIDDLE) {
            printf("SDL button middle down\n");
          } else if (event.button.button == SDL_BUTTON_RIGHT) {
            printf("SDL button right down\n");
            SDL_ShowCursor(true);
          }
          break;
        case SDL_MOUSEBUTTONUP:
          if (event.button.button == SDL_BUTTON_LEFT) {
            printf("SDL button left up\n");
          } else if (event.button.button == SDL_BUTTON_MIDDLE) {
            printf("SDL button middle up\n");
          } else if (event.button.button == SDL_BUTTON_RIGHT) {
            printf("SDL button right up\n");
          }
          break;
        case SDL_MOUSEWHEEL:
          printf("SDL wheel dir:%d\n", event.wheel.direction);
          printf("SDL wheel x:%d, y:%d\n", event.wheel.x, event.wheel.y);
          break;
        case SDL_KEYDOWN:
          printf("SDL key down sym:%x(%s), mod:%x\n", event.key.keysym.sym,
                 SDL_GetKeyName(event.key.keysym.sym), event.key.keysym.mod);
          break;
        case SDL_KEYUP:
          printf("SDL key up sym:%x(%s), mod:%x\n", event.key.keysym.sym,
                 SDL_GetKeyName(event.key.keysym.sym), event.key.keysym.mod);
          break;
        case SDL_WINDOWEVENT:
          printf("SDL window event %d\n", event.window.event);
          break;
        case SDL_TEXTINPUT:
          printf("SDL text input %s\n", event.text.text);
          break;
        default:
          break;
      }
    }
	g_MainWindow.Paint(&g_LCD);
	lcd_display();

    SDL_Delay(10);
  }
  printf("sdl exit\n");
  SDL_Quit();
  return 0;
}


// END

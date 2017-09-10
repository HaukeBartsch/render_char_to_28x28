
/* ./renderText "/Library/Fonts/Times New Roman.ttf" A */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <ft2build.h>
#include FT_FREETYPE_H


#define WIDTH   28
#define HEIGHT  28


/* origin is the upper left corner */
unsigned char image[HEIGHT][WIDTH];


/* Replace this function with something useful. */

void
draw_bitmap( FT_Bitmap*  bitmap,
             FT_Int      x,
             FT_Int      y)
{
  FT_Int  i, j, p, q;
  FT_Int  x_max = x + bitmap->width;
  FT_Int  y_max = y + bitmap->rows;


  for ( i = x, p = 0; i < x_max; i++, p++ )
  {
    for ( j = y, q = 0; j < y_max; j++, q++ )
    {
      if ( i < 0      || j < 0       ||
           i >= WIDTH || j >= HEIGHT )
        continue;

      image[j][i] |= bitmap->buffer[q * bitmap->width + p];
    }
  }
}


void
show_image( void )
{
  int  i, j;


  for ( i = 0; i < HEIGHT; i++ ) {
    for ( j = 0; j < WIDTH; j++ )
      putchar( image[i][j] == 0 ? ' '
                                : image[i][j] < 64 ? '.' : ( image[i][j] < 128 ? '+'
                                                    : '*' ));
    putchar( '\n' );
  }
}


void
show_json( char* c )
{
  int  i, j;

  int label[] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
                  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z' };
  fprintf(stdout, "{ \"label\": [ ");
  int sizeOfLabel = sizeof(label)/sizeof(*label);
  for (int i = 0; i < sizeOfLabel; i++) {
      if ( label[i] == (int)c[0] )
         fprintf(stdout, "1");
      else
         fprintf(stdout, "0");
      if (i < sizeOfLabel-1) 
         fprintf(stdout, ",");
  }
  fprintf(stdout, " ],\n");

  fprintf(stdout, "  \"image\": [ ");
  for (i = 0; i < HEIGHT; i++)
  {
      for (j = 0; j < WIDTH; j++)
      {
          fprintf(stdout, "%d", (int)image[i][j]);
          if (!((i == HEIGHT - 1 && j == WIDTH - 1)))
              fprintf(stdout, ",");
      }
  }
  fprintf(stdout, " ] }\n");
}

int
main( int     argc,
      char**  argv )
{
  FT_Library    library;
  FT_Face       face;

  FT_GlyphSlot  slot;
  FT_Matrix     matrix;                 /* transformation matrix */
  FT_Vector     pen;                    /* untransformed origin  */
  FT_Error      error;

  char*         filename;
  char*         text;

  double        angle;
  int           target_height;
  int           n, num_chars;
  char*         json;


  if ( argc < 3 )
  {
    fprintf ( stderr, "usage: %s font_file.ttf sample-text json|text\n", argv[0] );
    exit( 1 );
  }

  filename      = argv[1];                           /* first argument     */
  text          = argv[2];                           /* second argument    */
  json          = (char *)"text";
  if (argc == 4) {
     json          = argv[3];
  }
  num_chars     = strlen( text );
  angle         = 0; // ( 25.0 / 360 ) * 3.14159 * 2;      /* use 25 degrees     */
  target_height = HEIGHT;

  error = FT_Init_FreeType( &library );              /* initialize library */
  /* error handling omitted */

  error = FT_New_Face( library, filename, 0, &face );/* create face object */
  /* error handling omitted */
  if (face == NULL) {
      fprintf(stderr, "Error: no face found, provide the filename of a ttf file...\n");
      exit(-1);
  }

  /* use 50pt at 100dpi */
  error = FT_Set_Char_Size( face, 20 * 64, 0,
                            90, 0 );                /* set character size */
  /* error handling omitted */

  slot = face->glyph;

  /* set up matrix */
  matrix.xx = (FT_Fixed)( cos( angle ) * 0x10000L );
  matrix.xy = (FT_Fixed)(-sin( angle ) * 0x10000L );
  matrix.yx = (FT_Fixed)( sin( angle ) * 0x10000L );
  matrix.yy = (FT_Fixed)( cos( angle ) * 0x10000L );

  /* the pen position in 26.6 cartesian space coordinates; */
  /* start at (300,200) relative to the upper left corner  */
  pen.x = 1 * 64;
  pen.y = ( target_height - 20 ) * 64;

  for ( n = 0; n < num_chars; n++ )
  {
    /* set transformation */
    FT_Set_Transform( face, &matrix, &pen );

    /* load glyph image into the slot (erase previous one) */
    error = FT_Load_Char( face, text[n], FT_LOAD_RENDER );
    if ( error )
      continue;                 /* ignore errors */

    /* now, draw to our target surface (convert position) */
    draw_bitmap( &slot->bitmap,
                 slot->bitmap_left,
                 target_height - slot->bitmap_top );

    /* increment pen position */
    pen.x += slot->advance.x;
    pen.y += slot->advance.y;
  }

  if (strcmp(json, "text")== 0) {
      show_image();
  } else {
      show_json( text );
  }

  FT_Done_Face    ( face );
  FT_Done_FreeType( library );

  return 0;
}

/* EOF */
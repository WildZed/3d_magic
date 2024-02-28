#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>    /* Only include <sys/types.h> once. */
#include <sys/stat.h>
#include <sys/param.h>
#include <stdlib.h>

#define PROGNAME "3d_magic"             /* Used in resource database. */
#define True 1
#define False 0
#define RED 0
#define GREEN 1
#define BLUE 2

/* MONO returns total intensity of r,g,b components */
#define MONO(rd,gn,bl) (((rd)*11 + (gn)*16 + (bl)*5) >> 5)  /*.33R+ .5G+ .17B*/

typedef unsigned char byte;

/* Picture structure for LoadGIF routine. */
typedef struct pic_s {
  byte	*pic;
  int	 w, h;
  byte	 r[256], g[256], b[256];
  int	 numcols;
  char	 format_str[80];
  float	 normaspect;
} Pic;

typedef struct line_s {
  double vx;
  double vy;
  double vz;
  double px;
  double py;
  double pz;
} Line;

typedef long double Matrix[4][4];

typedef struct point_s {
  double x;
  double y;
  double z;
} Point;

int LoadGIF(char *, struct pic_s *);
int WriteGIF(char *, struct pic_s *);

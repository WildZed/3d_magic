/******************************************************************************
 * Program to generate 3d stereograms. - Thomas W. J. Lucas. 		      *
 ******************************************************************************/

#include <varargs.h>
/*#include <sys/types.h>*/
#include <time.h>

#include "3d.h"

#define PATTERNWIDTH 6		/* Width of pattern. */
/*#define RNDDIV 21474836		/* For use in generating colour map. */

/* Default settings. */
static double	pixpercm = 39.98;
static double	eyesep	 = 6.5;
static double	eyedst	 = 60.0;
static double	backdst	 = 60.0;
static short int	rf	 = 20;
static int		width	 = 800;
static int		height	 = 600;

/* Boolean switch. */
static int widthused = 0;

/* Global data. (Local to this file). */
static Pic	patpic;
static Pic	sgpic;
static char	sfile[255];
static char	pfile[255];

/* Internal procedure declarations. */
int	GeneratePattern();
void	Usage();
void	ProcessOpt();

/* Load in data and generate 3d stereogram. */
main(argc, argv)
int	  argc;
char	*argv[];
{

  /* Initialization. */
  ProcessOpt(argc, argv);
  if (InitScene(sfile) != 0) {
    fprintf(stderr, "%s: Couldn't initialise the scene.\n", PROGNAME);
    exit(1);
  }

  /* Load in the pattern file or generate one. */
  if (*pfile != '\0') {
    if (LoadGIF(pfile, &patpic) != 0) {
      exit(1);
    }
    height = patpic.h;
    if (!widthused) width = (patpic.h * 5) / 3;
  }
  else {
    InitPic(&patpic);
    /* Set the height and width. */
    patpic.h = height;
    patpic.w = (int) PATTERNWIDTH*pixpercm;
    printf("Generating pattern.\n");
    if (GenerateColMap1(&patpic, rf)) {
      exit(1);
    }
    if (GeneratePattern(&patpic)) {
      exit(1);
    }
  }

  /* Generate the stereogram. */
  printf("Generating the stereogram.\n");
  sgpic.w = width;
  sgpic.h = height;
  if (GenerateStereoGram(&patpic, &sgpic)) {
    exit(1);
  }

  /* Write the stereogram. */
  WriteGIF("out.gif", &sgpic);
}

/* Procedure to process command line options. */
void ProcessOpt(argc, argv)
int argc;
char *argv[];
{
  int i, j, k;

  /* Examine each arg starting at 2, 1 is program name. */
  *sfile = '\0';
  *pfile = '\0';
  for (i = 2; i <= argc; i++) {
    k = i; /* Save position of arg with '-', because i might change. */
    if (argv[k-1][0] == '-') {
      for (j = 1; j < (int) strlen(argv[k-1]); j++) {
	i++;
	switch (argv[k-1][j]) {
	  case 's':
	    if (argc < i) {
	      Usage();
	    }
	    if (sscanf(argv[i-1], "%f", &pixpercm) != 1) {
	      Usage();
	    }
	    break;
	  case 'r':
	    if (argc < i) {
	      Usage();
	    }
	    if (sscanf(argv[i-1], "%hi", &rf) != 1) {
	      Usage();
	    }
	    break;
	  case 'w':
	    if (argc < i) {
	      Usage();
	    }
	    if (sscanf(argv[i-1], "%d", &width) != 1) {
	      Usage();
	    }
	    widthused = 1;
	    break;
	  case 'h':
	    if (argc < i) {
	      Usage();
	    }
	    if (sscanf(argv[i-1], "%d", &height) != 1) {
	      Usage();
	    }
	    break;
	  case 'p':
	    if (argc < i) {
	      Usage();
	    }
	    strcpy(pfile, argv[i-1]);
	    break;
	  default:
	    Usage();
	    break;
	} /* End switch. */
      } /* End for j -... */
    }
    else {
      if (argc < i) {
	Usage();
      }
      if (*sfile == '\0') {
	strcpy(sfile, argv[i-1]);
      }
      else {
	Usage();
      }
    }
  }
}

/* Display usage message and exit. */
void Usage()
{
  fprintf(stderr, "%s: Error in arguments.\n", PROGNAME);
  fprintf(stderr,
	  "Usage: %s [opts] <scene file>\n%s\n%s\n%s\n%s\n%s\n", PROGNAME,
	  "         -r <randomness (0-100, 0 not random, 100 very random)>",
	  "         -s <pixels per cm>",
	  "         -p <pattern file (GIF87)>",
	  "         -w <width (pixels)>",
	  "         -h <height (pixels)>");
  exit(1);
}

/* Generate a pattern for generating a 3d stereogram with. */
int GeneratePattern(pic)
Pic *pic;
{
  int i, w, h, numpix, n;
  double wf, hf;

  numpix = pic->h*pic->w;
  pic->pic = (byte *) malloc(numpix*sizeof(byte));
  if (pic->pic == NULL) {
    fprintf(stderr, "%s: Couldn't malloc space for pattern.\n", PROGNAME);
    return(1);
  }
  wf = 0.0;
  for (h = 0; h < pic->h; h++) {
    hf = 0.0;
    for (w = 0; w < pic->w; w++) {
      n = pic->numcols*((sin(w/14.0 + wf)*sin(w/22.0)
          + sin(h/24.0 + hf) + 2.0)/4.0);
      pic->pic[(h*pic->w)+w] = (n % pic->numcols);
      hf += 0.04;
    }
    wf += 0.04;
  }
  return(0);
}

/* Function to generate a stereogram. */
/* sg should contain width and height of stereogram. */
int GenerateStereoGram(p, sg)
Pic	*p;
Pic	*sg;
{
  int i, w, h, pp, nw, pw, pwch;
  Line l;
  double x, z, nx;
  double cmperpix = 1.0/pixpercm;
  clock_t usecs;

  *(sg->format_str) = '\0';
  sg->normaspect = 0.0;
  sg->numcols = p->numcols;

  /* Copy colourmap. */
  for (i = 0; i< p->numcols; i++) {
    sg->r[i] = p->r[i];
    sg->g[i] = p->g[i];
    sg->b[i] = p->b[i];
  }

  /* Initialise stereogram pic. */
  if ((sg->pic = (byte *) malloc(sg->w*sg->h*sizeof(byte))) == NULL) {
    fprintf(stderr, "%s: Couldn't malloc space for image.\n", PROGNAME);
    return(1);
  }

  /* Right eye position. */
  l.px = sg->w*cmperpix/2.0 + eyesep/2.0;
  l.py = sg->h*cmperpix/2.0;
  l.pz = -eyedst;
  /* Height value of gif goes from top to bottom. */
  /* Initialise direction vector of line from right eye to first pixel. */
  l.vy = sg->h*cmperpix/2.0;
  l.vz = -l.pz; /* 0.0 - l.pz */

  usecs = clock();

  /* Do the real processing. */
  for (h = 0; h < sg->h; h++) {
    pw = 0;
    pwch = 1;

    /* Initialise x part of line vector, set to left of stereogram. */
    l.vx = -l.px; /* 0.0 - l.px */

    for (w = 0; w < sg->w; w++) {
      pp = sg->w*h+w; /* Pixel (w, h) of the stereogram, (right eye). */

      /* Get intersection point of line with scene. */
      /* Know already the x, y given a z value. */
      if (SceneIntersect(&l, &z)) {
	z = backdst;

	/*if (h>120 && h<280 && w>260 && w<460) {
	  z = backdst - (w - 360)*0.04 - (h - 200)*0.08;
	}
	else {
	}*/
      }
      else if (z < 0) {
	z = backdst;
	fprintf(stderr, "%s: Error, negative depth.\n", PROGNAME);
      }

      /* Get x value for z of the line. */
      x = l.px + (l.vx*(z - l.pz))/l.vz;

      /* Find the point of intersection of the line from here to the */
      /* left eye with the stereogram. */
      nx = x + (z*(l.px - eyesep - x))/(z + eyedst);

      /* Assign it to the corresponding value on the stereogram. */
      nw = nx*pixpercm;

      /* Colour it. */
      if (nw < 0) {
	if (pw >= p->w) {
	  pw = 0;
	  pwch = 1;
	}
	else if (pw >= (w-nw)/2) {
	  pwch = -1;
	}
	else if (pw <= 0) {
	  pwch = 1;
	}
	sg->pic[pp] = p->pic[p->w*h+pw];
	pw = pw + pwch;
      }
      else {
	sg->pic[pp] = sg->pic[sg->w*h+nw];
      }

      /* Work out change in x vector in line from right eye to pixel. */
      l.vx = l.vx + cmperpix;
    }

    /* Change height of point on line each step in h. */
    l.vy = l.vy - cmperpix;
  }

  printf("Total time for stereogram generation = %.2f seconds.\n",
	 clock()/1000000.0);

  return(0);
}

/* Generate a colour map. */
int GenerateColMap1(pic, rndfactor)
Pic		*pic;
short int 	 rndfactor;
{
  int i;
  double rndvar = 256*(rndfactor/100.0);

  i = 0;
  srand48(time((long *) &i));
  for (i = 0; i < pic->numcols; i++) {
    pic->r[i] = (int) (i + (drand48()*rndvar)) % 256;
    pic->g[i] = (int) (i + (drand48()*rndvar)) % 256;
    pic->b[i] = (int) (i + (drand48()*rndvar)) % 256;
  }
  return(0);
}

/* Generate a colour map. */
int GenerateColMap2(pic, rndfactor)
Pic		*pic;
short int 	 rndfactor;
{
  int i, cvar[3];
  double rndvar = 256*(rndfactor/100.0);

  i = 0;
  srand48(time((long *) &i));
  cvar[RED] = ((drand48()*6.0) - 3.0);
  cvar[GREEN] = ((drand48()*6.0) - 3.0);
  cvar[BLUE] = ((drand48()*6.0) - 3.4);
  pic->r[0] = 90 + 150.0*drand48();
  pic->g[0] = 0 + 40.0*drand48();
  pic->b[0] = 20 + 80.0*drand48();
  for (i = 1; i < pic->numcols; i++) {
    pic->r[i] = (int) (pic->r[i-1] + (4 + 6*drand48())*cvar[RED]) % 256;
    pic->g[i] = (int) (pic->g[i-1] + (4 + 6*drand48())*cvar[GREEN]) % 256;
    pic->b[i] = (int) (pic->b[i-1] + (4 + 4*drand48())*cvar[BLUE]) % 256;
    if (drand48() > 0.96) {
      cvar[RED] = ((drand48()*6.0) - 3.0);
      cvar[GREEN] = ((drand48()*6.0) - 3.0);
      cvar[BLUE] = ((drand48()*6.0) - 3.4);
      if (drand48() > 0.6)
	pic->r[i] = pic->r[i] + drand48()*rndfactor*2.0 - rndfactor;
      if (drand48() > 0.6)
	pic->g[i] = pic->g[i] + drand48()*rndfactor*2.0 - rndfactor;
      if (drand48() > 0.6)
	pic->b[i] = pic->b[i] + drand48()*rndfactor*2.0 - rndfactor;
    }
  }
  return(0);
}

InitPic(pic)
Pic *pic;
{
  pic->numcols = 256;
  *(pic->format_str) = '\0';
  pic->normaspect = 0.0;
}

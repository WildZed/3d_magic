#include "3d.h"

typedef int boolean;

#define IMAGESEP 0x2c
#define EXTENSION 0x21
#define INTERLACEMASK 0x40
#define COLOURMAPMASK 0x80

FILE *fp;

int BitOffset = 0,		/* Bit Offset of next code. */
    XC = 0, YC = 0,		/* Output X and Y coords of current pixel. */
    Pass = 0,			/* Used by output routine if interlaced pic. */
    OutCount = 0,		/* Decompressor output 'stack count'. */
    RWidth, RHeight,		/* Screen dimensions. */
    Width, Height,		/* Image dimensions. */
    LeftOfs, TopOfs,		/* Image offset. */
    BitsPerPixel,		/* Bits per pixel, read from GIF header. */
    BytesPerScanline,		/* Bytes per scanline in output raster. */
    ColourMapSize,		/* Number of colours. */
    Background,			/* Background colour. */
    CodeSize,			/* Code size, read from GIF header. */
    InitCodeSize,		/* Starting code size, used during Clear. */
    Code,			/* Value returned by ReadCode. */
    MaxCode,			/* Limiting value for current code size. */
    ClearCode,			/* GIF clear code. */
    EOFCode,			/* GIF end-of-information code. */
    CurCode, OldCode, InCode,	/* Decompressor variables. */
    FirstFree,			/* First free code, generated per GIF spec. */
    FreeCode,			/* Decompressor,next free slot in hash table. */
    FinChar,			/* Decompressor variable. */
    BitMask,			/* AND mask for data size. */
    ReadMask,			/* Code AND mask for current code size. */
    Misc;                       /* Miscellaneous bits (interlace,local cmap). */

boolean Interlace, HasColourmap;

byte *RawGIF;			/* The heap array to hold it, raw. */
byte *Raster;			/* The raster data stream, unblocked. */

/* The hash table used by the decompressor. */
int Prefix[4096];
int Suffix[4096];

/* An output array used by the decompressor. */
int OutCode[1025];

int	gif89 = 0;
char	*id87 = "GIF87a";
char	*id89 = "GIF89a";

int	filesize;

static int EGApalette[16][3] = {
  {0,0,0},       {0,0,128},     {0,128,0},     {0,128,128}, 
  {128,0,0},     {128,0,128},   {128,128,0},   {200,200,200},
  {100,100,100}, {100,100,255}, {100,255,100}, {100,255,255},
  {255,100,100}, {255,100,255}, {255,255,100}, {255,255,255}
};

static int  ReadCode();
static void DoInterlace();
static int  GifError();

/******************************************************************************
 * External procedure to Load a GIF image into memory.			      *
 ******************************************************************************/

int LoadGIF(fname, pic)
char	*fname;	/* File name. */
Pic	*pic;  /* The picture. */
{
  register byte ch, ch1;
  register byte *ptr, *ptr1, *picptr;
  register int  i;
  int           npixels, maxpixels, aspect;

  /* Initialize variables. */
  BitOffset = XC = YC = Pass = OutCount = npixels = maxpixels = 0;
  RawGIF = Raster = pic->pic = NULL;
  gif89 = 0;

  fp = fopen(fname,"r");
  if (!fp) {
    fprintf(stderr,"%s: LoadGIF() - Unable to open file '%s'.\n", PROGNAME,
    	    fname);
    return 1;
  }
  
  /* Find the size of the file. */
  fseek(fp, 0L, 2);
  filesize = ftell(fp);
  fseek(fp, 0L, 0);
  
  /* The +256's are so we can read truncated GIF files without fear of 
     segmentation violation. */
  if (!(ptr = RawGIF = (byte *) calloc(filesize+256,1)))
    return( GifError(pic, "Not enough memory to read gif file.") );
  
  if (!(Raster = (byte *) calloc(filesize+256,1)))    
    return( GifError(pic, "Not enough memory to read gif file.") );
  
  if (fread(ptr, filesize, 1, fp) != 1) 
    return( GifError(pic, "GIF data read failed.") );
  
  if      (strncmp((char *) ptr, id87, 6)==0) gif89 = 0;
  else if (strncmp((char *) ptr, id89, 6)==0) gif89 = 1;
  else    return( GifError(pic, "Not a GIF file."));
  
  ptr += 6;
  
  /* Get variables from the GIF screen descriptor. */
  
  ch = (*ptr++);
  RWidth = ch + 0x100 * (*ptr++);	/* Screen dimensions... not used. */
  ch = (*ptr++);
  RHeight = ch + 0x100 * (*ptr++);
  
  ch = (*ptr++);
  HasColourmap = ((ch & COLOURMAPMASK) ? True : False);
  
  BitsPerPixel = (ch & 7) + 1;
  pic->numcols = ColourMapSize = 1 << BitsPerPixel;
  BitMask = ColourMapSize - 1;
  
  Background = (*ptr++);		/* Background colour... not used. */
  
  aspect = (*ptr++);
  if (aspect) {
    if (!gif89) return(GifError(pic, "Corrupt GIF file (screen descriptor)."));
    else pic->normaspect = (float) (aspect + 15) / 64.0;   /* gif89 aspect ratio */
    fprintf(stderr,"GIF89 aspect = %f\n", pic->normaspect);
  }
  
  
  /* Read in global colourmap. */
  if (HasColourmap)
    for (i=0; i<ColourMapSize; i++) {
      pic->r[i] = (*ptr++);
      pic->g[i] = (*ptr++);
      pic->b[i] = (*ptr++);
    }
  else {				/* No colourmap in GIF file. */
    /* Put std EGA palette (repeated 16 times) into colourmap, for lack of
       anything better to do. */
    for (i=0; i<256; i++) {
      pic->r[i] = EGApalette[i&15][0];
      pic->g[i] = EGApalette[i&15][1];
      pic->b[i] = EGApalette[i&15][2];
    }
  }


  while ( (i=(*ptr++)) == EXTENSION) {		/* Parse extension blocks. */
    int i, fn, blocksize, aspnum, aspden;

    /* read extension block */
    fn = (*ptr++);

    if (fn == 'R') {                  /* GIF87 aspect extension. */
      blocksize = (*ptr++);
      if (blocksize == 2) {
	aspnum = (*ptr++);
	aspden = (*ptr++);
	if (aspden>0 && aspnum>0) 
	  pic->normaspect = (float) aspnum / (float) aspden;
	else { pic->normaspect = 1.0;  aspnum = aspden = 1; }

	fprintf(stderr,"GIF87 aspect extension: %d:%d = %f\n", 
	        aspnum, aspden, pic->normaspect);
      }
      else {
	for (i=0; i<blocksize; i++) (*ptr++);
      }
    }

    else if (fn == 0xFE) {  /* Comment Extension.  just eat it. */
      int ch, j, sbsize;

      /* Read (and ignore) data sub-blocks. */
      do {
	j = 0;  sbsize = (*ptr++);
	while (j<sbsize) {
	  ch = (*ptr++);  j++;
	  fprintf(stderr,"%c", ch);
	}
      } while (sbsize);
    }

    else if (fn == 0x01) {  /* PlainText Extension. */
      int j,sbsize,ch;
      int tgLeft, tgTop, tgWidth, tgHeight, cWidth, cHeight, fg, bg;
      
      GifError(pic, "PlainText extension found in GIF file.  Ignored.");

      sbsize   = (*ptr++);
      tgLeft   = (*ptr++);  tgLeft   += ((*ptr++))<<8;
      tgTop    = (*ptr++);  tgTop    += ((*ptr++))<<8;
      tgWidth  = (*ptr++);  tgWidth  += ((*ptr++))<<8;
      tgHeight = (*ptr++);  tgHeight += ((*ptr++))<<8;
      cWidth   = (*ptr++);
      cHeight  = (*ptr++);
      fg       = (*ptr++);
      bg       = (*ptr++);
      i=12;
      for ( ; i<sbsize; i++) (*ptr++);   /* Read rest of first subblock. */
      
      fprintf(stderr,
	   "PlainText: tgrid=%d,%d %dx%d  cell=%dx%d  col=%d,%d\n",
	    tgLeft, tgTop, tgWidth, tgHeight, cWidth, cHeight, fg, bg);

      /* Read (and ignore) data sub-blocks. */
      do {
	j = 0;
	sbsize = (*ptr++);
	while (j<sbsize) {
	  ch = (*ptr++);  j++;
	  fprintf(stderr,"%c", ch);
	}
      } while (sbsize);
    }


    else if (fn == 0xF9) {  /* Graphic Control Extension. */
      int j, sbsize;

      GifError(pic, "Graphic Control Extension in GIF file.  Ignored.");

      /* Read (and ignore) data sub-blocks. */
      do {
	j = 0; sbsize = (*ptr++);
	while (j<sbsize) { (*ptr++);  j++; }
      } while (sbsize);
    }


    else {		/* Unknown extension. */
      int j, sbsize;

      fprintf(stderr, "%s: LoadGIF() - Unknown extension 0x%02x in GIF file.  Ignored.\n", PROGNAME, fn);

      /* Read (and ignore) data sub-blocks. */
      do {
	j = 0; sbsize = (*ptr++);
	while (j<sbsize) { (*ptr++);  j++; }
      } while (sbsize);
    }
  }


  /* Check for image seperator. */
  if (i != IMAGESEP) 
    return( GifError(pic, "Corrupt GIF file (no image separator).") );
  
  /* Now read in values from the image descriptor. */
  
  ch = (*ptr++);
  LeftOfs = ch + 0x100 * (*ptr++);
  ch = (*ptr++);
  TopOfs = ch + 0x100 * (*ptr++);
  ch = (*ptr++);
  Width = ch + 0x100 * (*ptr++);
  ch = (*ptr++);
  Height = ch + 0x100 * (*ptr++);

  Misc = (*ptr++);
  Interlace = ((Misc & INTERLACEMASK) ? True : False);

  if (Misc & 0x80) {
    for (i=0; i< 1 << ((Misc&7)+1); i++) {
      pic->r[i] = (*ptr++);
      pic->g[i] = (*ptr++);
      pic->b[i] = (*ptr++);
    }
  }


  if (!HasColourmap && !(Misc&0x80)) {
    /* No global or local colourmap. */
    GifError(pic, "No colourmap in this GIF file.  Assuming EGA colours.");
  }
    

  
  /* Start reading the raster data. First we get the intial code size
   * and compute decompressor constant values, based on this code size.
   */
  
  CodeSize = (*ptr++);
  ClearCode = (1 << CodeSize);
  EOFCode = ClearCode + 1;
  FreeCode = FirstFree = ClearCode + 2;
  
  /* The GIF spec has it that the code size is the code size used to
   * compute the above values is the code size given in the file, but the
   * code size used in compression/decompression is the code size given in
   * the file plus one. (thus the ++).
   */
  
  CodeSize++;
  InitCodeSize = CodeSize;
  MaxCode = (1 << CodeSize);
  ReadMask = MaxCode - 1;

  /* UNBLOCK:
   * Read the raster data.  Here we just transpose it from the GIF array
   * to the Raster array, turning it from a series of blocks into one long
   * data stream, which makes life much easier for ReadCode().
   */
  
  ptr1 = Raster;
  do {
    ch = ch1 = (*ptr++);
    while (ch--) { *ptr1 = (*ptr++); ptr1++; }
    if ((ptr - RawGIF) > filesize) {
      GifError(pic, "This GIF file seems to be truncated.  Winging it.");
      break;
    }
  } while(ch1);
  free(RawGIF);	 RawGIF = NULL; 	/* We're done with the raw data now. */
  
  fprintf(stderr,"%s: LoadGIF() - Picture is %dx%d, %d bits, %sinterlaced.\n",
	  PROGNAME, Width, Height, BitsPerPixel, Interlace ? "" : "non-");
  
  fprintf(stderr, "GIF%s, %d bits per pixel, %sinterlaced.  (%d bytes)\n",
	  (gif89) ? "89" : "87", BitsPerPixel, 
	  Interlace ? "" : "non-", filesize);

  /* Allocate the 'pic'. */
  pic->w = Width;  pic->h = Height;
  maxpixels = Width*Height;
  picptr = pic->pic = (byte *) malloc(maxpixels);
  sprintf(pic->format_str, "%dx%d GIF%s.", pic->w, pic->h, (gif89) ? "89" : "87");

  if (!pic->pic) 
    return( GifError(pic, "Not enough memory for 'pic'.") );
  
  /* Decompress the file, continuing until you see the GIF EOF code.
   * One obvious enhancement is to add checking for corrupt files here.
   */
  
  Code = ReadCode();
  while (Code != EOFCode) {
    /* Clear code sets everything back to its initial value, then reads the
     * immediately subsequent code as uncompressed data.
     */

    if (Code == ClearCode) {
      CodeSize = InitCodeSize;
      MaxCode = (1 << CodeSize);
      ReadMask = MaxCode - 1;
      FreeCode = FirstFree;
      Code = ReadCode();
      CurCode = OldCode = Code;
      FinChar = CurCode & BitMask;
      if (!Interlace) *picptr++ = FinChar;
         else DoInterlace(FinChar, pic);
      npixels++;
    }
    else {
      /* If not a clear code, must be data: save same as CurCode and InCode. */

      /* If we're at maxcode and didn't get a clear, stop loading. */
      if (FreeCode>=4096) { /* printf("freecode blew up\n"); */
	break;
      }

      CurCode = InCode = Code;
      
      /* If greater or equal to FreeCode, not in the hash table yet;
       * repeat the last character decoded.
       */
      
      if (CurCode >= FreeCode) {
	CurCode = OldCode;
	if (OutCount > 1024) {  /* printf("outcount1 blew up\n"); */
	  break;
	}
	OutCode[OutCount++] = FinChar;
      }
      
      /* Unless this code is raw data, pursue the chain pointed to by CurCode
       * through the hash table to its end; each code in the chain puts its
       * associated output code on the output queue.
       */
      
      while (CurCode > BitMask) {
	if (OutCount > 1024) break;   		/* Corrupt file. */
	OutCode[OutCount++] = Suffix[CurCode];
	CurCode = Prefix[CurCode];
      }
      
      if (OutCount > 1024) { /* printf("outcount blew up\n"); */
	break;
      }
      
      /* The last code in the chain is treated as raw data. */
      
      FinChar = CurCode & BitMask;
      OutCode[OutCount++] = FinChar;
      
      /* Now we put the data out to the Output routine.
       * It's been stacked LIFO, so deal with it that way...
       */

      /* safety thing:  prevent exceeding range of 'pic' */
      if (npixels + OutCount > maxpixels) OutCount = maxpixels-npixels;
	
      npixels += OutCount;
      if (!Interlace) for (i=OutCount-1; i>=0; i--) *picptr++ = OutCode[i];
                else  for (i=OutCount-1; i>=0; i--) DoInterlace(OutCode[i], pic);
      OutCount = 0;

      /* Build the hash table on-the-fly. No table is stored in the file. */
      
      Prefix[FreeCode] = OldCode;
      Suffix[FreeCode] = FinChar;
      OldCode = InCode;
      
      /* Point to the next slot in the table.  If we exceed the current
       * MaxCode value, increment the code size unless it's already 12.  If it
       * is, do nothing: the next code decompressed better be CLEAR.
       */
      
      FreeCode++;
      if (FreeCode >= MaxCode) {
	if (CodeSize < 12) {
	  CodeSize++;
	  MaxCode *= 2;
	  ReadMask = (1 << CodeSize) - 1;
	}
      }
    }
    Code = ReadCode();
    if (npixels >= maxpixels) break;
  }
  free(Raster);  Raster = NULL;
  
  if (npixels != maxpixels) {
    GifError(pic, "This GIF file seems to be truncated.  Winging it.");
    memset(pic->pic+npixels, 0, maxpixels-npixels);  /* Clear to EOBuffer. */
  }

  if (fp != stdin) fclose(fp);

  return 0;
}


/* Fetch the next code from the raster data stream.  The codes can be
 * any length from 3 to 12 bits, packed into 8-bit bytes, so we have to
 * maintain our location in the Raster array as a BIT Offset.  We compute
 * the byte Offset into the raster array by dividing this by 8, pick up
 * three bytes, compute the bit Offset into our 24-bit chunk, shift to
 * bring the desired code to the bottom, then mask it off and return it. 
 */

static int ReadCode()
{
  int RawCode, ByteOffset;
  
  ByteOffset = BitOffset / 8;
  RawCode = Raster[ByteOffset] + (Raster[ByteOffset + 1] << 8);
  if (CodeSize >= 8)
    RawCode += ( ((int) Raster[ByteOffset + 2]) << 16);
  RawCode >>= (BitOffset % 8);
  BitOffset += CodeSize;

  return(RawCode & ReadMask);
}


/***************************/
static void DoInterlace(Index, pic)
byte	 Index;
Pic	*pic;
{
  static byte *ptr = NULL;
  static int   oldYC = -1;
  
  if (oldYC != YC) {  ptr = pic->pic + YC * Width;  oldYC = YC; }
  
  if (YC<Height)
    *ptr++ = Index;
  
  /* Update the X-coordinate, and if it overflows, update the Y-coordinate. */
  
  if (++XC == Width) {
    
    /* Deal with the interlace as described in the GIF
     * spec.  Put the decoded scan line out to the screen if we haven't gone
     * past the bottom of it.
     */
    
    XC = 0;
    
    switch (Pass) {
    case 0:
      YC += 8;
      if (YC >= Height) { Pass++; YC = 4; }
      break;
      
    case 1:
      YC += 8;
      if (YC >= Height) { Pass++; YC = 2; }
      break;
      
    case 2:
      YC += 4;
      if (YC >= Height) { Pass++; YC = 1; }
      break;
      
    case 3:
      YC += 2;  break;
      
    default:
      break;
    }
  }
}

/*****************************/
static int GifError(pic, st)
char	*st;
Pic	*pic;
{
  fprintf(stderr,"%s: LoadGIF() - %s\n",PROGNAME,st);
  
  if (RawGIF != NULL) free(RawGIF);
  if (Raster != NULL) free(Raster);
  if (pic    != NULL) free(pic);
  
  return -1;
}





/******************************************************************************
 * GIF write routines. 							      *
 ******************************************************************************/

typedef long int        count_int;

static int  Width, Height;
static int  curx, cury;
static long CountDown;
static int  Interlace;
static byte bw[2] = {0, 0xff};

#ifdef __STDC__
static void putword(int, FILE *);
static void compress(int, FILE *, Pic *); /*byte *, int);*/
static void output(int, Pic *);
static void cl_block(Pic *);
static void cl_hash(count_int);
static void char_init(void);
static void char_out(int);
static void flush_char(void);
#else
static void putword(), compress(), output(), cl_block(), cl_hash();
static void char_init(), char_out(), flush_char();
#endif

static byte pc2nc[256], r1[256], g1[256], b1[256];


/*************************************************************/
int WriteGIF(filename, pic) /*wpic, ww, wh, rmap, gmap, bmap, numcols)*/
char	*filename;	/* File name of file to write to. */
Pic	*pic;		/* The picture. */
{
  int RWidth, RHeight;
  int LeftOfs, TopOfs;
  int Resolution, ColourMapSize, InitCodeSize, Background, BitsPerPixel;
  int i,j,nc;

  /* Open the file for writing. */
  fp = fopen(filename,"w");
  if (!fp) {
    fprintf(stderr,"%s: WriteGIF() - Unable to open file '%s'.\n", PROGNAME,
    	    filename);
    return 1;
  }

  Interlace = 0;
  Background = 0;


  for (i=0; i<256; i++) { pc2nc[i] = r1[i] = g1[i] = b1[i] = 0; }

  /* Compute number of unique colours. */
  nc = 0;

  for (i=0; i<pic->numcols; i++) {
    /* See if colour #i is already used. */
    for (j=0; j<i; j++) {
      if (pic->r[i] == pic->r[j] && pic->g[i] == pic->g[j] && 
	  pic->b[i] == pic->b[j]) break;
    }

    if (j==i) {  /* Wasn't found. */
      pc2nc[i] = nc;
      r1[nc] = pic->r[i];
      g1[nc] = pic->g[i];
      b1[nc] = pic->b[i];
      nc++;
    }
    else pc2nc[i] = pc2nc[j];
  }


  /* Figure out 'BitsPerPixel'. */
  for (i=1; i<8; i++)
    if ( (1<<i) >= nc) break;
  
  BitsPerPixel = i;

  ColourMapSize = 1 << BitsPerPixel;
	
  RWidth  = Width  = pic->w;
  RHeight = Height = pic->h;
  LeftOfs = TopOfs = 0;
	
  Resolution = BitsPerPixel;

  CountDown = pic->w * pic->h;    /* # of pixels we'll be doing. */

  if (BitsPerPixel <= 1) InitCodeSize = 2;
                    else InitCodeSize = BitsPerPixel;

  curx = cury = 0;

  if (!fp) {
    fprintf(stderr,  "WriteGIF: file not open for writing\n" );
    return (1);
  }

  fprintf(stderr,"WriteGIF: pic=%lx, w,h=%dx%d, numcols=%d, Bits%d,Cmap=%d\n",
	    pic->pic, pic->w, pic->h, pic->numcols,BitsPerPixel,ColourMapSize);

  fwrite("GIF87a", 1, 6, fp);    /* The GIF magic number. */

  putword(RWidth, fp);           /* Screen descriptor. */
  putword(RHeight, fp);

  i = 0x80;	                /* Yes, there is a colour map. */
  i |= (8-1)<<4;                /* OR in the colour resolution (hardwired 8). */
  i |= (BitsPerPixel - 1);      /* OR in the # of bits per pixel. */
  fputc(i,fp);          

  fputc(Background, fp);         /* Background colour. */

  fputc(0, fp);                  /* Future expansion byte. */


  /*if (colourstyle == 1) {         /* Greyscale. */
  /*  for (i=0; i<ColourMapSize; i++) {
      j = MONO(r1[i], g1[i], b1[i]);
      fputc(j, fp);
      fputc(j, fp);
      fputc(j, fp);
    }
  }
  else {*/
    for (i=0; i<ColourMapSize; i++) {       /* Write out Global colourmap. */
      fputc(r1[i], fp);
      fputc(g1[i], fp);
      fputc(b1[i], fp);
    }
  /*}*/

  fputc( ',', fp );              /* Image separator. */

  /* Write the Image header. */
  putword(LeftOfs, fp);
  putword(TopOfs,  fp);
  putword(Width,   fp);
  putword(Height,  fp);
  if (Interlace) fputc(0x40, fp);   /* Use Global Colourmap, maybe Interlace. */
            else fputc(0x00, fp);

  fputc(InitCodeSize, fp);
  compress(InitCodeSize+1, fp, pic); /*pic->pic, pic->w*pic->h);*/

  fputc(0,fp);                      /* Write out a Zero-length packet (EOF). */
  fputc(';',fp);                    /* Write GIF file terminator. */

  if (fp != stdin) fclose(fp);

  return (0);
}




/******************************/
static void putword(ww, fp)
int ww;
FILE *fp;
{
  /* Writes a 16-bit integer in GIF order (LSB first). */
  fputc(ww & 0xff, fp);
  fputc((ww>>8)&0xff, fp);
}




/***********************************************************************/


static unsigned long cur_accum = 0;
static int           cur_bits = 0;




#define min(a,b)        ((a>b) ? b : a)

#define BITS	12
#define MSDOS	1

#define HSIZE  5003            /* 80% occupancy. */

typedef unsigned char   char_type;


static int n_bits;                   /* Number of bits/code. */
static int maxbits = BITS;           /* User settable max # bits/code. */
static int maxcode;                  /* Maximum code, given n_bits. */
static int maxmaxcode = 1 << BITS;   /* NEVER generate this. */

#define MAXCODE(n_bits)     ( (1 << (n_bits)) - 1)

static  count_int      htab [HSIZE];
static  unsigned short codetab [HSIZE];
#define HashTabOf(i)   htab[i]
#define CodeTabOf(i)   codetab[i]

static int hsize = HSIZE;            /* For dynamic table sizing. */

/*
 * To save much memory, we overlay the table used by compress() with those
 * used by decompress().  The tab_prefix table is the same size and type
 * as the codetab.  The tab_suffix table needs 2**BITS characters.  We
 * get this from the beginning of htab.  The output stack uses the rest
 * of htab, and contains characters.  There is plenty of room for any
 * possible stack (stack used to be 8000 characters).
 */

#define tab_prefixof(i) CodeTabOf(i)
#define tab_suffixof(i)        ((char_type *)(htab))[i]
#define de_stack               ((char_type *)&tab_suffixof(1<<BITS))

static int free_ent = 0;                  /* First unused entry. */

/*
 * Block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */
static int clear_flg = 0;

static long int in_count = 1;           /* Length of input. */
static long int out_count = 0;          /* # of codes output (for debugging). */

/*
 * Compress stdin to stdout.
 *
 * Algorithm:  use open addressing double hashing (no chaining) on the 
 * prefix code / next character combination.  We do a variant of Knuth's
 * algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 * secondary probe.  Here, the modular division first probe is gives way
 * to a faster exclusive-or manipulation.  Also do block compression with
 * an adaptive reset, whereby the code table is cleared when the compression
 * ratio decreases, but after the table fills.  The variable-length output
 * codes are re-sized at this point, and a special CLEAR code is generated
 * for the decompressor.  Late addition:  construct the table according to
 * file size for noticeable speed improvement on small files.  Please direct
 * questions about this implementation to ames!jaw.
 */

static int g_init_bits;
static FILE *g_outfile;

static int ClearCode;
static int EOFCode;


/********************************************************/
static void compress(init_bits, outfile, pic) /*data, len, pic)*/
int	 init_bits;
FILE	*outfile;
Pic	*pic;
{
  register long fcode;
  register int i = 0;
  register int c;
  register int ent;
  register int disp;
  register int hsize_reg;
  register int hshift;
  int len = pic->w*pic->h;
  byte *data = pic->pic;

  /*
   * Set up the globals:  g_init_bits - initial number of bits.
   *                      g_outfile   - pointer to output file.
   */
  g_init_bits = init_bits;
  g_outfile   = outfile;

  /* Initialize 'compress' globals. */
  maxbits = BITS;
  maxmaxcode = 1<<BITS;
  memset((char *) htab, 0, sizeof(htab));
  memset((char *) codetab, 0, sizeof(codetab));
  hsize = HSIZE;
  free_ent = 0;
  clear_flg = 0;
  in_count = 1;
  out_count = 0;
  cur_accum = 0;
  cur_bits = 0;


  /*
   * Set up the necessary values.
   */
  out_count = 0;
  clear_flg = 0;
  in_count = 1;
  maxcode = MAXCODE(n_bits = g_init_bits);

  ClearCode = (1 << (init_bits - 1));
  EOFCode = ClearCode + 1;
  free_ent = ClearCode + 2;

  char_init();
  ent = pc2nc[*data++];  len--;

  hshift = 0;
  for ( fcode = (long) hsize;  fcode < 65536L; fcode *= 2L )
    hshift++;
  hshift = 8 - hshift;                /* Set hash code range bound. */

  hsize_reg = hsize;
  cl_hash( (count_int) hsize_reg);            /* Clear hash table. */

  output(ClearCode, pic);
    
  while (len) {
    c = pc2nc[*data++];  len--;
    in_count++;

    fcode = (long) ( ( (long) c << maxbits) + ent);
    i = (((int) c << hshift) ^ ent);    /* Xor hashing. */

    if ( HashTabOf (i) == fcode ) {
      ent = CodeTabOf (i);
      continue;
    }

    else if ( (long)HashTabOf (i) < 0 )      /* Empty slot. */
      goto nomatch;

    disp = hsize_reg - i;           /* Secondary hash (after G. Knott). */
    if ( i == 0 )
      disp = 1;

probe:
    if ( (i -= disp) < 0 )
      i += hsize_reg;

    if ( HashTabOf (i) == fcode ) {
      ent = CodeTabOf (i);
      continue;
    }

    if ( (long)HashTabOf (i) > 0 ) 
      goto probe;

nomatch:
    output(ent, pic);
    out_count++;
    ent = c;

    if ( free_ent < maxmaxcode ) {
      CodeTabOf (i) = free_ent++; /* Code -> hashtable. */
      HashTabOf (i) = fcode;
    }
    else
      cl_block(pic);
  }

  /* Put out the final code. */
  output(ent, pic);
  out_count++;
  output(EOFCode, pic);
}


/*****************************************************************
 * TAG( output )
 *
 * Output the given code.
 * Inputs:
 *      code:   A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *              that n_bits =< (long)wordsize - 1.
 * Outputs:
 *      Outputs code to the file.
 * Assumptions:
 *      Chars are 8 bits long.
 * Algorithm:
 *      Maintain a BITS character long buffer (so that 8 codes will
 * fit in it exactly).  Use the VAX insv instruction to insert each
 * code in turn.  When the buffer fills up empty it and start over.
 */

static
unsigned long masks[] = { 0x0000, 0x0001, 0x0003, 0x0007, 0x000F,
                                  0x001F, 0x003F, 0x007F, 0x00FF,
                                  0x01FF, 0x03FF, 0x07FF, 0x0FFF,
                                  0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF };

static void output(code, pic)
int	 code;
Pic	*pic;
{
  cur_accum &= masks[cur_bits];

  if (cur_bits > 0)
    cur_accum |= ((long)code << cur_bits);
  else
    cur_accum = code;
	
  cur_bits += n_bits;

  while( cur_bits >= 8 ) {
    char_out( (unsigned int) (cur_accum & 0xff) );
    cur_accum >>= 8;
    cur_bits -= 8;
  }

  /*
   * If the next entry is going to be too big for the code size,
   * then increase it, if possible.
   */

  if (free_ent > maxcode || clear_flg) {

    if( clear_flg ) {
      maxcode = MAXCODE (n_bits = g_init_bits);
      clear_flg = 0;
    }
    else {
      n_bits++;
      if ( n_bits == maxbits )
	maxcode = maxmaxcode;
      else
	maxcode = MAXCODE(n_bits);
    }
  }
	
  if( code == EOFCode ) {
    /* At EOF, write the rest of the buffer. */
    while( cur_bits > 0 ) {
      char_out( (unsigned int)(cur_accum & 0xff) );
      cur_accum >>= 8;
      cur_bits -= 8;
    }

    flush_char();
	
    fflush( g_outfile );

    if( ferror( g_outfile ) )
      GifError(pic, "Unable to write GIF file.");
  }
}


/********************************/
static void cl_block(pic)             /* Table clear for block compress. */
Pic	*pic;
{
  /* Clear out the hash table. */

  cl_hash ( (count_int) hsize );
  free_ent = ClearCode + 2;
  clear_flg = 1;

  output(ClearCode, pic);
}


/********************************/
static void cl_hash(hsize)          /* Reset code table. */
register count_int hsize;
{
  register count_int *htab_p = htab+hsize;
  register long i;
  register long m1 = -1;

  i = hsize - 16;
  do {                            /* Might use Sys V memset(3) here. */
    *(htab_p-16) = m1;
    *(htab_p-15) = m1;
    *(htab_p-14) = m1;
    *(htab_p-13) = m1;
    *(htab_p-12) = m1;
    *(htab_p-11) = m1;
    *(htab_p-10) = m1;
    *(htab_p-9) = m1;
    *(htab_p-8) = m1;
    *(htab_p-7) = m1;
    *(htab_p-6) = m1;
    *(htab_p-5) = m1;
    *(htab_p-4) = m1;
    *(htab_p-3) = m1;
    *(htab_p-2) = m1;
    *(htab_p-1) = m1;
    htab_p -= 16;
  } while ((i -= 16) >= 0);

  for ( i += 16; i > 0; i-- )
    *--htab_p = m1;
}


/******************************************************************************
 *
 * GIF Specific routines.
 *
 ******************************************************************************/

/*
 * Number of characters so far in this 'packet'.
 */
static int a_count;

/*
 * Set up the 'byte output' routine.
 */
static void char_init()
{
	a_count = 0;
}

/*
 * Define the storage for the packet accumulator.
 */
static char accum[ 256 ];

/*
 * Add a character to the end of the current packet, and if it is 254
 * characters, flush the packet to disk.
 */
static void char_out(c)
int c;
{
  accum[ a_count++ ] = c;
  if( a_count >= 254 ) 
    flush_char();
}

/*
 * Flush the packet to disk, and reset the accumulator.
 */
static void flush_char()
{
  if( a_count > 0 ) {
    fputc( a_count, g_outfile );
    fwrite( accum, 1, a_count, g_outfile );
    a_count = 0;
  }
}	

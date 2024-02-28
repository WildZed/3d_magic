/****************************************************************************** 
 * Procedures to do with the scene which the 3d stereogram is generated from. *
 ******************************************************************************/

#include <stdio.h>

#include "3d.h"

struct scene_s {
  int ns;
} scene;

/* Procedure to do all necessary initialisation of the scene. */
int InitScene(file)
char *file;
{
/*  Line l;
  Matrix mat;

  l.px = 0.0;
  l.py = 0.0;
  l.pz = 0.0;
  l.vx = 0.0;
  l.vy = 0.0;
  l.vz = 1.0;
  LineToYAxisTrans(&l, mat);
  printf("%f %f %f %f\n", mat[0][0], mat[0][1], mat[0][2], mat[0][3]);
  printf("%f %f %f %f\n", mat[1][0], mat[1][1], mat[1][2], mat[1][3]);
  printf("%f %f %f %f\n", mat[2][0], mat[2][1], mat[2][2], mat[2][3]);
  printf("%f %f %f %f\n", mat[3][0], mat[3][1], mat[3][2], mat[3][3]);*/
  return(0);
}

/* Fills in the value of the intersect of a line with the scene or 
   returns non zero value. */
int SceneIntersect(l, z)
Line		*l;
double		*z;
{
  Matrix mat;
  double sqrtval1, sqrtval2, r, ny;
  Point p, q1, q2;

  LineToYAxisTrans(l, mat);

  /* Circle. */
  r = 10.0;
  p.x = 12.0;
  p.y = 6.0;
  p.z = 60.0;
  TransformPoint(&p, mat, &q1);
  sqrtval1 = r*r - q1.x*q1.x - q1.z*q1.z;
  if (sqrtval1 < 0.0) return(1);
  ny = q1.y - sqrt(sqrtval1);

  /* Circles. */
  /*r = 8.0;
  p.x = 8.0;
  p.y = 6.0;
  p.z = 60.0;
  TransformPoint(&p, mat, &q1);
  p.x = p.x + 10.0;
  TransformPoint(&p, mat, &q2);
  sqrtval1 = r*r - q1.x*q1.x - q1.z*q1.z;
  sqrtval2 = r*r - q2.x*q2.x - q2.z*q2.z;
  if (sqrtval1 < 0.0 && sqrtval2 < 0.0) return(1);
  if (sqrtval1 >= sqrtval2) {
    ny = q1.y - sqrt(sqrtval1);
  }
  else {
    ny = q2.y - sqrt(sqrtval2);
  }*/

  /* Weird shape. */
  /*p.x = 10.0;
  p.y = 6.0;
  p.z = 46.0;
  TransformPoint(&p, mat, &q1);
  if (q1.x > 8.0 || q1.x < -8.0 || q1.z > 6.0 || q1.z < -6.0) {
    return(1);
  }
  ny = 60.0 + p.z + q1.x;*/

  /* Rectangle. */
  /*p.x = 10.0;
  p.y = 6.0;
  p.z = 46.0;
  TransformPoint(&p, mat, &q1);
  if (q1.x > 16.0 || q1.x < -16.0 || q1.z > 6.0 || q1.z < -6.0) {
    return(1);
  }
  ny = 110.0; /* - l->vx/3.0;*/

  /* Work out z distance of intersect. */
  *z = l->pz + ny*l->vz/sqrt(l->vx*l->vx + l->vy*l->vy + l->vz*l->vz);
  return(0);
}

LineToYAxisTrans(l, mat)
Line *l;
Matrix mat;
{
  double d, e, f;

  /*printf("%e\n", l->vx*l->vx + l->vy*l->vy + l->vz*l->vz);*/
  d = sqrt(l->vx*l->vx + l->vy*l->vy + l->vz*l->vz);
  /*printf("%e\n", l->vx*l->vx + l->vy*l->vy);*/
  e = sqrt(l->vx*l->vx + l->vy*l->vy);
  f = e*d;

  if (f == 0.0) {
    mat[0][0] = 0.0;
    mat[0][2] = 0.0;
    mat[1][0] = 0.0;
    mat[1][2] = 0.0;
    mat[3][0] = -l->px;
    mat[3][1] = -l->py;
    mat[3][2] = -l->pz;
  }
  else {
    mat[0][0] = l->vy/e;
    mat[0][2] = -l->vx*l->vz/f;
    mat[1][0] = -l->vx/e;
    mat[1][2] = -l->vy*l->vz/f;
    mat[3][0] = (l->py*l->vx - l->px*l->vy)/e;
    mat[3][1] = -(l->px*l->vx + l->py*l->vy + l->pz*l->vz)/d;
    mat[3][2] = (l->px*l->vx*l->vz + l->py*l->vy*l->vz - l->pz*e*e)/f;
  }
  mat[0][1] = l->vx/d;
  mat[0][3] = 0.0;
  mat[1][1] = l->vy/d;
  mat[1][3] = 0.0;
  mat[2][0] = 0.0;
  mat[2][1] = l->vz/d;
  mat[2][2] = e/d;
  mat[2][3] = 0.0;
  mat[3][3] = 1.0;
}

TransformPoint(p, mat, q)
Point *p, *q; Matrix mat;
{
  q->x = p->x*mat[0][0] + p->y*mat[1][0] + p->z*mat[2][0] + mat[3][0];
  q->y = p->x*mat[0][1] + p->y*mat[1][1] + p->z*mat[2][1] + mat[3][1];
  q->z = p->x*mat[0][2] + p->y*mat[1][2] + p->z*mat[2][2] + mat[3][2];
}

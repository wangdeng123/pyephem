#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "astro.h"
#include "preferences.h"

/* sprint the variable a in sexagesimal format into out[].
 * w is the number of spaces for the whole part.
 * fracbase is the number of pieces a whole is to broken into; valid options:
 *	360000:	<w>:mm:ss.ss
 *	36000:	<w>:mm:ss.s
 *	3600:	<w>:mm:ss
 *	600:	<w>:mm.m
 *	60:	<w>:mm
 * return number of characters written to out, not counting final '\0'.
 */
int
fs_sexa (char *out, double a, int w, int fracbase)
{
	char *out0 = out;
	unsigned long n;
	int d;
	int f;
	int m;
	int s;
	int isneg;

	/* save whether it's negative but do all the rest with a positive */
	isneg = (a < 0);
	if (isneg)
	    a = -a;

	/* convert to an integral number of whole portions */
	n = (unsigned long)(a * fracbase + 0.5);
	d = n/fracbase;
	f = n%fracbase;

	/* form the whole part; "negative 0" is a special case */
	if (isneg && d == 0)
	    out += sprintf (out, "%*s-0", w-2, "");
	else
	    out += sprintf (out, "%*d", w, isneg ? -d : d);

	/* do the rest */
	switch (fracbase) {
	case 60:	/* dd:mm */
	    m = f/(fracbase/60);
	    out += sprintf (out, ":%02d", m);
	    break;
	case 600:	/* dd:mm.m */
	    out += sprintf (out, ":%02d.%1d", f/10, f%10);
	    break;
	case 3600:	/* dd:mm:ss */
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    out += sprintf (out, ":%02d:%02d", m, s);
	    break;
	case 36000:	/* dd:mm:ss.s*/
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    out += sprintf (out, ":%02d:%02d.%1d", m, s/10, s%10);
	    break;
	case 360000:	/* dd:mm:ss.ss */
	    m = f/(fracbase/60);
	    s = f%(fracbase/60);
	    out += sprintf (out, ":%02d:%02d.%02d", m, s/100, s%100);
	    break;
	default:
	    printf ("fs_sexa: unknown fracbase: %d\n", fracbase);
	    abort();
	}

	return (out - out0);
}

/* put the given modified Julian date, jd, in out[] according to preference
 * format.
 * return number of characters written to out, not counting final '\0'.
 */
int
fs_date (char out[], double jd)
{
	int p = pref_get (PREF_DATE_FORMAT);
	char *out0 = out;
	int m, y;
	double d;

	mjd_cal (jd, &m, &d, &y);
	/* beware of %g rounding day up */
	if ((d < 1.0 && d - floor(d) >= .9999995)
				    || (d < 10.0 && d - floor(d) >= .999995)
				    || (d >= 10.0 && d - floor(d) >= .99995))
	    mjd_cal (mjd_day(jd+0.5), &m, &d, &y);

	switch (p) {
	case PREF_YMD:
	    out += sprintf (out, "%4d/%02d/%02.6g", y, m, d);
	    break;
	case PREF_DMY:
	    out += sprintf (out, "%2.6g/%02d/%-4d", d, m, y);
	    break;
	case PREF_MDY:
	    out += sprintf (out, "%2d/%02.6g/%-4d", m, d, y);
	    break;
	default:
	    printf ("fs_date: bad date pref: %d\n", p);
	    abort();
	}

	return (out - out0);
}


/* convert sexagesimal string str AxBxC to double.
 *   x can be anything non-numeric. Any missing A, B or C will be assumed 0.
 *   optional - and + can be anywhere.
 * return 0 if ok, -1 if can't find a thing.
 */
int
f_scansexa (
const char *str0,	/* input string */
double *dp)		/* cracked value, if return 0 */
{
	double a = 0, b = 0, c = 0;
	char str[128];
	char *neg;
	int r;

	/* copy str0 so we can play with it */
	strncpy (str, str0, sizeof(str)-1);
	str[sizeof(str)-1] = '\0';

	neg = strchr(str, '-');
	if (neg)
	    *neg = ' ';
	r = sscanf (str, "%lf%*[^0-9]%lf%*[^0-9]%lf", &a, &b, &c);
	if (r < 1)
	    return (-1);
	*dp = a + b/60 + c/3600;
	if (neg)
	    *dp *= -1;
	return (0);
}

/* crack a floating date string, bp, of the form X/Y/Z determined by the
 *   PREF_DATE_FORMAT preference into its components. allow the day to be a
 *   floating point number,
 * actually the slashes may be anything but digits.
 * a lone component with a decimal point is considered a year.
 */
void
f_sscandate (
char *bp,
int pref,       /* one of PREF_X for PREF_DATE_FORMAT */
int *m,
double *d,
int *y)
{
        double c[3]; /* the three components */
	int n;

	memset (c, 0, sizeof(c));
	n = sscanf (bp, "%lf%*[^0-9]%lf%*[^0-9]%lf", &c[0], &c[1], &c[2]);
	if (n == 1 && (strchr(bp, '.')
			|| (pref == PREF_MDY && (c[0] < 1 || c[0] > 12))
			|| (pref == PREF_DMY && (c[0] < 1 || c[0] > 31)))) {
	    double Mjd;
	    year_mjd (c[0], &Mjd);
	    mjd_cal (Mjd, m, d, y);
	} else {
	    switch (pref) {
	    case PREF_MDY:
		if (n > 0 && c[0] != 0)
		    *m = (int)c[0];
		if (n > 1 && c[1] != 0)
		    *d = c[1];
		if (n > 2 && c[2] != 0)
		    *y = (int)c[2];
		break;
	    case PREF_YMD:
		if (n > 0 && c[0] != 0)
		    *y = (int)c[0];
		if (n > 1 && c[1] != 0)
		    *m = (int)c[1];
		if (n > 2 && c[2] != 0)
		    *d = c[2];
		break;
	    case PREF_DMY:
		if (n > 1 && c[0] != 0)
		    *d = c[0];
		if (n > 2 && c[1] != 0)
		    *m = (int)c[1];
		if (n > 3 && c[2] != 0)
		    *y = (int)c[2];
		break;
	    }
	}
}

/* For RCS Only -- Do Not Edit */
static char *rcsid[2] = {(char *)rcsid, "@(#) $RCSfile: formats.c,v $ $Date: 2004/05/05 17:45:49 $ $Revision: 1.13 $ $Name:  $"};

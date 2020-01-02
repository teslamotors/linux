/*
   1920x1200.S: EDID data set for standard 1920x1200 60 Hz monitor

   Copyright (C) 2012 Carsten Emde <C.Emde@osadl.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.
*/

/* non EDID stuff - copied from xrandr --verbose on the rig*/
#define HDISP 1920
#define HSYNCSTART 1944
#define HSYNCEND 1984
#define HTOTAL 2048
#define VDISP 720
#define VSYNCSTART 724
#define VSYNCEND 730
#define VTOTAL 736

/* EDID */
#define VERSION 1
#define REVISION 4

/* Display */
#define CLOCK 89450 /* kHz */
#define XPIX HDISP
#define YPIX VDISP
#define XY_RATIO XY_RATIO_16_10 /* The screen is actually much larger than that, but this is the widest ratio we have */
#define XBLANK (HTOTAL-HDISP)
#define YBLANK (VTOTAL-VDISP)
#define XOFFSET (HSYNCSTART-HDISP)
#define XPULSE (HSYNCEND-HSYNCSTART)
#define YOFFSET (63+(VSYNCSTART-VDISP))
#define YPULSE (63+(VSYNCEND-VSYNCSTART))
#define DPI 131
#define VFREQ 60 /* Hz */
#define TIMING_NAME "Linux FHD"
/* No ESTABLISHED_TIMINGx_BITS */
#define HSYNC_POL 0
#define VSYNC_POL 0
#define CRC 0xde

#include "edid.S"

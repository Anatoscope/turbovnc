/*
 * paintinactwarn.c
 *
 * software sprite routines for the popup warning
 */

/*
 *  Copyright (C) 2021 AnatoScope SA.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 *  USA.
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <X11/X.h>
#include "dix.h"
#include "gcstruct.h"
#include "pixmapstr.h"

const int rfbInactWarnTickCount = 16;

static void rfbPaintPowerSymbol(DrawablePtr drawable, GCPtr pGC,
                       unsigned short x, unsigned short y, unsigned short size,
                       CARD32 defaultFgColor, CARD32 bgColor)
{
  const CARD16 rOut = size;
  const CARD16 rIn = rOut * .71;

  const INT16 aTopSeamOut = 35;
  const INT16 aTopSeamIn = aTopSeamOut - 1;

  const CARD16 tickW = rOut - rIn;

  xArc outerArc;
  outerArc.x = x - rOut;
  outerArc.y = y - rOut;
  outerArc.width = 2 * rOut;
  outerArc.height = 2 * rOut;
  outerArc.angle1 = (90 + aTopSeamOut) * 64;
  outerArc.angle2 = (360 - 2 * aTopSeamOut) * 64;

  (*pGC->ops->PolyFillArc) (drawable, pGC, 1, &outerArc);

  ChangeGCVal gcval;
  BITS32 gcmask = GCForeground;

  gcval.val = bgColor;

  ChangeGC(NullClient, pGC, gcmask, &gcval);
  ValidateGC(drawable, pGC);

  xArc innerArc;
  innerArc.x = x - rIn;
  innerArc.y = y - rIn;
  innerArc.width = 2 * rIn;
  innerArc.height = 2 * rIn;
  innerArc.angle1 = (90 + aTopSeamIn) * 64;
  innerArc.angle2 = (360 - 2 * aTopSeamIn) * 64;

  (*pGC->ops->PolyFillArc) (drawable, pGC, 1, &innerArc);

  const double topSeamhw = rOut * sin(aTopSeamIn);
  const double topSeamh = rOut * cos(aTopSeamIn);

  xPoint v[3] = {
    { x, y },
    { x + topSeamhw, y + topSeamh },
    { x - topSeamhw, y + topSeamh }
  };

  (*pGC->ops->FillPolygon) (drawable, pGC, Convex, CoordModeOrigin, 3, v);

  gcval.val = defaultFgColor;

  ChangeGC(NullClient, pGC, gcmask, &gcval);
  ValidateGC(drawable, pGC);

  xRectangle tick;
  tick.x = x - tickW / 2;
  tick.y = y - rOut - rIn * .1;
  tick.width = tickW;
  tick.height = rOut - rIn * .17;

  (*pGC->ops->PolyFillRect) (drawable, pGC, 1, &tick);
}

static void paintTimer(DrawablePtr drawable, GCPtr pGC,
                       unsigned short x, unsigned short y, unsigned short size,
                       double amountLeft)
{
  const int ticksLeft = rfbInactWarnTickCount * min(max(amountLeft, 0.), 1.);

  /* external radius */
  const double r = size;

  /* internal radius */
  const double ri = r * .6;

  /* half of the angle of a tick */
  const double hta = 2 * M_PI / rfbInactWarnTickCount * .3;
  const double sinhta = sin(hta);
  const double coshta = cos(hta);

  /* angle between tick's side and radius */
  const double z = asin(ri / r * sin(hta));

  const double halfTickWidth = ri * sinhta;
  const double tickLength = - ri * coshta + sqrt(r * r - ri * ri * sinhta * sinhta);

  /* distance from center to a tick */
  const double tickDist = ri * coshta;

  const xPoint v[4] = {
    { -halfTickWidth, -tickDist },
    { -halfTickWidth, -tickDist - tickLength },
    { halfTickWidth, -tickDist - tickLength },
    { halfTickWidth, -tickDist }
  };

  int i, j;
  for (i = rfbInactWarnTickCount - ticksLeft + 1; i <= rfbInactWarnTickCount; ++i) {
    const double tickAngle = i * 2 * M_PI / rfbInactWarnTickCount;

    const double sinTickAngle = sin(tickAngle);
    const double cosTickAngle = cos(tickAngle);

    xPoint vt[4];

    for (j = 0; j != 4; ++j) {
      vt[j].x = v[j].x * cosTickAngle - v[j].y * sinTickAngle + x;
      vt[j].y = v[j].x * sinTickAngle + v[j].y * cosTickAngle + y;
    }

    (*pGC->ops->FillPolygon) (drawable, pGC, Convex, CoordModeOrigin, 4, vt);
  }
}

void rfbPaintInactWarning(DrawablePtr drawable, double amountLeft)
{
  xRectangle box;
  box.x = 0;
  box.y = 0;
  box.width = drawable->width;
  box.height = drawable->height;

  ScreenPtr pScreen = drawable->pScreen;

  GCPtr pGC = GetScratchGC(drawable->depth, pScreen);
  ChangeGCVal gcval[6];
  BITS32 gcmask;

  const CARD32 fgColor = 0xE6A0A0A0;
  const CARD32 bgColor = 0xE6303030;

  gcmask = GCFunction;
  gcval[0].val = GXcopy;
  gcval[1].val = bgColor;
  gcval[2].val = FillSolid;
  gcmask |= GCForeground | GCFillStyle;

  ChangeGC(NullClient, pGC, gcmask, gcval);
  ValidateGC(drawable, pGC);

  /* box background */

  (*pGC->ops->PolyFillRect) (drawable, pGC, 1, &box);

  /* box border */

  gcval[1].val = fgColor;
  ChangeGC(NullClient, pGC, gcmask, gcval);
  ValidateGC(drawable, pGC);

  (*pGC->ops->PolyRectangle) (drawable, pGC, 1, &box);

  const unsigned short xOrig = drawable->width / 2;
  const unsigned short yOrig = drawable->height / 2;
  const unsigned short drawableSize = min(drawable->width, drawable->height);

  rfbPaintPowerSymbol(drawable, pGC, xOrig, yOrig, drawableSize * .1, fgColor, bgColor);

  paintTimer(drawable, pGC, xOrig, yOrig, drawableSize * .45, amountLeft);

  FreeScratchGC(pGC);
}

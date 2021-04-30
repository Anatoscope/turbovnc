/*
 * popupsprite.c
 *
 * software sprite routines for the popup message - based on misprite
 */

/*
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *  Copyright (C) 2012-2014, 2017-2018 D. R. Commander.  All Rights Reserved.
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

/*

Copyright 1989, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.
*/

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include   <X11/X.h>
#include   <X11/Xproto.h>
#include   "misc.h"
#include   "pixmapstr.h"
#include   "mi.h"
#include   <X11/fonts/font.h>
#include   "scrnintstr.h"
#include   "windowstr.h"
#include   "gcstruct.h"
#include   "dixfontstr.h"
#include   <X11/fonts/fontstruct.h>
#include   "inputstr.h"
#include   "damage.h"
#include   "rfb.h"
#include   "popupsprite.h"

typedef struct {
    rfbPopupPtr pPopup;
    int x;                      /* popup position */
    int y;
    BoxRec saved;               /* saved area from the screen */
    Bool isUp;                  /* popup in frame buffer */
    Bool shouldBeUp;            /* popup should be displayed */
    WindowPtr pCacheWin;        /* window the popup last seen in */
    Bool isInCacheWin;
    ScreenPtr pScreen;
} rfbPopupInfoRec, *rfbPopupInfoPtr;

/*
 * per screen information
 */

typedef struct {
    /* screen procedures */
    CloseScreenProcPtr CloseScreen;
    GetImageProcPtr GetImage;
    GetSpansProcPtr GetSpans;
    SourceValidateProcPtr SourceValidate;

    /* window procedures */
    CopyWindowProcPtr CopyWindow;

    VisualPtr pVisual;
    DamagePtr pDamage;          /* damage tracking structure */
    Bool damageRegistered;
    int numberOfPopups;

    rfbPopupInfoRec popupInfoPriv;
} rfbPopupSpriteScreenRec, *rfbPopupSpriteScreenPtr;

#define SOURCE_COLOR	0
#define MASK_COLOR	1

/*
 * Overlap BoxPtr and Box elements
 */
#define BOX_OVERLAP(pCbox,X1,Y1,X2,Y2) \
 	(((pCbox)->x1 <= (X2)) && ((X1) <= (pCbox)->x2) && \
	 ((pCbox)->y1 <= (Y2)) && ((Y1) <= (pCbox)->y2))

/*
 * Overlap BoxPtr, origins, and rectangle
 */
#define ORG_OVERLAP(pCbox,xorg,yorg,x,y,w,h) \
    BOX_OVERLAP((pCbox),(x)+(xorg),(y)+(yorg),(x)+(xorg)+(w),(y)+(yorg)+(h))

/*
 * Overlap BoxPtr, origins and RectPtr
 */
#define ORGRECT_OVERLAP(pCbox,xorg,yorg,pRect) \
    ORG_OVERLAP((pCbox),(xorg),(yorg),(pRect)->x,(pRect)->y, \
		(int)((pRect)->width), (int)((pRect)->height))
/*
 * Overlap BoxPtr and horizontal span
 */
#define SPN_OVERLAP(pCbox,y,x,w) BOX_OVERLAP((pCbox),(x),(y),(x)+(w),(y))

#define LINE_SORT(x1,y1,x2,y2) \
{ int _t; \
  if (x1 > x2) { _t = x1; x1 = x2; x2 = _t; } \
  if (y1 > y2) { _t = y1; y1 = y2; y2 = _t; } }

#define LINE_OVERLAP(pCbox,x1,y1,x2,y2,lw2) \
    BOX_OVERLAP((pCbox), (x1)-(lw2), (y1)-(lw2), (x2)+(lw2), (y2)+(lw2))

#define SPRITE_DEBUG_ENABLE 0
#if SPRITE_DEBUG_ENABLE
#define SPRITE_DEBUG(x)	ErrorF x
#else
#define SPRITE_DEBUG(x)
#endif

static void
rfbPopupSpriteDisableDamage(ScreenPtr pScreen, rfbPopupSpriteScreenPtr pScreenPriv)
{
    if (pScreenPriv->damageRegistered) {
        DamageUnregister(pScreenPriv->pDamage);
        pScreenPriv->damageRegistered = 0;
    }
}

static void
rfbPopupSpriteEnableDamage(ScreenPtr pScreen, rfbPopupSpriteScreenPtr pScreenPriv)
{
    if (!pScreenPriv->damageRegistered) {
        pScreenPriv->damageRegistered = 1;
        DamageRegister(&(pScreen->GetScreenPixmap(pScreen)->drawable),
                       pScreenPriv->pDamage);
    }
}

static void
rfbPopupSpriteIsUp(rfbPopupInfoPtr pPopupInfo)
{
  pPopupInfo->isUp = TRUE;
    rfbFB.popupIsDrawn = TRUE;
}

static void
rfbPopupSpriteIsDown(rfbPopupInfoPtr pPopupInfo)
{
    pPopupInfo->isUp = FALSE;
    rfbFB.popupIsDrawn = FALSE;
}

/*
 * screen wrappers
 */

static DevPrivateKeyRec rfbPrivateKeyRec;

#define rfbPrivateKey (&rfbPrivateKeyRec)
#define GetPrivate(pScreen) \
	(dixLookupPrivate(&(pScreen)->devPrivates, rfbPrivateKey))

static Bool rfbPopupSpriteCloseScreen(ScreenPtr pScreen);
static void rfbPopupSpriteGetImage(DrawablePtr pDrawable, int sx, int sy,
                                   int w, int h, unsigned int format,
                                   unsigned long planemask, char *pdstLine);
static void rfbPopupSpriteGetSpans(DrawablePtr pDrawable, int wMax,
                                   DDXPointPtr ppt, int *pwidth, int nspans,
                                   char *pdstStart);
static void rfbPopupSpriteSourceValidate(DrawablePtr pDrawable, int x, int y,
                                         int width, int height,
                                         unsigned int subWindowMode);
static void rfbPopupSpriteCopyWindow(WindowPtr pWindow,
                                     DDXPointRec ptOldOrg, RegionPtr prgnSrc);

static void rfbPopupSpriteComputeSaved(ScreenPtr pScreen);

#define SCREEN_PROLOGUE(pPriv, pScreen, field) ((pScreen)->field = \
   (pPriv)->field)
#define SCREEN_EPILOGUE(pPriv, pScreen, field)\
    ((pPriv)->field = (pScreen)->field, (pScreen)->field = rfbPopupSprite##field)

/*
 * other misc functions
 */

static void rfbPopupSpriteRemovePopup(ScreenPtr pScreen);
static void rfbPopupSpriteSaveUnderPopup(ScreenPtr pScreen);
static void rfbPopupSpriteRestorePopup(ScreenPtr pScreen);

static void
rfbPopupSpriteReportDamage(DamagePtr pDamage, RegionPtr pRegion, void *closure)
{
    ScreenPtr pScreen = closure;
    rfbPopupSpriteScreenPtr pScreenPriv = GetPrivate(pScreen);
    rfbPopupInfoPtr pPopupInfo = &pScreenPriv->popupInfoPriv;

    if (pPopupInfo->isUp &&
        pPopupInfo->pScreen == pScreen &&
        RegionContainsRect(pRegion, &pPopupInfo->saved) != rgnOUT) {
        SPRITE_DEBUG(("Damage remove\n"));
        rfbPopupSpriteRemovePopup(pScreen);
    }
}

Bool
rfbPopupSpriteInitialize(ScreenPtr pScreen)
{
    rfbPopupSpriteScreenPtr pScreenPriv;
    VisualPtr pVisual;

    if (!DamageSetup(pScreen))
        return FALSE;

    if (!dixRegisterPrivateKey(&rfbPrivateKeyRec, PRIVATE_SCREEN, 0))
        return FALSE;

    pScreenPriv = rfbAlloc(sizeof(rfbPopupSpriteScreenRec));

    pScreenPriv->pDamage = DamageCreate(rfbPopupSpriteReportDamage,
                                        NULL,
                                        DamageReportRawRegion,
                                        TRUE, pScreen, pScreen);

    for (pVisual = pScreen->visuals;
         pVisual->vid != pScreen->rootVisual; pVisual++);
    pScreenPriv->pVisual = pVisual;
    pScreenPriv->CloseScreen = pScreen->CloseScreen;
    pScreenPriv->GetImage = pScreen->GetImage;
    pScreenPriv->GetSpans = pScreen->GetSpans;
    pScreenPriv->SourceValidate = pScreen->SourceValidate;

    pScreenPriv->CopyWindow = pScreen->CopyWindow;

    pScreenPriv->damageRegistered = 0;
    pScreenPriv->numberOfPopups = 0;

    dixSetPrivate(&pScreen->devPrivates, rfbPrivateKey, pScreenPriv);

    pScreen->CloseScreen = rfbPopupSpriteCloseScreen;
    pScreen->GetImage = rfbPopupSpriteGetImage;
    pScreen->GetSpans = rfbPopupSpriteGetSpans;
    pScreen->SourceValidate = rfbPopupSpriteSourceValidate;

    pScreen->CopyWindow = rfbPopupSpriteCopyWindow;

    rfbPopupInfoPtr pPopupInfo = &pScreenPriv->popupInfoPriv;

    pPopupInfo->pPopup = NULL;
    pPopupInfo->x = 0;
    pPopupInfo->y = 0;
    pPopupInfo->isUp = FALSE;
    pPopupInfo->shouldBeUp = FALSE;
    pPopupInfo->pCacheWin = NullWindow;
    pPopupInfo->isInCacheWin = FALSE;
    pPopupInfo->pScreen = FALSE;

    return TRUE;
}

/*
 * Screen wrappers
 */

/*
 * CloseScreen wrapper -- unwrap everything, free the private data
 * and call the wrapped function
 */

static Bool
rfbPopupSpriteCloseScreen(ScreenPtr pScreen)
{
    rfbPopupSpriteScreenPtr pScreenPriv = GetPrivate(pScreen);

    pScreen->CloseScreen = pScreenPriv->CloseScreen;
    pScreen->GetImage = pScreenPriv->GetImage;
    pScreen->GetSpans = pScreenPriv->GetSpans;
    pScreen->SourceValidate = pScreenPriv->SourceValidate;

    DamageDestroy(pScreenPriv->pDamage);

    free(pScreenPriv);

    return (*pScreen->CloseScreen) (pScreen);
}

static void
rfbPopupSpriteGetImage(DrawablePtr pDrawable, int sx, int sy, int w, int h,
                       unsigned int format, unsigned long planemask, char *pdstLine)
{
    ScreenPtr pScreen = pDrawable->pScreen;
    rfbPopupInfoPtr pPopupInfo;
    rfbPopupSpriteScreenPtr pPriv = GetPrivate(pScreen);

    SCREEN_PROLOGUE(pPriv, pScreen, GetImage);

    if (pDrawable->type == DRAWABLE_WINDOW) {
        pPopupInfo = &pPriv->popupInfoPriv;
        if (pPopupInfo->isUp && pPopupInfo->pScreen == pScreen &&
            ORG_OVERLAP(&pPopupInfo->saved, pDrawable->x, pDrawable->y,
                        sx, sy, w, h)) {
            SPRITE_DEBUG(("GetImage remove\n"));
            rfbPopupSpriteRemovePopup(pScreen);
        }
    }

    (*pScreen->GetImage) (pDrawable, sx, sy, w, h, format, planemask, pdstLine);

    SCREEN_EPILOGUE(pPriv, pScreen, GetImage);
}

static void
rfbPopupSpriteGetSpans(DrawablePtr pDrawable, int wMax, DDXPointPtr ppt,
                       int *pwidth, int nspans, char *pdstStart)
{
    ScreenPtr pScreen = pDrawable->pScreen;
    rfbPopupInfoPtr pPopupInfo;
    rfbPopupSpriteScreenPtr pPriv = GetPrivate(pScreen);

    SCREEN_PROLOGUE(pPriv, pScreen, GetSpans);

    if (pDrawable->type == DRAWABLE_WINDOW) {
        pPopupInfo = &pPriv->popupInfoPriv;;

        if (pPopupInfo->isUp && pPopupInfo->pScreen == pScreen) {
            DDXPointPtr pts;
            int *widths;
            int nPts;
            int xorg, yorg;

            xorg = pDrawable->x;
            yorg = pDrawable->y;

            for (pts = ppt, widths = pwidth, nPts = nspans;
                 nPts--; pts++, widths++) {
                if (SPN_OVERLAP(&pPopupInfo->saved, pts->y + yorg,
                                pts->x + xorg, *widths)) {
                    SPRITE_DEBUG(("GetSpans remove\n"));
                    rfbPopupSpriteRemovePopup(pScreen);
                    break;
                }
            }
        }
    }

    (*pScreen->GetSpans) (pDrawable, wMax, ppt, pwidth, nspans, pdstStart);

    SCREEN_EPILOGUE(pPriv, pScreen, GetSpans);
}

static void
rfbPopupSpriteSourceValidate(DrawablePtr pDrawable, int x, int y, int width,
                             int height, unsigned int subWindowMode)
{
    ScreenPtr pScreen = pDrawable->pScreen;
    rfbPopupInfoPtr pPopupInfo;
    rfbPopupSpriteScreenPtr pPriv = GetPrivate(pScreen);

    SCREEN_PROLOGUE(pPriv, pScreen, SourceValidate);

    if (pDrawable->type == DRAWABLE_WINDOW) {
        pPopupInfo = &pPriv->popupInfoPriv;;
        if (pPopupInfo->isUp && pPopupInfo->pScreen == pScreen &&
            ORG_OVERLAP(&pPopupInfo->saved, pDrawable->x, pDrawable->y,
                        x, y, width, height)) {
            SPRITE_DEBUG(("SourceValidate remove\n"));
            rfbPopupSpriteRemovePopup(pScreen);
        }
    }

    if (pScreen->SourceValidate)
        (*pScreen->SourceValidate) (pDrawable, x, y, width, height,
                                    subWindowMode);

    SCREEN_EPILOGUE(pPriv, pScreen, SourceValidate);
}

static void
rfbPopupSpriteCopyWindow(WindowPtr pWindow, DDXPointRec ptOldOrg, RegionPtr prgnSrc)
{
    ScreenPtr pScreen = pWindow->drawable.pScreen;
    rfbPopupInfoPtr pPopupInfo;
    rfbPopupSpriteScreenPtr pPriv = GetPrivate(pScreen);

    SCREEN_PROLOGUE(pPriv, pScreen, CopyWindow);

    pPopupInfo = &pPriv->popupInfoPriv;;
    /*
     * Damage will take care of destination check
     */
    if (pPopupInfo->isUp && pPopupInfo->pScreen == pScreen &&
        RegionContainsRect(prgnSrc, &pPopupInfo->saved) != rgnOUT) {
        SPRITE_DEBUG(("CopyWindow remove\n"));
        rfbPopupSpriteRemovePopup(pScreen);
    }

    (*pScreen->CopyWindow) (pWindow, ptOldOrg, prgnSrc);
    SCREEN_EPILOGUE(pPriv, pScreen, CopyWindow);
}

#define SPRITE_PAD  8

void
rfbPopupSpriteSetPopup(ScreenPtr pScreen,
                       rfbPopupPtr pPopup, int x, int y)
{
    rfbPopupInfoPtr pPopupInfo;
    rfbPopupSpriteScreenPtr pScreenPriv;
    rfbClientPtr cl, nextCl;

    pScreenPriv = GetPrivate(pScreen);
    pPopupInfo = &pScreenPriv->popupInfoPriv;

    if (!pPopup) {
        if (pPopupInfo->shouldBeUp)
            --pScreenPriv->numberOfPopups;
        pPopupInfo->shouldBeUp = FALSE;
        if (pPopupInfo->isUp)
            rfbPopupSpriteRemovePopup(pScreen);
        if (pScreenPriv->numberOfPopups == 0)
            rfbPopupSpriteDisableDamage(pScreen, pScreenPriv);
        pPopupInfo->pPopup = 0;
        return;
    }
    if (!pPopupInfo->shouldBeUp)
        pScreenPriv->numberOfPopups++;
    pPopupInfo->shouldBeUp = TRUE;
    pPopupInfo->x = x;
    pPopupInfo->y = y;
    pPopupInfo->pCacheWin = NullWindow;
    pPopupInfo->pPopup = pPopup;
    if (pPopupInfo->isUp) {
        SPRITE_DEBUG(("SetPopup remove %d\n", pDev->id));
        rfbPopupSpriteRemovePopup(pScreen);
    }

    if (!pPopupInfo->isUp && pPopupInfo->pPopup) {
        SPRITE_DEBUG(("SetPopup restore %d\n", pDev->id));
        rfbPopupSpriteSaveUnderPopup(pScreen);
        rfbPopupSpriteRestorePopup(pScreen);
    }
}

void
rfbPopupSpriteDevicePopupCleanup(ScreenPtr pScreen)
{
    rfbPopupSpriteScreenPtr pScreenPriv = GetPrivate(pScreen);
    rfbPopupInfoPtr pPopupInfo = &pScreenPriv->popupInfoPriv;

    rfbPopupCleanup(pScreen);

    memset(pPopupInfo, 0, sizeof(rfbPopupInfoRec));
}

/*
 * undraw/draw popup
 */

static void
rfbPopupSpriteRemovePopup(ScreenPtr pScreen)
{
    rfbPopupSpriteScreenPtr pScreenPriv;
    rfbPopupInfoPtr pPopupInfo;

    if (!rfbFB.popupIsDrawn)
        return;

    DamageDrawInternal(pScreen, TRUE);
    pScreenPriv = GetPrivate(pScreen);
    pPopupInfo = &pScreenPriv->popupInfoPriv;

    rfbFB.dontSendFramebufferUpdate = TRUE;

    rfbPopupSpriteIsDown(pPopupInfo);
    pPopupInfo->pCacheWin = NullWindow;
    rfbPopupSpriteDisableDamage(pScreen, pScreenPriv);
    if (!rfbPopupRestoreUnderPopup(pScreen,
                                   pPopupInfo->saved.x1,
                                   pPopupInfo->saved.y1,
                                   pPopupInfo->saved.x2 -
                                   pPopupInfo->saved.x1,
                                   pPopupInfo->saved.y2 -
                                   pPopupInfo->saved.y1)) {
        rfbPopupSpriteIsUp(pPopupInfo);
    }
    rfbPopupSpriteEnableDamage(pScreen, pScreenPriv);
    DamageDrawInternal(pScreen, FALSE);

    rfbFB.dontSendFramebufferUpdate = FALSE;
}

/*
 * Saves area under the popup.
 */

static void
rfbPopupSpriteSaveUnderPopup(ScreenPtr pScreen)
{
    rfbPopupSpriteScreenPtr pScreenPriv;
    rfbPopupInfoPtr pPopupInfo;

    DamageDrawInternal(pScreen, TRUE);
    pScreenPriv = GetPrivate(pScreen);
    pPopupInfo = &pScreenPriv->popupInfoPriv;

    rfbPopupSpriteComputeSaved(pScreen);

    rfbPopupSpriteDisableDamage(pScreen, pScreenPriv);

    rfbPopupSaveUnderPopup(pScreen,
                           pPopupInfo->saved.x1,
                           pPopupInfo->saved.y1,
                           pPopupInfo->saved.x2 -
                           pPopupInfo->saved.x1,
                           pPopupInfo->saved.y2 - pPopupInfo->saved.y1);
    SPRITE_DEBUG(("PopupSpriteSaveUnderPopup\n"));
    rfbPopupSpriteEnableDamage(pScreen, pScreenPriv);
    DamageDrawInternal(pScreen, FALSE);
}

/*
 * Restores the popup.
 */

static void
rfbPopupSpriteRestorePopup(ScreenPtr pScreen)
{
    rfbPopupSpriteScreenPtr pScreenPriv;
    int x, y;
    rfbPopupPtr pPopup;
    rfbPopupInfoPtr pPopupInfo;

    DamageDrawInternal(pScreen, TRUE);
    pScreenPriv = GetPrivate(pScreen);
    pPopupInfo = &pScreenPriv->popupInfoPriv;

    rfbPopupSpriteComputeSaved(pScreen);
    pPopup = pPopupInfo->pPopup;

    if (rfbFB.popupIsDrawn || !pPopup) {
        DamageDrawInternal(pScreen, FALSE);
        return;
    }

    rfbFB.dontSendFramebufferUpdate = TRUE;

    x = pPopupInfo->x;
    y = pPopupInfo->y;
    rfbPopupSpriteDisableDamage(pScreen, pScreenPriv);
    SPRITE_DEBUG(("RestorePopup\n"));
    if (rfbPopupPutUpPopup(pScreen,
                           pPopup, x, y)) {
        rfbPopupSpriteIsUp(pPopupInfo);
        pPopupInfo->pScreen = pScreen;
    }
    rfbPopupSpriteEnableDamage(pScreen, pScreenPriv);
    DamageDrawInternal(pScreen, FALSE);

    rfbFB.dontSendFramebufferUpdate = FALSE;
}

/*
 * compute the desired area of the screen to save
 */

static void
rfbPopupSpriteComputeSaved(ScreenPtr pScreen)
{
    int x, y, w, h;
    int wpad, hpad;
    rfbPopupPtr pPopup;
    rfbPopupSpriteScreenPtr pScreenPriv = GetPrivate(pScreen);
    rfbPopupInfoPtr pPopupInfo = &pScreenPriv->popupInfoPriv;

    pPopup = pPopupInfo->pPopup;

    if (!pPopup)
        return;

    x = pPopupInfo->x;
    y = pPopupInfo->y;
    w = pPopup->width;
    h = pPopup->height;
    wpad = SPRITE_PAD;
    hpad = SPRITE_PAD;
    pPopupInfo->saved.x1 = x - wpad;
    pPopupInfo->saved.y1 = y - hpad;
    pPopupInfo->saved.x2 = pPopupInfo->saved.x1 + w + wpad * 2;
    pPopupInfo->saved.y2 = pPopupInfo->saved.y1 + h + hpad * 2;
}

/*
 * Wrappers that allow us to interface with the older, non-device-aware code
 * in rfbserver.c
 */

void
rfbPopupSpriteRemovePopupAllDev(ScreenPtr pScreen)
{
    rfbPopupSpriteRemovePopup(pScreen);
}

void
rfbPopupSpriteRestorePopupAllDev(ScreenPtr pScreen)
{
    rfbPopupSpriteSaveUnderPopup(pScreen);
    rfbPopupSpriteRestorePopup(pScreen);
}

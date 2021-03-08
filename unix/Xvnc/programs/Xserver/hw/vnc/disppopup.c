/*
 * disppopup.c
 *
 * popup display routines - based on dispcur.c
 */

/*
 *  Copyright (C) 2014, 2017 D. R. Commander.  All Rights Reserved.
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
#include   "misc.h"
#include   "windowstr.h"
#include   "regionstr.h"
#include   "dixstruct.h"
#include   "scrnintstr.h"
#include   "servermd.h"
#include   "popupsprite.h"
#include   "gcstruct.h"

#include   "picturestr.h"

#include "inputstr.h"
#include "rfb.h"

typedef struct {
    CloseScreenProcPtr CloseScreen;
    PicturePtr pPicture;
    rfbPopupPtr pPopup;
} rfbPopupScreenRec, *rfbPopupScreenPtr;

/* popup's private data */
typedef struct {
    GCPtr pSaveGC, pRestoreGC;
    PixmapPtr pSave;
    PicturePtr pRootPicture;

    rfbPopupScreenRec screenPriv;

} rfbPopupBufferRec, *rfbPopupBufferPtr;

/* per-screen private data */
static DevPrivateKeyRec rfbPrivateKeyRec;

#define rfbPrivateKey (&rfbPrivateKeyRec)

#define rfbGetPrivate(s) ((rfbPopupBufferPtr) \
    dixLookupPrivate(&(s)->devPrivates, rfbPrivateKey))
#define rfbSetPrivate(s, v) \
    dixSetPrivate(&(s)->devPrivates, rfbPrivateKey, v);

static Bool rfbPopupCloseScreen(ScreenPtr pScreen);

static void
rfbPopupSwitchScreenPopup(ScreenPtr pScreen, rfbPopupPtr pPopup, PixmapPtr sourceBits, PixmapPtr maskBits, PicturePtr pPicture)
{
    rfbPopupBufferPtr pBuffer = rfbGetPrivate(pScreen);
    rfbPopupScreenPtr pScreenPriv = &pBuffer->screenPriv;

    if (pScreenPriv->pPicture)
        FreePicture(pScreenPriv->pPicture, 0);
    pScreenPriv->pPicture = pPicture;

    pScreenPriv->pPopup = pPopup;
}

static Bool
rfbPopupCloseScreen(ScreenPtr pScreen)
{
    rfbPopupBufferPtr pBuffer = rfbGetPrivate(pScreen);
    rfbPopupScreenPtr pScreenPriv = &pBuffer->screenPriv;
    pScreen->CloseScreen = pScreenPriv->CloseScreen;

    rfbPopupSwitchScreenPopup(pScreen, NULL, NULL, NULL, NULL);
    free((void *) pScreenPriv);
    return (*pScreen->CloseScreen) (pScreen);
}

#define EnsurePicture(picture,draw,win) (picture || rfbPopupMakePicture(&picture,draw,win))

static PicturePtr
rfbPopupMakePicture(PicturePtr * ppPicture, DrawablePtr pDraw, WindowPtr pWin)
{
    PictFormatPtr pFormat;
    XID subwindow_mode = IncludeInferiors;
    PicturePtr pPicture;
    int error;

    pFormat = PictureWindowFormat(pWin);
    if (!pFormat)
        return 0;
    pPicture = CreatePicture(0, pDraw, pFormat,
                             CPSubwindowMode, &subwindow_mode,
                             serverClient, &error);
    *ppPicture = pPicture;
    return pPicture;
}

static Bool
rfbPopupRealize(ScreenPtr pScreen, rfbPopupPtr pPopup)
{
    rfbPopupBufferPtr pBuffer = rfbGetPrivate(pScreen);
    rfbPopupScreenPtr pScreenPriv = &pBuffer->screenPriv;
    GCPtr pGC;
    ChangeGCVal gcvals;
    PixmapPtr   sourceBits, maskBits;
    PixmapPtr pPixmap;
    PictFormatPtr pFormat;
    int error;
    PicturePtr  pPicture;

    pFormat = PictureMatchFormat(pScreen, 32, PICT_a8r8g8b8);
    if (!pFormat)
        return FALSE;

    pPixmap = (*pScreen->CreatePixmap) (pScreen, pPopup->width,
                                        pPopup->height, 32,
                                        CREATE_PIXMAP_USAGE_SCRATCH);
    if (!pPixmap)
        return FALSE;

    if (pPopup->paintPopupFunc) {
        (*pPopup->paintPopupFunc)(&pPixmap->drawable,
                                  pPopup->paintPopupFuncUserData);
    } else {
      pGC = GetScratchGC(32, pScreen);
      if (!pGC) {
          (*pScreen->DestroyPixmap) (pPixmap);
          return FALSE;
      }
      ValidateGC(&pPixmap->drawable, pGC);
      (*pGC->ops->PutImage) (&pPixmap->drawable, pGC, 32,
                             0, 0, pPopup->width,
                             pPopup->height,
                             0, ZPixmap, (char *) pPopup->argb);
      FreeScratchGC(pGC);
    }

    pPicture = CreatePicture(0, &pPixmap->drawable,
                             pFormat, 0, 0, serverClient, &error);
    (*pScreen->DestroyPixmap) (pPixmap);
    if (!pPicture)
        return FALSE;

    rfbPopupSwitchScreenPopup(pScreen, pPopup, NULL, NULL, pPicture);
    return TRUE;
}

Bool
rfbPopupUnrealizePopup(ScreenPtr pScreen)
{
    rfbPopupBufferPtr pBuffer = rfbGetPrivate(pScreen);
    rfbPopupScreenPtr pScreenPriv = &pBuffer->screenPriv;
    rfbPopupSwitchScreenPopup(pScreen, NULL, NULL, NULL, NULL);
    return TRUE;
}

static GCPtr
rfbPopupMakeGC(WindowPtr pWin)
{
    GCPtr pGC;
    int status;
    XID gcvals[2];

    gcvals[0] = IncludeInferiors;
    gcvals[1] = FALSE;
    pGC = CreateGC((DrawablePtr) pWin,
                   GCSubwindowMode | GCGraphicsExposures, gcvals, &status,
                   (XID) 0, serverClient);
    return pGC;
}

Bool
rfbPopupPutUpPopup(ScreenPtr pScreen, rfbPopupPtr pPopup,
                   int x, int y)
{
    rfbPopupScreenPtr pScreenPriv;
    rfbPopupBufferPtr pBuffer;
    WindowPtr pWin;

    if (!rfbPopupRealize(pScreen, pPopup))
        return FALSE;

    pWin = pScreen->root;
    pBuffer = rfbGetPrivate(pScreen);
    pScreenPriv = &pBuffer->screenPriv;

    if (!EnsurePicture(pBuffer->pRootPicture, &pWin->drawable, pWin))
        return FALSE;
    CompositePicture(PictOpOver,
                     pScreenPriv->pPicture,
                     NULL,
                     pBuffer->pRootPicture,
                     0, 0, 0, 0,
                     x, y, pPopup->width, pPopup->height);

    return TRUE;
}

Bool
rfbPopupSaveUnderPopup(ScreenPtr pScreen,
                       int x, int y, int w, int h)
{
    rfbPopupBufferPtr pBuffer;
    PixmapPtr pSave;
    WindowPtr pWin;
    GCPtr pGC;

    pBuffer = rfbGetPrivate(pScreen);

    pSave = pBuffer->pSave;
    pWin = pScreen->root;
    if (!pSave || pSave->drawable.width < w || pSave->drawable.height < h) {
        if (pSave)
            (*pScreen->DestroyPixmap) (pSave);
        pBuffer->pSave = pSave =
            (*pScreen->CreatePixmap) (pScreen, w, h, pScreen->rootDepth, 0);
        if (!pSave)
            return FALSE;
    }

    pGC = pBuffer->pSaveGC;
    if (pSave->drawable.serialNumber != pGC->serialNumber)
        ValidateGC((DrawablePtr) pSave, pGC);
    (*pGC->ops->CopyArea) ((DrawablePtr) pWin, (DrawablePtr) pSave, pGC,
                           x, y, w, h, 0, 0);
    return TRUE;
}

Bool
rfbPopupRestoreUnderPopup(ScreenPtr pScreen,
                          int x, int y, int w, int h)
{
    rfbPopupBufferPtr pBuffer;
    PixmapPtr pSave;
    WindowPtr pWin;
    GCPtr pGC;

    pBuffer = rfbGetPrivate(pScreen);
    pSave = pBuffer->pSave;

    pWin = pScreen->root;
    if (!pSave)
        return FALSE;

    pGC = pBuffer->pRestoreGC;
    if (pWin->drawable.serialNumber != pGC->serialNumber)
        ValidateGC((DrawablePtr) pWin, pGC);
    (*pGC->ops->CopyArea) ((DrawablePtr) pSave, (DrawablePtr) pWin, pGC,
                           0, 0, w, h, x, y);
    return TRUE;
}

Bool
rfbPopupScreenInitialize(ScreenPtr pScreen)
{
    rfbPopupBufferPtr pBuffer = rfbAlloc0(sizeof(rfbPopupBufferRec));

    if (!dixRegisterPrivateKey(&rfbPrivateKeyRec, PRIVATE_SCREEN, 0))
        return FALSE;

    rfbSetPrivate(pScreen, pBuffer);

    /* init the screen part of drawing data */
    rfbPopupScreenPtr pScreenPriv = &pBuffer->screenPriv;

    pScreenPriv->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = rfbPopupCloseScreen;

    if (!rfbPopupSpriteInitialize(pScreen)) {
        free((void *) pScreenPriv);
        return FALSE;
    }

    return TRUE;
}

void
rfbPopupCleanup(ScreenPtr pScreen);

Bool
rfbPopupInitialize(ScreenPtr pScreen)
{
    rfbPopupBufferPtr pBuffer = rfbGetPrivate(pScreen);
    WindowPtr pWin = pScreen->root;

    pBuffer->pSaveGC = rfbPopupMakeGC(pWin);
    if (!pBuffer->pSaveGC)
        goto failure;

    pBuffer->pRestoreGC = rfbPopupMakeGC(pWin);
    if (!pBuffer->pRestoreGC)
        goto failure;

    pBuffer->pRootPicture = NULL;

    /* (re)allocated lazily depending on the popup size */
    pBuffer->pSave = NULL;

    return TRUE;

 failure:

    rfbPopupCleanup(pScreen);

    return FALSE;
}

void
rfbPopupCleanup(ScreenPtr pScreen)
{
    rfbPopupBufferPtr pBuffer = rfbGetPrivate(pScreen);

    if (pBuffer) {
        if (pBuffer->pSaveGC)
            FreeGC(pBuffer->pSaveGC, (GContext) 0);
        if (pBuffer->pRestoreGC)
            FreeGC(pBuffer->pRestoreGC, (GContext) 0);

        /* If a pRootPicture was allocated for a root window, it
         * is freed when that root window is destroyed, so don't
         * free it again here. */

        if (pBuffer->pSave)
            (*pScreen->DestroyPixmap) (pBuffer->pSave);

        /* cleanup the screen part of drawing data */
        rfbPopupScreenPtr pScreenPriv = &pBuffer->screenPriv;
        pScreen->CloseScreen = pScreenPriv->CloseScreen;
        rfbPopupSwitchScreenPopup(pScreen, NULL, NULL, NULL, NULL);

        /* all removed */
        free(pBuffer);
        rfbSetPrivate(pScreen, NULL);
    }
}

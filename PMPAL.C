/* pmpal, Summer 1993, David Fortin, Plymouth, IN */
#include <stdlib.h>
#define INCL_GPI
#define INCL_WIN
#define INCL_DOS
#include <os2.h>
#include <mem.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define MAX_VAL 255
#define clientClass "DPalstuff"


MRESULT EXPENTRY ClientWinProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2);
void CyclePaletteThread(void);
int Palette1(ULONG *tbl);
void GeneralTunnel1(HPS hps,int xc,int yc,int size);
void GeneralTunnel2(HPS hps,int xc,int yc,int size);
void CosWavV(HPS hps,int sizex,int sizey,int side);
void CosWavH(HPS hps,int sizex,int sizey,int side);
void DoPaint(void);

static USHORT redTarg;
static USHORT greenTarg;
static USHORT blueTarg;
static HPAL   hpal;
ULONG  Tbl[MAX_VAL];

static LONG   hasPalMan;
static LONG   colorsToShow;
static HAB    hab;
QMSG  qmsg;
HMTX hmtxScreenWrite;
static HPS    hps;
static USHORT sizex, sizey;


#pragma argsused
int main (int argc, char *argv[])
{
  HMQ   hmq;
  HWND  hwnd,
        hwndClient;

  ULONG createFlags  =  FCF_TITLEBAR | FCF_SYSMENU | FCF_SIZEBORDER
                                     | FCF_MINMAX | FCF_SHELLPOSITION
                                     | FCF_TASKLIST;

  LONG sColors;
  HDC hdcScr;
  HPS hps;
  hdcScr = GpiQueryDevice (hps = WinGetPS (HWND_DESKTOP));
  DevQueryCaps (hdcScr, CAPS_COLORS, 1L, &sColors);
  DevQueryCaps (hdcScr, CAPS_ADDITIONAL_GRAPHICS, 1, &hasPalMan);
  hasPalMan &= CAPS_PALETTE_MANAGER;
  WinReleasePS(hps);

  hab = WinInitialize (0);
  hmq = WinCreateMsgQueue (hab, 0);
  WinRegisterClass (hab, clientClass, (PFNWP) ClientWinProc, CS_SIZEREDRAW, 0);
  if (hasPalMan)
  { 
    DosCreateMutexSem((PSZ)NULL,&hmtxScreenWrite,DC_SEM_SHARED,FALSE);
    hwnd = WinCreateStdWindow (HWND_DESKTOP, WS_VISIBLE, &createFlags,
                               clientClass, "Palette Stuff - by David Fortin, Plymouth, IN", 0L,
                               0UL, 0, &hwndClient);
    {
      MENUITEM mi;
      HWND hWndSysSubMenu;
      WinSendMsg(WinWindowFromID(hwnd,FID_SYSMENU),0x01f3,0L,((PVOID)MAKEULONG(LOUSHORT(&mi),(HIUSHORT(&mi)<<3)|7)));  //MM_QUERYITEMBYPOS
      hWndSysSubMenu = mi.hwndSubMenu;
      mi.iPosition = MIT_END;
      mi.afStyle = MIS_SEPARATOR;
      mi.afAttribute = 0;
      mi.id = -1;
      mi.hwndSubMenu = 0;
      mi.hItem = 0;
      WinSendMsg(hWndSysSubMenu,MM_INSERTITEM,MPFROMP(&mi),NULL);
      mi.afStyle = MIS_TEXT;
      mi.id = 2000;  // idm_about
      WinSendMsg(hWndSysSubMenu,MM_INSERTITEM,MPFROMP(&mi),"About...");
    }
  }
  else
  {
    WinMessageBox(HWND_DESKTOP,HWND_DESKTOP,"The program cycles the palette.  You have no Palette, pff...","Palette Stuff",0,MB_OK);
    return 0;
  }
  while (WinGetMsg (hab, &qmsg, NULLHANDLE, 0, 0))
  {   
    WinDispatchMsg (hab, &qmsg);
  }
  WinDestroyWindow (hwnd);  
  WinDestroyMsgQueue (hmq); 
  WinTerminate (hab);       
  DosExit(1,1);
  return 0;
}



MRESULT EXPENTRY ClientWinProc (HWND hwnd, USHORT msg, MPARAM mp1, MPARAM mp2)
{
  static HDC    hdc;
  static BOOL   firstPaint = TRUE;
  USHORT rj,gj,bj;
  int    j;
  char szBuffer[100];
                                  
  TID ThreadID;

  switch (msg)
  {
    case WM_CREATE:
      {
        SIZEL sizl = {0L, 0L};
        hdc = WinOpenWindowDC (hwnd);
        hps = GpiCreatePS (hab, hdc, &sizl, PU_PELS | GPIF_DEFAULT
                 | GPIT_MICRO | GPIA_ASSOC);
        colorsToShow = Palette1(Tbl);
        hpal = GpiCreatePalette (hab, 0L, LCOLF_CONSECRGB,colorsToShow, Tbl);
        GpiSelectPalette (hps, hpal);
        DosCreateThread(&ThreadID,(PFNTHREAD)CyclePaletteThread,(ULONG)NULL,0,0x5000);
      }
      return (MRESULT) FALSE;
    case WM_DESTROY:
      if (hasPalMan)
      {
        GpiSelectPalette (hps, NULLHANDLE);
        GpiDeletePalette (hpal);
      }
      GpiDestroyPS (hps);
      return (MRESULT) FALSE;
    case WM_ERASEBACKGROUND:
      return (MRESULT) TRUE;
    case WM_PAINT:
      {
        POINTL ptl;
        LONG   j;

        WinBeginPaint (hwnd, hps, NULL);
        if (firstPaint)
        {
          ULONG palSize = colorsToShow;
          WinRealizePalette (hwnd, hps, &palSize);
          firstPaint = FALSE;
        }
        WinEndPaint (hps);
        DosCreateThread(&ThreadID,(PFNTHREAD)DoPaint,(ULONG)NULL,0,0x5000);
      }
      return (MRESULT) FALSE;
    case WM_REALIZEPALETTE:
      {
        ULONG palSize = colorsToShow;
        if (WinRealizePalette (hwnd, hps, &palSize))
        {
          WinInvalidateRect (hwnd, NULL, FALSE);
        }
        return (MRESULT) FALSE;
      }
    case WM_SIZE:
      sizex = SHORT1FROMMP (mp2);
      sizey = SHORT2FROMMP (mp2);
      return (MRESULT) FALSE;
     case WM_SYSCOMMAND:
     case WM_COMMAND:
       if(LOUSHORT(mp1)==2000)
         WinMessageBox(hwnd,hwnd,"Palette Stuff, by David Fortin, Plymouth, IN","Summer 1993",0,MB_OK);
    default:
      return (WinDefWindowProc (hwnd, msg, mp1, mp2));
  }
}




void DoPaint(void)
{
  int cx,cy,size;

  DosRequestMutexSem(hmtxScreenWrite,SEM_INDEFINITE_WAIT);
  cx = sizex/2;
  cy = sizey/2;
  size = min(sizex,sizey);
  GeneralTunnel1(hps,cx,cy,max(sizex,sizey));
  GeneralTunnel1(hps,sizex/5,sizey/5,size/5);
  GeneralTunnel2(hps,sizex-sizex/5,sizey/5,size/5);
  GeneralTunnel1(hps,sizex-sizex/5,sizey-sizey/5,size/5);
  GeneralTunnel2(hps,sizex/5,sizey-sizey/5,size/5);
  CosWavV(hps,sizex,sizey,0);
  CosWavH(hps,sizex,sizey,0);
  DosReleaseMutexSem(hmtxScreenWrite);
  return;
}





int Palette1(ULONG *tbl)
{
  USHORT i;
  USHORT rj,gj,bj;

  int j= 0;
  for(i=10;i<31;i++)
  {
    rj = 6*((BYTE)30+10-i);
    gj = 6*((BYTE)30+10-i);
    bj = 6*((BYTE)30);
    tbl[j]= PC_RESERVED*16777216 + 65536 * rj + 256 * gj + bj;
    j++;
  }
  for(i=10;i<31;i++)
  {
    rj = 6*((BYTE)i);
    gj = 6*((BYTE)10);
    bj = 6*((BYTE)30);
    tbl[j] = PC_RESERVED*16777216 + 65536 * rj + 256 * gj + bj;
    j++;
  }
  for(i=10;i<31;i++)
  {
    rj = 6*((BYTE)30);
    gj = 6*((BYTE)10);
    bj = 6*((BYTE)30+10-i);
    tbl[j] = PC_RESERVED*16777216 + 65536 * rj + 256 * gj + bj;
    j++;
  }
  for(i=10;i<31;i++)
  {
    rj = 6*((BYTE)30);
    gj = 6*((BYTE)i);
    bj = 6*((BYTE)10);
    tbl[j] = PC_RESERVED*16777216 + 65536 * rj + 256 * gj + bj;
    j++;
  }
  for(i=10;i<31;i++)
  {
    rj = 6*((BYTE)30+10-i);
    gj = 6*((BYTE)30);
    bj = 6*((BYTE)10);
    tbl[j] = PC_RESERVED*16777216 + 65536 * rj + 256 * gj + bj;
    j++;
  }
  for(i=10;i<31;i++)
  {
    rj = 6*((BYTE)10);
    gj = 6*((BYTE)30);
    bj = 6*((BYTE)i);
    tbl[j] = PC_RESERVED*16777216 + 65536 * rj + 256 * gj + bj;
    j++;
  }
  for(i=10;i<31;i++)
  {
    rj = 6*((BYTE)i);
    gj = 6*((BYTE)30);
    bj = 6*((BYTE)30);
    tbl[j] = PC_RESERVED*16777216 + 65536 * rj + 256 * gj + bj;
    j++;
  }
  return j;
}

void GeneralTunnel1(HPS hps,int xc,int yc,int size)
{
  int i;
  int x;
  POINTL ptl;
  for(i=0;i<size;i++)
  {
    GpiSetColor(hps,colorsToShow - i % colorsToShow);
    ptl.y = yc-i;
    ptl.x = xc-i;
    GpiMove (hps, &ptl);
    ptl.x = xc-i;
    ptl.y = yc+i;
    GpiLine(hps,&ptl);
    ptl.x = xc+i;
    ptl.y = yc+i;
    GpiLine(hps,&ptl);
    ptl.x = xc+i;
    ptl.y = yc-i;
    GpiLine(hps,&ptl);
    ptl.x = xc-i;
    ptl.y = yc-i;
    GpiLine(hps,&ptl);
  }
  return;
}


void GeneralTunnel2(HPS hps,int xc,int yc,int size)
{
  int i;
  int x;
  POINTL ptl;
  for(i=0;i<size;i++)
  {
    GpiSetColor(hps,i % colorsToShow);
    ptl.y = yc-i;
    ptl.x = xc-i;
    GpiMove (hps, &ptl);
    ptl.x = xc-i;
    ptl.y = yc+i;
    GpiLine(hps,&ptl);
    ptl.x = xc+i;
    ptl.y = yc+i;
    GpiLine(hps,&ptl);
    ptl.x = xc+i;
    ptl.y = yc-i;
    GpiLine(hps,&ptl);
    ptl.x = xc-i;
    ptl.y = yc-i;
    GpiLine(hps,&ptl);
  }
  return;
}



void CosWavV(HPS hps,int sizex,int sizey,int side)
{
  int i;
  RECTL rect;
  POINTL ptl;
  double c,s,amp;
  int rb;
  int rl,rr,screeny;

  rb = sizey/30;
  rl = sizex/2-sizex/30;
  rr = sizex/2+sizex/30;
  amp = sizex/5;
  

  for(i=-sizey/10;i<sizey;i+=1)
  {
    c=cos((double)i*0.0349*(180.0/(double)sizey))*amp;
    s=sin((double)i*0.0349*(180.0/(double)sizey))*amp;
    rect.xLeft = rl+c;
    rect.xRight = rr+s;
    rect.yTop = i;
    rect.yBottom = rb + i;
    GpiSetColor(hps,((i+sizex/10))%colorsToShow);
    if(side==1 || side==0)
    {
      ptl.x= rect.xLeft;
      ptl.y = rect.yTop;
      GpiMove(hps,&ptl);
      ptl.x= rect.xRight;
      ptl.y = rect.yBottom;
      GpiBox (hps, DRO_FILL, &ptl, 0L, 0L);
    }
    rect.yBottom = sizey-i;
    rect.yTop = sizey -(rb + i);
    rect.xLeft = rl-c;
    rect.xRight = rr-s;
    if(side==0 || side == 2)
    {
      ptl.x= rect.xLeft;
      ptl.y = rect.yTop;
      GpiMove(hps,&ptl);
      ptl.x= rect.xRight;
      ptl.y = rect.yBottom;
      GpiBox (hps, DRO_FILL, &ptl, 0L, 0L);
    }
  }
  return;
}


void CosWavH(HPS hps,int sizex,int sizey,int side)
{
  int i;
  RECTL rect;
  POINTL ptl;
  double c,s,amp;
  int rb;
  int rl,rr,screeny;

  rb = sizex/10;
  rl = sizey/2-sizey/20;
  rr = sizey/2+sizey/20;
  amp = sizey/3;

  
  for(i=-sizex/10;i<sizex;i+=1)
  {
    c=cos((double)i*0.0349*(360.0/(double)sizex))*amp;
    rect.yTop = rl+c;
    rect.yBottom = rr+c;
    rect.xLeft = i;
    rect.xRight = rb + i;
    GpiSetColor(hps,(i/3+sizex/10)%colorsToShow);
    if(side==1 || side==0)
    {
      ptl.x= rect.xLeft;
      ptl.y = rect.yTop;
      GpiMove(hps,&ptl);
      ptl.x= rect.xRight;
      ptl.y = rect.yBottom;
      GpiBox (hps, DRO_FILL, &ptl, 0L, 0L);
    }
    rect.xRight = sizex-i;
    rect.xLeft = sizex -(rb + i);
    rect.yTop = rl-c;
    rect.yBottom = rr-c;
    GpiSetColor(hps,(i/2+sizex/10)%colorsToShow);
    if(side==0 || side == 2)
    {
      ptl.x= rect.xLeft;
      ptl.y = rect.yTop;
      GpiMove(hps,&ptl);
      ptl.x= rect.xRight;
      ptl.y = rect.yBottom;
      GpiBox (hps, DRO_FILL, &ptl, 0L, 0L);
    }
  }
  return;
}


void CyclePaletteThread(void)
{
    ULONG hold[20];
    INT    j;
    int blockSize = 5;
    while(1)
    {
      DosRequestMutexSem(hmtxScreenWrite,SEM_INDEFINITE_WAIT);
      memcpy(hold,&Tbl[0],sizeof(Tbl[0])*(blockSize));
      memcpy(Tbl,&Tbl[blockSize],sizeof(Tbl[0])*(colorsToShow-blockSize));
      memcpy(&Tbl[colorsToShow-blockSize],hold,sizeof(Tbl[0])*(blockSize));
      GpiAnimatePalette(hpal,LCOLF_CONSECRGB,0,colorsToShow,Tbl);
      DosReleaseMutexSem(hmtxScreenWrite);
      DosSleep(20);
    }
 }


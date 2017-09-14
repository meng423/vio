/******************************************************************************
  A simple program of Hisilicon Hi35xx video input and output implementation.
  Copyright (C), 2010-2011, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2011-8 Created
******************************************************************************/

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#include "sample_comm.h"
#include "loadbmp.h"
#include "hi_rtc.h"
#include "SDL.h"
#include "SDL_ttf.h"
#include "hifb.h"
#include "hi_tde_api.h"
#include "hi_tde_type.h"
#include "hi_tde_errcode.h"

#define WIDTH_1920             1920
#define HEIGHT_1080            1080
#define WIDTH_720              720
#define HEIGHT_576             576

#define SAMPLE_IMAGE_WIDTH     532
#define SAMPLE_IMAGE_HEIGHT    55
#define SAMPLE_IMAGE_SIZE      (532*55*2)
#define SAMPLE_IMAGE_NUM       20
#define HIFB_RED_1555   0xFC00

#define GRAPHICS_LAYER_G0  0

#define SAMPLE_IMAGE1_PATH		"./%d.bmp"

static struct fb_bitfield s_r16 = {10, 5, 0};
static struct fb_bitfield s_g16 = {5, 5, 0};
static struct fb_bitfield s_b16 = {0, 5, 0};
static struct fb_bitfield s_a16 = {15, 1, 0};

HI_U32 Bmpheight;
HI_U32 Bmpwidth;
HI_U32 maxW;
HI_U32 maxH;

HI_U32 g_Phyaddr0 = 0;
HI_U32 g_Phyaddr1 = 0;
HI_U32 g_CanvasAddr = 0;

pthread_t g_stHifbThread = 0;
pthread_t gs_TimePid;
pthread_t gs_OsdPid;
pthread_t gs_BmpPid;
pthread_t gs_ZoomPid;

HI_U16* pBuf;
TDE_HANDLE s32Handle;
static HI_CHAR gs_cExitFlag = 0;
VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_PAL;
//VO_INTF_TYPE_E g_enVoIntfType = VO_INTF_CVBS;
VO_INTF_TYPE_E g_enVoIntfType = VO_INTF_BT1120;

PIC_SIZE_E g_enPicSize = PIC_HD1080;

HI_U32 gs_u32ViFrmRate = 0;
static SAMPLE_BIND_E g_enVioBind = SAMPLE_BIND_VI_VO;

const char *dev_name = "/dev/hi_rtc";
rtc_time_t tm;
int fd = -1;

RGN_ATTR_S stRgnAttrSet;
RGN_CANVAS_INFO_S stCanvasInfo;
BITMAP_S stBitmap;
SIZE_S stSize;
static RGN_HANDLE Handle = 0;
HI_S32 s32Ret = HI_SUCCESS;
pthread_mutex_t mutex ;
VO_ZOOM_ATTR_S stZoom;

typedef struct hiPTHREAD_HIFB_SAMPLE
{
    HI_S32 fd;
    HI_S32 layer;
    HI_S32 ctrlkey;
} PTHREAD_HIFB_SAMPLE_INFO;

SAMPLE_VI_CONFIG_S g_stViChnConfig =
{
    .enViMode = PANASONIC_MN34220_SUBLVDS_1080P_30FPS,
    .enNorm   = VIDEO_ENCODING_MODE_AUTO,

    .enRotate = ROTATE_NONE,
    .enViChnSet = VI_CHN_SET_NORMAL,
    .enWDRMode  = WDR_MODE_NONE,
    .enFrmRate  = SAMPLE_FRAMERATE_DEFAULT,
    .enSnsNum = SAMPLE_SENSOR_SINGLE,
};

SAMPLE_VO_CONFIG_S g_stVoConfig =
{
    .u32DisBufLen = 3,
};

static SAMPLE_VPSS_ATTR_S g_stVpssAttr =
{
    .VpssGrp = 0,
	.VpssGrp0 = 0,
	.VpssGrp1 = 1,
    .VpssChn = 1,
  
    .stVpssGrpAttr =
    {
        .bDciEn    = HI_FALSE,
        .bHistEn   = HI_FALSE,
        .bIeEn     = HI_FALSE,
        .bNrEn     = HI_TRUE,
        .bStitchBlendEn = HI_FALSE,
        .stNrAttr  =
        {
            .enNrType       = VPSS_NR_TYPE_VIDEO,
            .u32RefFrameNum = 2,
            .stNrVideoAttr =
            {
                .enNrRefSource = VPSS_NR_REF_FROM_RFR,
                .enNrOutputMode = VPSS_NR_OUTPUT_NORMAL
            }
        },
        .enDieMode = VPSS_DIE_MODE_NODIE,
        .enPixFmt  = PIXEL_FORMAT_YUV_SEMIPLANAR_420,
        .u32MaxW   = 1920,
        .u32MaxH   = 1080
    },
    
    .stVpssChnAttr =
    {
        .bBorderEn       = HI_FALSE,
        .bFlip           = HI_FALSE,
        .bMirror         = HI_FALSE,
        .bSpEn           = HI_FALSE,
        .s32DstFrameRate = -1,
        .s32SrcFrameRate = -1,
    },

    .stVpssChnMode =
    {
        .bDouble         = HI_FALSE,
        .enChnMode       = VPSS_CHN_MODE_USER,
        .enCompressMode  = COMPRESS_MODE_NONE,
        .enPixelFormat   = PIXEL_FORMAT_YUV_SEMIPLANAR_420,
        .u32Width        = 1920,
        .u32Height       = 1080
    },

    .stVpssExtChnAttr =
    {
    }
};

static HI_S32 SAMPLE_VIO_StartVPSS(VPSS_GRP VpssGrp, VPSS_CHN VpssChn,
                                   SAMPLE_VPSS_ATTR_S* pstVpssAttr, ROTATE_E enRotate);
static HI_S32 SAMPLE_VIO_StopVPSS(VPSS_GRP VpssGrp, VPSS_CHN VpssChn);


/******************************************************************************
* function : show usage
******************************************************************************/
void SAMPLE_VIO_ResetGrobalVariable(void)
{
#ifdef __HuaweiLite__
    gs_enNorm = VIDEO_ENCODING_MODE_PAL;
    g_enVoIntfType = VO_INTF_CVBS;
    g_enPicSize = PIC_HD1080;
    gs_u32ViFrmRate = 0;
    g_enVioBind = SAMPLE_BIND_VI_VO;
    
    g_stViChnConfig.enViMode = PANASONIC_MN34220_SUBLVDS_1080P_30FPS;
    g_stViChnConfig.enNorm   = VIDEO_ENCODING_MODE_AUTO;
    g_stViChnConfig.enRotate = ROTATE_NONE;
    g_stViChnConfig.enViChnSet = VI_CHN_SET_NORMAL;
    g_stViChnConfig.enWDRMode  = WDR_MODE_NONE;
    g_stViChnConfig.enFrmRate  = SAMPLE_FRAMERATE_DEFAULT;
    g_stViChnConfig.enSnsNum = SAMPLE_SENSOR_SINGLE;
#endif
    return;
}

/******************************************************************************
* function : to process abnormal case
******************************************************************************/
void SAMPLE_VIO_HandleSig(HI_S32 signo)
{
    if (SIGINT == signo || SIGTERM == signo)
    {
        SAMPLE_COMM_ISP_Stop();
        SAMPLE_COMM_SYS_Exit();
        printf("\033[0;31mprogram termination abnormally!\033[0;39m\n");
    }
    exit(-1);
}

static HI_S32 SAMPLE_VIO_StartVO(SAMPLE_VO_CONFIG_S *pstVoConfig)
{
    VO_DEV VoDev = SAMPLE_VO_DEV_DSD0;
    //VO_CHN VoChn = 0;
    VO_LAYER VoLayer = 0;
    SAMPLE_VO_MODE_E enVoMode = VO_MODE_1MUX;
    VO_PUB_ATTR_S stVoPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    HI_S32 s32Ret = HI_SUCCESS;
//	VO_CHN_ATTR_S stChnAttr;

    stVoPubAttr.enIntfType = g_enVoIntfType;
    if (VO_INTF_BT1120 == g_enVoIntfType)
    {
        stVoPubAttr.enIntfSync = VO_OUTPUT_1080P60;
        gs_u32ViFrmRate = 50;
    }
    else
    {
        stVoPubAttr.enIntfSync = VO_OUTPUT_PAL;
    }
    stVoPubAttr.u32BgColor = 0x000000ff;

    /* In HD, this item should be set to HI_FALSE */
    s32Ret = SAMPLE_COMM_VO_StartDev(0, &stVoPubAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartDev failed!\n");
        return s32Ret;
    }

    stLayerAttr.bClusterMode = HI_FALSE;
    stLayerAttr.bDoubleFrame = HI_FALSE;
    stLayerAttr.enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;

    stLayerAttr.stDispRect.s32X = 0;
    stLayerAttr.stDispRect.s32Y = 0;

    if (SAMPLE_SENSOR_DOUBLE == g_stViChnConfig.enSnsNum)
    {
        if (VO_INTF_BT1120 == g_enVoIntfType)
        {
            stLayerAttr.stDispRect.u32Width = 1920;
            stLayerAttr.stDispRect.u32Height = 1080;
            stLayerAttr.u32DispFrmRt = 30;

        }
        else
        {
            stLayerAttr.stDispRect.u32Width = 720;
            stLayerAttr.stDispRect.u32Height = 288;
            stLayerAttr.u32DispFrmRt = 25;
        }
    }
    else
    {
        s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync,
                                      &stLayerAttr.stDispRect.u32Width, &stLayerAttr.stDispRect.u32Height,
                                      &stLayerAttr.u32DispFrmRt);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VO_GetWH failed!\n");
            SAMPLE_COMM_VO_StopDev(VoDev);
            return s32Ret;
        }
    }

    stLayerAttr.stImageSize.u32Width  = stLayerAttr.stDispRect.u32Width;
    stLayerAttr.stImageSize.u32Height = stLayerAttr.stDispRect.u32Height;

    if (pstVoConfig->u32DisBufLen)
    {
        s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr, HI_FALSE);
    }
    else
    {
        s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr, HI_TRUE);
    }
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartChn failed!\n");
        SAMPLE_COMM_VO_StopDev(VoDev);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer, enVoMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VO_StartChn failed!\n");
        SAMPLE_COMM_VO_StopLayer(VoLayer);
        SAMPLE_COMM_VO_StopDev(VoDev);
        return s32Ret;
    }

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_VIO_StopVO(void)
{
    VO_DEV VoDev = SAMPLE_VO_DEV_DSD0;
    VO_LAYER VoLayer = 0;
    SAMPLE_VO_MODE_E enVoMode = VO_MODE_1MUX;

    if (SAMPLE_SENSOR_DOUBLE == g_stViChnConfig.enSnsNum)
    {
        enVoMode = VO_MODE_2MUX;
    }

    SAMPLE_COMM_VO_StopChn(VoDev, enVoMode);
    SAMPLE_COMM_VO_StopLayer(VoLayer);
    SAMPLE_COMM_VO_StopDev(VoDev);

    return HI_SUCCESS;
}

static HI_S32 SAMPLE_VIO_StartVPSS(VPSS_GRP VpssGrp, VPSS_CHN VpssChn, 
                            SAMPLE_VPSS_ATTR_S* pstVpssAttr, ROTATE_E enRotate)
{
    VPSS_GRP_ATTR_S *pstVpssGrpAttr = NULL;
    VPSS_CHN_ATTR_S *pstVpssChnAttr = NULL;
    VPSS_CHN_MODE_S *pstVpssChnMode = NULL;
    VPSS_EXT_CHN_ATTR_S *pstVpssExtChnAttr = NULL;
    HI_S32 s32Ret = HI_SUCCESS;

    if (NULL != pstVpssAttr)
    {
        pstVpssGrpAttr = &pstVpssAttr->stVpssGrpAttr;
        pstVpssChnAttr = &pstVpssAttr->stVpssChnAttr;
        pstVpssChnMode = &pstVpssAttr->stVpssChnMode;
        pstVpssExtChnAttr = &pstVpssAttr->stVpssExtChnAttr;
    }

    
    s32Ret = SAMPLE_COMM_VPSS_StartGroup(VpssGrp, pstVpssGrpAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start VPSS GROUP failed!\n");
        return s32Ret;
    }

    if (enRotate != ROTATE_NONE && SAMPLE_COMM_IsViVpssOnline())
    {
        s32Ret =  HI_MPI_VPSS_SetRotate(VpssGrp, VpssChn, enRotate);
        if (HI_SUCCESS != s32Ret)
        {
           SAMPLE_PRT("set VPSS rotate failed\n");
           SAMPLE_COMM_VPSS_StopGroup(VpssGrp);
           return s32Ret;
        }
    }

    s32Ret = SAMPLE_COMM_VPSS_EnableChn(VpssGrp, VpssChn, pstVpssChnAttr, pstVpssChnMode, pstVpssExtChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start VPSS CHN failed!\n");
        SAMPLE_COMM_VPSS_StopGroup(VpssGrp);
        return s32Ret;
    }

    return s32Ret;
}

static HI_S32 SAMPLE_VIO_StopVPSS(VPSS_GRP VpssGrp, VPSS_CHN VpssChn)
{

    SAMPLE_COMM_VPSS_DisableChn(VpssGrp, VpssChn);
    SAMPLE_COMM_VPSS_StopGroup(VpssGrp);

    return HI_SUCCESS;
}

/******************************************************************************
 * Function:    SAMPLE_VIO_PreView
 * Description: online mode / offline mode. Embeded isp, phychn preview
******************************************************************************/
static HI_S32 SAMPLE_VIO_StartViVo(SAMPLE_VI_CONFIG_S* pstViConfig, SAMPLE_VPSS_ATTR_S* pstVpssAttr, SIZE_S* pstSize, SAMPLE_VO_CONFIG_S* pstVoConfig)
{
    VI_CHN   ViChn   = 0;
    VO_DEV   VoDev   = SAMPLE_VO_DEV_DSD0;
    VO_CHN   VoChn   = 0;
//	VPSS_GRP VpssGrp = pstVpssAttr->VpssGrp;
	VPSS_CHN VpssChn = pstVpssAttr->VpssChn;
    VPSS_GRP VpssGrp0 = pstVpssAttr->VpssGrp0;
	VPSS_GRP VpssGrp1 = pstVpssAttr->VpssGrp1;
    HI_BOOL  bViVpssOnline = HI_FALSE;
    HI_S32   s32Ret = HI_SUCCESS;
//    VI_STITCH_CORRECTION_ATTR_S stCorretionAttr;
//    VI_CHN ViChn0 = 0;
//    VI_CHN ViChn1 = 1;
    MPP_CHN_S stSrcChn, stDestChn;

    HI_ASSERT(NULL != pstViConfig);
    HI_ASSERT(NULL != pstSize);
    HI_ASSERT(NULL != pstVoConfig);
    /******************************************
     step 1: start vi dev & chn to capture
    ******************************************/
    s32Ret = SAMPLE_COMM_VI_StartVi(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed!\n");
        return s32Ret;
    }

    /******************************************
    step 2: start VO SD0
    ******************************************/
    s32Ret = SAMPLE_VIO_StartVO(pstVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO start VO failed with %#x!\n", s32Ret);
        goto exit1;
    }

    /******************************************
    step 3: start VPSS
    ******************************************/
    bViVpssOnline = SAMPLE_COMM_IsViVpssOnline();
    if (bViVpssOnline || 
        (SAMPLE_BIND_VPSS_VO == g_enVioBind) ||
        (SAMPLE_BIND_VI_VPSS_VO == g_enVioBind) )
    {
        s32Ret = SAMPLE_VIO_StartVPSS(VpssGrp0, VpssChn, pstVpssAttr, pstViConfig->enRotate);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_VIO_StartVPSS failed with %#x!\n", s32Ret);
            goto exit2;
        }

		s32Ret = SAMPLE_VIO_StartVPSS(VpssGrp1, VpssChn, pstVpssAttr, pstViConfig->enRotate);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_VIO_StartVPSS failed with %#x!\n", s32Ret);
            goto exit2;
        }

    }

    /******************************************
    step 4: sys bind
    ******************************************/
#if 1
    if (bViVpssOnline)       // vi-vpss online
    {
 //       SAMPLE_COMM_VO_BindVpss(VoDev, VoChn, VpssGrp, VpssChn);
    }
    else
    {
        if (SAMPLE_BIND_VI_VPSS_VO == g_enVioBind)
        {
            if (SAMPLE_SENSOR_DOUBLE == g_stViChnConfig.enSnsNum)
            {
                stSrcChn.enModId  = HI_ID_VIU;
                stSrcChn.s32DevId = 0;
                stSrcChn.s32ChnId = 0;
				
                stDestChn.enModId  = HI_ID_VPSS;
                stDestChn.s32DevId = 0;
                stDestChn.s32ChnId = 0;

                s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
                if (s32Ret != HI_SUCCESS)
                {
                    SAMPLE_PRT("failed with %#x!\n", s32Ret);
                    return HI_FAILURE;
                }

                stSrcChn.enModId  = HI_ID_VIU;
                stSrcChn.s32DevId = 0;
                stSrcChn.s32ChnId = 1;
				
                stDestChn.enModId  = HI_ID_VPSS;
                stDestChn.s32DevId = 1;
                stDestChn.s32ChnId = 0;

                s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
                if (s32Ret != HI_SUCCESS)
                {
                    SAMPLE_PRT("failed with %#x!\n", s32Ret);
                    return HI_FAILURE;
                }

					SAMPLE_COMM_VO_BindVpss(0, 0, 0, 1);   // VPSS --> VO
					SAMPLE_COMM_VO_BindVpss(0, 1, 1, 1);  
				
            }
            else
            {
                SAMPLE_COMM_VI_BindVpss(pstViConfig->enViMode);   // VI --> VPSS
//                SAMPLE_COMM_VO_BindVpss(VoDev, VoChn, VpssGrp, VpssChn);   // VPSS --> VO
            }
        }
        else
        {
            SAMPLE_COMM_VO_BindVi(VoDev, VoChn, ViChn);    // VI --> VO
        }
    }
#endif

    return s32Ret;

exit2:
    SAMPLE_VIO_StopVO();
exit1:
    SAMPLE_COMM_VI_StopVi(pstViConfig);
    return s32Ret;
}

HI_S32 SAMPLE_VIO_StopViVo(SAMPLE_VI_CONFIG_S* pstViConfig, SAMPLE_VPSS_ATTR_S* pstVpssAttr)
{
    VI_CHN   ViChn   = 0;
    VO_DEV   VoDev   = SAMPLE_VO_DEV_DSD0;
    VO_CHN   VoChn   = 0;
    VPSS_GRP VpssGrp = pstVpssAttr->VpssGrp;
    VPSS_CHN VpssChn = pstVpssAttr->VpssChn;
    HI_BOOL  bViVpssOnline;

    bViVpssOnline = SAMPLE_COMM_IsViVpssOnline();
    
    SAMPLE_COMM_VO_UnBindVpss(VoDev, VoChn, VpssGrp, VpssChn);
    SAMPLE_COMM_VI_UnBindVpss(pstViConfig->enViMode);
    SAMPLE_COMM_VO_UnBindVi(VoDev, ViChn);

    if (bViVpssOnline || 
        (SAMPLE_BIND_VPSS_VO == g_enVioBind) ||
        (SAMPLE_BIND_VI_VPSS_VO == g_enVioBind) )
    {
        SAMPLE_VIO_StopVPSS(VpssGrp, VpssChn);
    }

    SAMPLE_VIO_StopVO();
    SAMPLE_COMM_VI_StopVi(pstViConfig);

    return HI_SUCCESS;
}


HI_S32 SAMPLE_RGN_CreateOverlayExForVpss(RGN_HANDLE Handle, HI_U32 u32Num)
{
    HI_S32 i;
    HI_S32 s32Ret;
    MPP_CHN_S stChn;
    HI_U32 u32layer = 0;
    RGN_ATTR_S stRgnAttrSet;
    RGN_CHN_ATTR_S stChnAttr;

    /*attach the OSD to the vpss*/
#if 0
    stChn.enModId  = HI_ID_VPSS;
    stChn.s32DevId = 0;
    stChn.s32ChnId = 1;
#endif

    stChn.enModId = HI_ID_VOU;
    stChn.s32DevId = 0;
    stChn.s32ChnId = 0;

    for (i = Handle; i < (Handle + u32Num); i++)
    {
        stRgnAttrSet.enType = OVERLAYEX_RGN;
        stRgnAttrSet.unAttr.stOverlayEx.enPixelFmt       = PIXEL_FORMAT_RGB_1555;
        stRgnAttrSet.unAttr.stOverlayEx.stSize.u32Width  = 1000;
        stRgnAttrSet.unAttr.stOverlayEx.stSize.u32Height = 100;
        stRgnAttrSet.unAttr.stOverlayEx.u32BgColor       = 0x000003e0;

        s32Ret = HI_MPI_RGN_Create(i, &stRgnAttrSet);
        if (s32Ret != HI_SUCCESS)
        {
            printf("HI_MPI_RGN_Create failed! s32Ret: 0x%x.\n", s32Ret);
            return s32Ret;
        }

        stChnAttr.bShow  = HI_TRUE;
        stChnAttr.enType = OVERLAYEX_RGN;
        stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32X = 20;
        stChnAttr.unChnAttr.stOverlayExChn.stPoint.s32Y = 20;
        stChnAttr.unChnAttr.stOverlayExChn.u32BgAlpha   = 0;
        stChnAttr.unChnAttr.stOverlayExChn.u32FgAlpha   = 255;
        stChnAttr.unChnAttr.stOverlayExChn.u32Layer     = u32layer;
        u32layer++;

        s32Ret = HI_MPI_RGN_AttachToChn(i, &stChn, &stChnAttr);
        if (s32Ret != HI_SUCCESS)
        {
            printf("HI_MPI_RGN_AttachToChn failed! s32Ret: 0x%x.\n", s32Ret);
            return s32Ret;
        }
    }

    return HI_SUCCESS;

}

HI_S32 SAMPLE_RGN_UpdateCanvas(const char* filename, BITMAP_S* pstBitmap, HI_BOOL bFil,
                               HI_U32 u16FilColor, SIZE_S* pstSize, HI_U32 u32Stride, PIXEL_FORMAT_E enPixelFmt)
{
    OSD_SURFACE_S Surface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    if (GetBmpInfo(filename, &bmpFileHeader, &bmpInfo) < 0)
    {
        printf("GetBmpInfo err!\n");
        return HI_FAILURE;
    }

    if (PIXEL_FORMAT_RGB_1555 == enPixelFmt)
    {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;
    }
    else if (PIXEL_FORMAT_RGB_4444 == enPixelFmt)
    {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB4444;
    }
    else if (PIXEL_FORMAT_RGB_8888 == enPixelFmt)
    {
        Surface.enColorFmt = OSD_COLOR_FMT_RGB8888;
    }
    else
    {
        printf("Pixel format is not support!\n");
        return HI_FAILURE;
    }

    if (NULL == pstBitmap->pData)
    {
        printf("malloc osd memroy err!\n");
        return HI_FAILURE;
    }
    CreateSurfaceByCanvas(filename, &Surface, (HI_U8*)(pstBitmap->pData), pstSize->u32Width, pstSize->u32Height, u32Stride);

    pstBitmap->u32Width  = Surface.u16Width;
    pstBitmap->u32Height = Surface.u16Height;

    if (PIXEL_FORMAT_RGB_1555 == enPixelFmt)
    {
        pstBitmap->enPixelFormat = PIXEL_FORMAT_RGB_1555;
    }
    else if (PIXEL_FORMAT_RGB_4444 == enPixelFmt)
    {
        pstBitmap->enPixelFormat = PIXEL_FORMAT_RGB_4444;
    }
    else if (PIXEL_FORMAT_RGB_8888 == enPixelFmt)
    {
        pstBitmap->enPixelFormat = PIXEL_FORMAT_RGB_8888;
    }

    int i, j;
    HI_U16* pu16Temp;
    pu16Temp = (HI_U16*)pstBitmap->pData;

    if (bFil)
    {
        for (i = 0; i < pstBitmap->u32Height; i++)
        {
            for (j = 0; j < pstBitmap->u32Width; j++)
            {
                if (u16FilColor == *pu16Temp)
                {
                    *pu16Temp &= 0x7FFF;
                }

                pu16Temp++;
            }
        }

    }

    return HI_SUCCESS;
}


HI_S32 SAMPLE_HIFB_LoadBmp(const char* filename, HI_U8* pAddr)
{
    OSD_SURFACE_S Surface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    if (GetBmpInfo(filename, &bmpFileHeader, &bmpInfo) < 0)
    {
        SAMPLE_PRT("GetBmpInfo err!\n");
        return HI_FAILURE;
    }

    Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;

    CreateSurfaceByBitMap(filename, &Surface, pAddr);

	Bmpheight = Surface.u16Height;
	Bmpwidth = Surface.u16Width;

    return HI_SUCCESS;
}


HI_S32 SAMPLE_HIFB_OsdDisplay(HI_U8* Viraddr, HI_U32 g_Phyaddr,HI_U8 x,HI_U8 y,HI_U8 i)
{
	TDE2_RECT_S stSrcRect, stDstRect;
    TDE2_SURFACE_S stSrc, stDst;
	HI_CHAR image_name[128];
	HI_U8* pDst = NULL;

    snprintf(image_name, 128, SAMPLE_IMAGE1_PATH, i % 2);
    pDst = (HI_U8*)Viraddr;
    SAMPLE_HIFB_LoadBmp(image_name, pDst);

    /* 0. open tde */
    stSrcRect.s32Xpos = 0;
    stSrcRect.s32Ypos = 0;
    stSrcRect.u32Height = Bmpheight;
    stSrcRect.u32Width = Bmpwidth;
    stDstRect.s32Xpos = x;
    stDstRect.s32Ypos = y;
    stDstRect.u32Height = stSrcRect.u32Width;
    stDstRect.u32Width = stSrcRect.u32Width;

    stDst.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
    stDst.u32Width = maxW;
    stDst.u32Height = maxH;
    stDst.u32Stride = maxW * 2;
    stDst.u32PhyAddr = g_CanvasAddr;

    stSrc.enColorFmt = TDE2_COLOR_FMT_ARGB1555;
    stSrc.u32Width = Bmpwidth;
    stSrc.u32Height = Bmpheight;
    stSrc.u32Stride = 2 * Bmpwidth;
    stSrc.u32PhyAddr = g_Phyaddr;
    stSrc.bAlphaExt1555 = HI_TRUE;
    stSrc.bAlphaMax255 = HI_TRUE;
    stSrc.u8Alpha0 = 0XFF;
    stSrc.u8Alpha1 = 0XFF;

    s32Ret = HI_TDE2_QuickCopy(s32Handle, &stSrc, &stSrcRect, &stDst, &stDstRect);
    if (s32Ret < 0)
    {
        SAMPLE_PRT("HI_TDE2_QuickCopy:%d failed,ret=0x%x!\n", __LINE__, s32Ret);
        HI_TDE2_CancelJob(s32Handle);
        HI_MPI_SYS_MmzFree(g_Phyaddr, Viraddr);
        g_Phyaddr = 0;
        
        HI_MPI_SYS_MmzFree(g_CanvasAddr, pBuf);
        g_CanvasAddr = 0;
        return HI_NULL;
    }
}


HI_VOID* SAMPLE_HIFB_REFRESH(void* pData)
{
    HI_S32 s32Ret = HI_SUCCESS;
    HIFB_LAYER_INFO_S stLayerInfo = {0};
    HIFB_BUFFER_S stCanvasBuf;
    HI_BOOL bShow;
    HIFB_POINT_S stPoint = {0};
    struct fb_var_screeninfo stVarInfo;
    PTHREAD_HIFB_SAMPLE_INFO* pstInfo;
    HIFB_COLORKEY_S stColorKey;
    HI_VOID* Viraddr0 = NULL;
    HI_VOID* Viraddr1 = NULL;

    if (HI_NULL == pData)
    {
        return HI_NULL;
    }
    pstInfo = (PTHREAD_HIFB_SAMPLE_INFO*)pData;

    /* 1. open framebuffer device overlay 0 */
    pstInfo->fd = open("/dev/fb0", O_RDWR, 0);
    if (pstInfo->fd < 0)
    {
        SAMPLE_PRT("open /dev/fb0 failed!\n");
        return HI_NULL;
    }
    /*all layer surport colorkey*/
    stColorKey.bKeyEnable = HI_TRUE;
    stColorKey.u32Key = 0x0;
    if (ioctl(pstInfo->fd, FBIOPUT_COLORKEY_HIFB, &stColorKey) < 0)
    {
        SAMPLE_PRT("FBIOPUT_COLORKEY_HIFB!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    s32Ret = ioctl(pstInfo->fd, FBIOGET_VSCREENINFO, &stVarInfo);
    if (s32Ret < 0)
    {
        SAMPLE_PRT("GET_VSCREENINFO failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    if (ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0)
    {
        SAMPLE_PRT("set screen original show position failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    if (VO_INTF_CVBS == g_enVoIntfType)
    {
        maxW = WIDTH_720;
        maxH = HEIGHT_576;
    }
    else
    {
        maxW = WIDTH_1920;
        maxH = HEIGHT_1080;
    }

    stVarInfo.transp = s_a16;
    stVarInfo.red = s_r16;
    stVarInfo.green = s_g16;
    stVarInfo.blue = s_b16;
    stVarInfo.bits_per_pixel = 16;
    stVarInfo.activate = FB_ACTIVATE_NOW;
    stVarInfo.xres = stVarInfo.xres_virtual = maxW;
    stVarInfo.yres = stVarInfo.yres_virtual = maxH;
    s32Ret = ioctl(pstInfo->fd, FBIOPUT_VSCREENINFO, &stVarInfo);
    if (s32Ret < 0)
    {
        SAMPLE_PRT("PUT_VSCREENINFO failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    switch (pstInfo->ctrlkey)
    {
        case 0 :
        {
            stLayerInfo.BufMode = HIFB_LAYER_BUF_ONE;
            stLayerInfo.u32Mask = HIFB_LAYERMASK_BUFMODE;
            break;
        }

        case 1 :
        {
            stLayerInfo.BufMode = HIFB_LAYER_BUF_DOUBLE;
            stLayerInfo.u32Mask = HIFB_LAYERMASK_BUFMODE;
            break;
        }

        default:
        {
            stLayerInfo.BufMode = HIFB_LAYER_BUF_NONE;
            stLayerInfo.u32Mask = HIFB_LAYERMASK_BUFMODE;
        }
    }
    s32Ret = ioctl(pstInfo->fd, FBIOPUT_LAYER_INFO, &stLayerInfo);
    if (s32Ret < 0)
    {
        SAMPLE_PRT("PUT_LAYER_INFO failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    bShow = HI_TRUE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0)
    {
        SAMPLE_PRT("FBIOPUT_SHOW_HIFB failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    if (HI_FAILURE == HI_MPI_SYS_MmzAlloc(&g_CanvasAddr, ((void**)&pBuf),
                                          NULL, NULL, maxW * maxH * 2))
    {
        SAMPLE_PRT("allocate memory (maxW*maxH*2 bytes) failed\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    stCanvasBuf.stCanvas.u32PhyAddr = g_CanvasAddr;
    stCanvasBuf.stCanvas.u32Height = maxH;
    stCanvasBuf.stCanvas.u32Width = maxW;
    stCanvasBuf.stCanvas.u32Pitch = maxW * 2;
    stCanvasBuf.stCanvas.enFmt = HIFB_FMT_ARGB1555;
    memset(pBuf, 0x00, stCanvasBuf.stCanvas.u32Pitch * stCanvasBuf.stCanvas.u32Height);

    /*change bmp*/
    if (HI_FAILURE == HI_MPI_SYS_MmzAlloc(&g_Phyaddr0, ((void**)&Viraddr0),
                                          NULL, NULL, SAMPLE_IMAGE_WIDTH * SAMPLE_IMAGE_HEIGHT* 2))
    {
        SAMPLE_PRT("allocate memory  failed\n");
        HI_MPI_SYS_MmzFree(g_CanvasAddr, pBuf);
        g_CanvasAddr = 0;
        close(pstInfo->fd);
        return HI_NULL;
    }

    if (HI_FAILURE == HI_MPI_SYS_MmzAlloc(&g_Phyaddr1, ((void**)&Viraddr1),
                                          NULL, NULL, SAMPLE_IMAGE_WIDTH * SAMPLE_IMAGE_HEIGHT* 2))
    {
        SAMPLE_PRT("allocate memory  failed\n");
        HI_MPI_SYS_MmzFree(g_CanvasAddr, pBuf);
        g_CanvasAddr = 0;
        close(pstInfo->fd);
        return HI_NULL;
    }

    s32Ret = HI_TDE2_Open();
    if (s32Ret < 0)
    {
        SAMPLE_PRT("HI_TDE2_Open failed :%d!\n", s32Ret);
        HI_MPI_SYS_MmzFree(g_Phyaddr0, Viraddr0);
        g_Phyaddr0 = 0;

		HI_MPI_SYS_MmzFree(g_Phyaddr1, Viraddr1);
		g_Phyaddr1 = 0;

		
        HI_MPI_SYS_MmzFree(g_CanvasAddr, pBuf);
        g_CanvasAddr = 0;
        
        close(pstInfo->fd);
        return HI_NULL;
    }

    /*time to play*/
    while(1)
    {
   		pthread_mutex_lock(&mutex);
        if ('q' == gs_cExitFlag)
        {
            printf("process exit...\n");
            break;
        }

        /* 1. start job */
        s32Handle = HI_TDE2_BeginJob();
        if (HI_ERR_TDE_INVALID_HANDLE == s32Handle)
        {
            SAMPLE_PRT("start job failed!\n");
            
			HI_MPI_SYS_MmzFree(g_Phyaddr0, Viraddr0);
			g_Phyaddr0 = 0;
			
			HI_MPI_SYS_MmzFree(g_Phyaddr1, Viraddr1);
			g_Phyaddr1 = 0;

            
            HI_MPI_SYS_MmzFree(g_CanvasAddr, pBuf);
            g_CanvasAddr = 0;
            
            close(pstInfo->fd);
            return HI_NULL;
        }
		
		SAMPLE_HIFB_OsdDisplay(Viraddr0, g_Phyaddr0, 10, 10, 0);
		SAMPLE_HIFB_OsdDisplay(Viraddr1, g_Phyaddr1, 10, 60, 1);

        /* 3. submit job */
        s32Ret = HI_TDE2_EndJob(s32Handle, HI_FALSE, HI_TRUE, 10);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("Line:%d,HI_TDE2_EndJob failed,ret=0x%x!\n", __LINE__, s32Ret);
            HI_TDE2_CancelJob(s32Handle);
            
			HI_MPI_SYS_MmzFree(g_Phyaddr0, Viraddr0);
			g_Phyaddr0 = 0;
			
			HI_MPI_SYS_MmzFree(g_Phyaddr1, Viraddr1);
			g_Phyaddr1 = 0;

            
            HI_MPI_SYS_MmzFree(g_CanvasAddr, pBuf);
            g_CanvasAddr = 0;
            
            close(pstInfo->fd);
            return HI_NULL;
        }

        stCanvasBuf.UpdateRect.x = 0;
        stCanvasBuf.UpdateRect.y = 0;
        stCanvasBuf.UpdateRect.w = maxW;
        stCanvasBuf.UpdateRect.h = maxH;
        s32Ret = ioctl(pstInfo->fd, FBIO_REFRESH, &stCanvasBuf);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("REFRESH failed!\n");
            
			HI_MPI_SYS_MmzFree(g_Phyaddr0, Viraddr0);
			g_Phyaddr0 = 0;
			
			HI_MPI_SYS_MmzFree(g_Phyaddr1, Viraddr1);
			g_Phyaddr1 = 0;

            
            HI_MPI_SYS_MmzFree(g_CanvasAddr, pBuf);
            g_CanvasAddr = 0;
            
            close(pstInfo->fd);
            return HI_NULL;
        }
		
		pthread_mutex_unlock(&mutex);
        usleep(300);
    }

	HI_MPI_SYS_MmzFree(g_Phyaddr0, Viraddr0);
	g_Phyaddr0 = 0;
	
	HI_MPI_SYS_MmzFree(g_Phyaddr1, Viraddr1);
	g_Phyaddr1 = 0;

    
    HI_MPI_SYS_MmzFree(g_CanvasAddr, pBuf);
    g_CanvasAddr = 0;
    
    close(pstInfo->fd);

    return HI_NULL;
}

HI_VOID* OSD_OSD(HI_VOID* p){
	char str[20];

	char * pstr = str;
	SDL_PixelFormat *fmt;
	TTF_Font *font;
	SDL_Surface *text, *temp; 
	int height = 0;
	while(1){
		pthread_mutex_lock(&mutex);
		
		int ret = ioctl(fd, HI_RTC_RD_TIME, &tm);
		if (ret < 0) {
			printf("ioctl: HI_RTC_RD_TIME failed\n");
		}
		height++;
		sprintf(str,"height:%d",height);
       
		if (TTF_Init() < 0)
			{
				fprintf(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
				SDL_Quit();
			}  
		
		font = TTF_OpenFont("./simhei.ttf", 48); 
		if (font == NULL)
			{
			fprintf(stderr, "Couldn't load %d pt font from %s: %s\n",18,"ptsize", SDL_GetError());
			}
		
		SDL_Color forecol = { 0xff, 0xff, 0xff, 0xff };
		text = TTF_RenderUTF8_Solid(font, pstr, forecol);

		fmt = (SDL_PixelFormat*)malloc(sizeof(SDL_PixelFormat));
		memset(fmt,0,sizeof(SDL_PixelFormat));
		fmt->BitsPerPixel = 16;
		fmt->BytesPerPixel = 2;
		fmt->colorkey = 0xffffffff;
		fmt->alpha = 0xff;

		temp = SDL_ConvertSurface(text,fmt,0);
		SDL_SaveBMP(temp, "1.bmp");
		free(fmt); 

		SDL_FreeSurface(text);
		SDL_FreeSurface(temp);
		TTF_CloseFont(font);
		TTF_Quit(); 

		pthread_mutex_unlock(&mutex);
		
		usleep(300);
	}
	
}


HI_VOID* OSD_TIME(HI_VOID* p){
	char str[20];

	char * pstr = str;
	SDL_PixelFormat *fmt;
	TTF_Font *font;
	SDL_Surface *text, *temp; 
	
	memset(&tm, 0, sizeof(tm));
		
	fd = open(dev_name, O_RDWR);
	if (fd < 0) {
		printf("open %s failed\n", dev_name);
		return -1;
	}
	
	
	while(1){
		pthread_mutex_lock(&mutex);
		
		int ret = ioctl(fd, HI_RTC_RD_TIME, &tm);
		if (ret < 0) {
			printf("ioctl: HI_RTC_RD_TIME failed\n");
		}

		sprintf(str,"%d-%d-%d %d:%d:%d",tm.year,tm.month,tm.date,tm.hour,tm.minute,tm.second);
       
		if (TTF_Init() < 0)
			{
				fprintf(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
				SDL_Quit();
			}  
		
		font = TTF_OpenFont("./simhei.ttf", 48); 
		if (font == NULL)
			{
			fprintf(stderr, "Couldn't load %d pt font from %s: %s\n",18,"ptsize", SDL_GetError());
			}
		
		SDL_Color forecol = { 0xff, 0xff, 0xff, 0xff };
		text = TTF_RenderUTF8_Solid(font, pstr, forecol);

		fmt = (SDL_PixelFormat*)malloc(sizeof(SDL_PixelFormat));
		memset(fmt,0,sizeof(SDL_PixelFormat));
		fmt->BitsPerPixel = 16;
		fmt->BytesPerPixel = 2;
		fmt->colorkey = 0xffffffff;
		fmt->alpha = 0xff;

		temp = SDL_ConvertSurface(text,fmt,0);
		SDL_SaveBMP(temp, "0.bmp");
		free(fmt); 

		SDL_FreeSurface(text);
		SDL_FreeSurface(temp);
		TTF_CloseFont(font);
		TTF_Quit(); 

		pthread_mutex_unlock(&mutex);
		
		usleep(300);
	}
	
}

HI_VOID* Zoom(HI_VOID* p){
	while(1){
		pthread_mutex_lock(&mutex);
		/* define input parameter */
		HI_S32 s32Ret;
		VO_ZOOM_ATTR_S stZoomWindow, stZooom;

		stZooom = stZoom;
		stZoomWindow.enZoomType = stZooom.enZoomType;
		stZoomWindow.stZoomRect.s32X = stZooom.stZoomRect.s32X;
		stZoomWindow.stZoomRect.s32Y = stZooom.stZoomRect.s32Y;
		stZoomWindow.stZoomRect.u32Width = stZooom.stZoomRect.u32Width;
		stZoomWindow.stZoomRect.u32Height = stZooom.stZoomRect.u32Height;
		/* set zoom window */
		s32Ret = HI_MPI_VO_SetZoomInWindow(0, 0, &stZoomWindow);
		if (s32Ret != HI_SUCCESS)
		{	
		printf("Set zoom attribute failed, ret = %#x.\n", s32Ret);
		}

		pthread_mutex_unlock(&mutex);
		sleep(1);
		}
	}


/******************************************************************************
* function : vi/vpss: duoble  sensro/mipi/isp/vi,online/offline mode VI-VO. Embeded isp, phychn channel preview.
******************************************************************************/
HI_S32 SAMPLE_VIO_DoubleSensor_Preview(SAMPLE_VI_CONFIG_S* pstViConfig)
{
    HI_U32 u32ViChnCnt = 2;
    VB_CONF_S stVbConf;
    PIC_SIZE_E enPicSize = g_enPicSize;
    HI_U32 u32BlkSize;
    SAMPLE_VPSS_ATTR_S stVpssAttr;

    if (SAMPLE_COMM_IsViVpssOnline())
    {
        SAMPLE_PRT("SAMPLE_VIO_DoubleSensor_Preview only supports offline!\n");
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(gs_enNorm, enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    /******************************************
     step  1: init global  variable
    ******************************************/
    gs_u32ViFrmRate = (VIDEO_ENCODING_MODE_PAL == gs_enNorm) ? 25 : 30;
    g_enVioBind = SAMPLE_BIND_VI_VPSS_VO;

    /******************************************
     step  2: mpp system init
    ******************************************/
    memset(&stVbConf, 0, sizeof(VB_CONF_S));
    stVbConf.u32MaxPoolCnt = 128;

    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize2(&stSize, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH);

    /* comm video buffer */
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt  = u32ViChnCnt * 6;

    stVbConf.astCommPool[1].u32BlkSize = u32BlkSize * 2;
    stVbConf.astCommPool[1].u32BlkCnt  = 4;

    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }

    /*************************************************
      step 1: create region and attach to vpss group
     *************************************************/
    HI_S32 u32RgnNum = 1;
    s32Ret = SAMPLE_RGN_CreateOverlayExForVpss(Handle, u32RgnNum);
    if (HI_SUCCESS != s32Ret)
    {
        printf("SAMPLE_RGN_CreateOverlayExForVpss failed! s32Ret: 0x%x.\n", s32Ret);
        return s32Ret;
    }


    /******************************************
     step  3: start VI VPSS VO
    ******************************************/
    memcpy(&stVpssAttr, &g_stVpssAttr, sizeof(SAMPLE_VPSS_ATTR_S));

    stVpssAttr.stVpssGrpAttr.u32MaxW   = stSize.u32Width;
    stVpssAttr.stVpssGrpAttr.u32MaxH   = stSize.u32Height;
    stVpssAttr.stVpssChnMode.u32Width = 1920;
    stVpssAttr.stVpssChnMode.u32Height = 1080;

    s32Ret = SAMPLE_VIO_StartViVo(pstViConfig, &stVpssAttr, &stSize, &g_stVoConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_VIO_StartViVo failed witfh %d\n", s32Ret);
        goto exit;
    }

    /*************************************************
      step 7: get time and load bitmap to region
     *************************************************/
	pthread_mutex_t mutex;
	pthread_mutex_init(&mutex, NULL);
	
	if (0 != pthread_create(&gs_TimePid, NULL, OSD_TIME, NULL))
	{
		printf("GET_TIME thread failed!\n");
	}

	if (0 != pthread_create(&gs_OsdPid, NULL, OSD_OSD, NULL))
	{
		printf("GET_TIME thread failed!\n");
	}

	if (0 != pthread_create(&gs_ZoomPid, NULL, Zoom, NULL))
	{
		printf("GET_BMP thread failed!\n");
	}

	PTHREAD_HIFB_SAMPLE_INFO stInfo0;
    stInfo0.layer   =  0;
    stInfo0.fd      = -1;
    stInfo0.ctrlkey =  1;
    if (0 != pthread_create(&g_stHifbThread, 0, SAMPLE_HIFB_REFRESH, (void*)(&stInfo0)))
    {
        SAMPLE_PRT("start hifb thread failed!\n");
    }

    /******************************************
     step  4: change stitch blend mode and param
    ******************************************/
	char m; 

	while(1)
	{
		printf("\n---------s(切换) z(放大) q(退出)---------\n");

		do {
			system("stty raw");
			m = getchar();
			system("stty -raw");
		} while ((m != 's') && (m != 'z') && (m != 'q'));

		if (m == 's')
		{
			char w;
			printf("\n----------------切换画面-----------------\n");
			while(1)
			{
				w = getchar();
				if(w == 'q')
				{
					printf("\n--------------退出切换画面---------------\n");
					break;
				}
				SAMPLE_COMM_VO_UNBindVpss(0, 0, 0, 1);
				SAMPLE_COMM_VO_UNBindVpss(0, 1, 1, 1);
				SAMPLE_COMM_VO_BindVpss(0, 0, 1, 1);   // VPSS --> VO
				SAMPLE_COMM_VO_BindVpss(0, 1, 0, 1);
				printf("----------------SUCCESS------------------\n");
				w = getchar();
				if(w == 'q')
				{
					printf("\n--------------退出切换画面--------------- \n");
					break;
				}
				SAMPLE_COMM_VO_UNBindVpss(0, 0, 1, 1);
				SAMPLE_COMM_VO_UNBindVpss(0, 1, 0, 1);
				SAMPLE_COMM_VO_BindVpss(0, 0, 0, 1);   // VPSS --> VO
				SAMPLE_COMM_VO_BindVpss(0, 1, 1, 1);
				printf("----------------SUCCESS------------------\n");
			}
		}

		else if (m == 'z')
		{
			printf("\n----------------放大画面-----------------\n");
			printf("\n-----坐标(X Y Width Height) q(退出)------\n");
			char b[4][5], a[20], c='q';
			while(1)
			{
				int i = 0;

				gets(a);
				char* token = strtok(a," ");
				while(token != NULL)
				{				
					strcpy(b[i++],token);
					token = strtok(NULL," ");
				}
				printf("%s-%s-%s-%s \n",b[0],b[1],b[2],b[3]);

				if(b[0][0] == c)
				{
					printf("\n--------------退出放大画面--------------- \n");
					break;
				}
				
				stZoom.enZoomType = VOU_ZOOM_IN_RECT;
				stZoom.stZoomRect.s32X = atoi(b[0]);
				stZoom.stZoomRect.s32Y = atoi(b[1]);
				stZoom.stZoomRect.u32Width = atoi(b[2]);
				stZoom.stZoomRect.u32Height = atoi(b[3]);	

			}

		}

		else if (m == 'q')
		{
			SAMPLE_VIO_StopViVo(pstViConfig, &stVpssAttr);
			SAMPLE_COMM_SYS_Exit();
			return 0;
		}	

	}

exit:
    return s32Ret;
}



/******************************************************************************
* function    : main()
* Description : video preview sample
******************************************************************************/

int main(int argc, char* argv[])
{
    HI_S32 s32Ret = HI_FAILURE;

    signal(SIGINT, SAMPLE_VIO_HandleSig);
    signal(SIGTERM, SAMPLE_VIO_HandleSig);

    g_stViChnConfig.enViMode = SENSOR_TYPE;
    g_stViChnConfig.enSnsNum = SAMPLE_SENSOR_DOUBLE;   //SAMPLE_SENSOR_SINGLE;
    SAMPLE_COMM_VI_GetSizeBySensor(&g_enPicSize);

    s32Ret = SAMPLE_VIO_DoubleSensor_Preview(&g_stViChnConfig);

    SAMPLE_VIO_ResetGrobalVariable();

    if (HI_SUCCESS == s32Ret)
    {
        SAMPLE_PRT("program exit normally!\n");
    }
    else
    {
        SAMPLE_PRT("program exit abnormally!\n");
    }
    exit(s32Ret);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


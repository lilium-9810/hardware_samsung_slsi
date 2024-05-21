/*
**
** Copyright 2013, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraParameters"
#include <cutils/log.h>

#include "ExynosCameraParameters.h"

namespace android {

ExynosCameraParameters::ExynosCameraParameters(int cameraId, __unused bool flagCompanion, int halVersion)
{
    m_cameraId = cameraId;
    m_halVersion = halVersion;

    const char *myName = (m_cameraId == CAMERA_ID_BACK) ? "ParametersBack" : "ParametersFront";
    strncpy(m_name, myName,  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_staticInfo = createSensorInfo(cameraId);
    m_useSizeTable = (m_staticInfo->sizeTableSupport) ? USE_CAMERA_SIZE_TABLE : false;
    m_useAdaptiveCSCRecording = (cameraId == CAMERA_ID_BACK) ? USE_ADAPTIVE_CSC_RECORDING : false;

    m_exynosconfig = NULL;
    m_activityControl = new ExynosCameraActivityControl(m_cameraId);

    memset(&m_cameraInfo, 0, sizeof(struct exynos_camera_info));
    memset(&m_exifInfo, 0, sizeof(m_exifInfo));

    m_initMetadata();

    m_setExifFixedAttribute();
#ifdef SAMSUNG_DNG
    m_setDngFixedAttribute();
#endif
    m_exynosconfig = new ExynosConfigInfo();

    mDebugInfo.num_of_appmarker = 1; /* Default : APP4 */
    mDebugInfo.idx[0][0] = 4; /* matching the app marker 4 */

#ifdef SAMSUNG_OIS
    if (cameraId == CAMERA_ID_BACK) {
        mDebugInfo.debugSize[4]  = sizeof(struct camera2_udm) + sizeof(struct ois_exif_data);
    } else {
        mDebugInfo.debugSize[4] = sizeof(struct camera2_udm);
    }
#else
    mDebugInfo.debugSize[4] = sizeof(struct camera2_udm);
#endif

#ifdef SAMSUNG_BD
    mDebugInfo.debugSize[4] += sizeof(struct bd_exif_data);
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    mDebugInfo.debugSize[4] += sizeof(struct lls_exif_data);
#endif

    mDebugInfo.debugData[4] = new char[mDebugInfo.debugSize[4]];
    memset((void *)mDebugInfo.debugData[4], 0, mDebugInfo.debugSize[4]);
    memset((void *)m_exynosconfig, 0x00, sizeof(struct ExynosConfigInfo));

#ifdef SAMSUNG_UTC_TS
    mDebugInfo.num_of_appmarker++;
    mDebugInfo.idx[1][0] = 5; /* mathcing the app marker 5 */
    mDebugInfo.debugSize[5] = sizeof(struct utc_ts);
    mDebugInfo.debugData[5] = new char[mDebugInfo.debugSize[5]];
    memset((void *)mDebugInfo.debugData[5], 0, mDebugInfo.debugSize[5]);
#endif

    // CAUTION!! : Initial values must be prior to setDefaultParameter() function.
    // Initial Values : START
#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
    m_romReadThreadDone = false;
    m_use_companion = flagCompanion;
#endif
    m_IsThumbnailCallbackOn = false;
    m_fastFpsMode = 0;
    m_previewRunning = false;
    m_previewSizeChanged = false;
    m_pictureRunning = false;
    m_recordingRunning = false;
    m_flagRestartPreviewChecked = false;
    m_flagRestartPreview = false;
    m_reallocBuffer = false;
    m_setFocusmodeSetting = false;
    m_flagMeteringRegionChanged = false;
    m_flagCheckDualMode = false;
    m_flagHWVDisMode = false;
    m_flagVideoStabilization = false;
    m_flag3dnrMode = false;

    m_flagCheck3aaIspOtf = false;
    m_flag3aaIspOtf = false;

    m_flagCheckIspTpuOtf = false;
    m_flagIspTpuOtf = false;

#ifdef LLS_CAPTURE
    m_flagLLSOn = false;
    m_LLSCaptureOn = false;
    m_LLSValue = 0;
#endif
#ifdef SR_CAPTURE
    m_flagSRSOn = false;
#endif
#ifdef OIS_CAPTURE
    m_flagOISCaptureOn = false;
#endif
#ifdef SAMSUNG_DNG
    m_flagDNGCaptureOn = false;
    m_use_DNGCapture = false;
    m_flagMultiFrame = false;
    m_isUsefulDngInfo = false;
#endif

    m_flagCheckRecordingHint = false;
    m_zoomWithScaler = false;
#ifdef SAMSUNG_HRM
    m_flagSensorHRM_Hint = false;
#endif
#ifdef SAMSUNG_LIGHT_IR
    m_flagSensorLight_IR_Hint = false;
#endif
#ifdef SAMSUNG_GYRO
    m_flagSensorGyroHint = false;
    /* Initialize gyro value for inital-calibration */
    m_metadata.shot.uctl.aaUd.gyroInfo.x = 1234;
    m_metadata.shot.uctl.aaUd.gyroInfo.y = 1234;
    m_metadata.shot.uctl.aaUd.gyroInfo.z = 1234;
#endif

    m_useDynamicBayer = (cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER : USE_DYNAMIC_BAYER_FRONT;
    m_useDynamicBayerVideoSnapShot =
        (cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_BAYER_VIDEO_SNAP_SHOT : USE_DYNAMIC_BAYER_VIDEO_SNAP_SHOT_FRONT;
    m_useDynamicScc = (cameraId == CAMERA_ID_BACK) ? USE_DYNAMIC_SCC_REAR : USE_DYNAMIC_SCC_FRONT;
#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
    m_useFastenAeStable = isFastenAeStable(cameraId, m_use_companion);
#else
    m_useFastenAeStable = isFastenAeStable(cameraId, false);
#endif

    /* we cannot know now, whether recording mode or not */
    /*
    if (getRecordingHint() == true || getDualRecordingHint() == true)
        m_usePureBayerReprocessing = (cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_RECORDING : USE_PURE_BAYER_REPROCESSING_FRONT_ON_RECORDING;
    else
    */
     m_usePureBayerReprocessing = (cameraId == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING : USE_PURE_BAYER_REPROCESSING_FRONT;

#ifdef SAMSUNG_LLV
    m_isLLVOn = true;
#endif
#ifdef SAMSUNG_OT
    m_startObjectTracking = false;
    m_objectTrackingAreaChanged = false;
    m_objectTrackingGet = false;
    int maxNumFocusAreas = getMaxNumFocusAreas();
    m_objectTrackingArea = new ExynosRect2[maxNumFocusAreas];
    m_objectTrackingWeight = new int[maxNumFocusAreas];
#endif
#ifdef SAMSUNG_LBP
    m_useBestPic = USE_LIVE_BEST_PIC;
#endif
#ifdef SAMSUNG_JQ
    m_isJpegQtableOn = false;
    m_jpegQtableStatus = JPEG_QTABLE_DEINIT;
#endif
    m_enabledMsgType = 0;

    m_previewBufferCount = NUM_PREVIEW_BUFFERS;

    m_dvfsLock = false;

#ifdef SAMSUNG_DOF
    m_curLensStep = 0;
    m_curLensCount = 0;
#endif
#ifdef USE_BINNING_MODE
    m_binningProperty = checkProperty(false);
#endif
#ifdef SAMSUNG_OIS
    m_oisNode = NULL;
    m_setOISmodeSetting = false;
#ifdef OIS_CAPTURE
    m_llsValue = 0;
#endif
#endif
    m_zoom_activated = false;
#ifdef FORCE_CAL_RELOAD
    m_calValid = true;
#endif
#ifdef USE_FADE_IN_ENTRANCE
    m_flagFirstEntrance = false;
#endif
    m_exposureTimeCapture = 0;

#ifdef SAMSUNG_LLS_DEBLUR
    m_flagLDCaptureMode = 0;
    m_LDCaptureCount = 0;
#endif

    // Initial Values : END
    setDefaultCameraInfo();
    setDefaultParameter();
}

ExynosCameraParameters::~ExynosCameraParameters()
{
    if (m_staticInfo != NULL) {
        delete m_staticInfo;
        m_staticInfo = NULL;
    }

    if (m_activityControl != NULL) {
        delete m_activityControl;
        m_activityControl = NULL;
    }

    for(int i = 0; i < mDebugInfo.num_of_appmarker; i++) {
        if(mDebugInfo.debugData[mDebugInfo.idx[i][0]])
                delete mDebugInfo.debugData[mDebugInfo.idx[i][0]];
        mDebugInfo.debugData[mDebugInfo.idx[i][0]] = NULL;
        mDebugInfo.debugSize[mDebugInfo.idx[i][0]] = 0;
    }

    if (m_exynosconfig != NULL) {
        memset((void *)m_exynosconfig, 0x00, sizeof(struct ExynosConfigInfo));
        delete m_exynosconfig;
        m_exynosconfig = NULL;
    }

    if (m_exifInfo.maker_note) {
        delete m_exifInfo.maker_note;
        m_exifInfo.maker_note = NULL;
    }

    if (m_exifInfo.user_comment) {
        delete m_exifInfo.user_comment;
        m_exifInfo.user_comment = NULL;
    }
#ifdef SAMSUNG_OT
    if (m_objectTrackingArea != NULL)
        delete[] m_objectTrackingArea;
    if (m_objectTrackingWeight != NULL)
        delete[] m_objectTrackingWeight;
#endif
}

int ExynosCameraParameters::getCameraId(void)
{
    return m_cameraId;
}

status_t ExynosCameraParameters::setParameters(const CameraParameters& params)
{
    status_t ret = NO_ERROR;

#ifdef TEST_GED_HIGH_SPEED_RECORDING
    int minFpsRange = 0, maxFpsRange = 0;
    int frameRate = 0;

    params.getPreviewFpsRange(&minFpsRange, &maxFpsRange);
    frameRate = params.getPreviewFrameRate();
    CLOGD("DEBUG(%s[%d]):getFastFpsMode=%d, maxFpsRange=%d, frameRate=%d",
        __FUNCTION__, __LINE__, getFastFpsMode(), maxFpsRange, frameRate);
    if (frameRate == 60) {
        setFastFpsMode(1);
    } else if (frameRate == 120) {
        setFastFpsMode(2);
    } else if (frameRate == 240) {
        setFastFpsMode(3);
    } else {
        setFastFpsMode(0);
    }

    CLOGD("DEBUG(%s[%d]):getFastFpsMode=%d", __FUNCTION__, __LINE__, getFastFpsMode());
#endif
#ifdef USE_FADE_IN_ENTRANCE
    if (checkFirstEntrance(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkFirstEntrance faild", __FUNCTION__, __LINE__);
    }
#endif
    /* Return OK means that the vision mode is enabled */
    if (checkVisionMode(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkVisionMode fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (getVisionMode() == true) {
        CLOGD("DEBUG(%s[%d]): Vision mode enabled", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    if (checkRecordingHint(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkRecordingHint fail", __FUNCTION__, __LINE__);

    if (checkDualMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkDualMode fail", __FUNCTION__, __LINE__);

    if (checkDualRecordingHint(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkDualRecordingHint fail", __FUNCTION__, __LINE__);

    if (checkEffectHint(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkEffectHint fail", __FUNCTION__, __LINE__);

    if (checkEffectRecordingHint(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkEffectRecordingHint fail", __FUNCTION__, __LINE__);

    if (checkPreviewFps(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkPreviewFps fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (getRecordingRunning() == false) {
        if (checkVideoSize(params) != NO_ERROR)
            CLOGE("ERR(%s[%d]): checkVideoSize fail", __FUNCTION__, __LINE__);
    }

    if (getCameraId() == CAMERA_ID_BACK) {
        if (checkFastFpsMode(params) != NO_ERROR)
            CLOGE("ERR(%s[%d]): checkFastFpsMode fail", __FUNCTION__, __LINE__);
    }

    if (checkVideoStabilization(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkVideoStabilization fail", __FUNCTION__, __LINE__);

    if (checkSWVdisMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkSWVdisMode fail", __FUNCTION__, __LINE__);

    bool swVdisUIMode = false;
#ifdef SUPPORT_SW_VDIS
    swVdisUIMode = getVideoStabilization();
#endif /*SUPPORT_SW_VDIS*/
    m_setSWVdisUIMode(swVdisUIMode);

    if (checkVtMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkVtMode fail", __FUNCTION__, __LINE__);

    if (checkHWVdisMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkHWVdisMode fail", __FUNCTION__, __LINE__);

    if (check3dnrMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): check3dnrMode fail", __FUNCTION__, __LINE__);

    if (checkDrcMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkDrcMode fail", __FUNCTION__, __LINE__);

    if (checkOdcMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkOdcMode fail", __FUNCTION__, __LINE__);

    if (checkPreviewSize(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkPreviewSize fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkPreviewFormat(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkPreviewFormat fail", __FUNCTION__, __LINE__);

    if (checkPictureSize(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkPictureSize fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkPictureFormat(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkPictureFormat fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkJpegQuality(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkJpegQuality fail", __FUNCTION__, __LINE__);

    if (checkThumbnailSize(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkThumbnailSize fail", __FUNCTION__, __LINE__);

    if (checkThumbnailQuality(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkThumbnailQuality fail", __FUNCTION__, __LINE__);

    if (checkZoomLevel(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkZoomLevel fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkRotation(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkRotation fail", __FUNCTION__, __LINE__);

    if (checkAutoExposureLock(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkAutoExposureLock fail", __FUNCTION__, __LINE__);

    ret = checkExposureCompensation(params);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkExposureCompensation fail", __FUNCTION__, __LINE__);
        return ret;
    }

    if (checkExposureTime(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkExposureTime fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkMeteringMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkMeteringMode fail", __FUNCTION__, __LINE__);

    if (checkMeteringAreas(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkMeteringAreas fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkAntibanding(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkAntibanding fail", __FUNCTION__, __LINE__);

    if (checkSceneMode(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkSceneMode fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkFocusMode(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkFocusMode fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkFlashMode(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkFlashMode fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkWhiteBalanceMode(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkWhiteBalanceMode fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkAutoWhiteBalanceLock(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkAutoWhiteBalanceLock fail", __FUNCTION__, __LINE__);

#ifdef SAMSUNG_FOOD_MODE
    if (checkWbLevel(params) != NO_ERROR) {
        ALOGE("ERR(%s[%d]): checkWbLevel fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }
#endif

    if (checkWbKelvin(params) != NO_ERROR) {
        ALOGE("ERR(%s[%d]): checkWbKelvin fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkFocusAreas(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkFocusAreas fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

#ifdef SAMSUNG_MANUAL_FOCUS
    if (checkFocusDistance(params) != NO_ERROR) {
        ALOGE("ERR(%s[%d]): checkFocusDistance fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }
#endif

    if (checkColorEffectMode(params) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): checkColorEffectMode fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if (checkGpsAltitude(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkGpsAltitude fail", __FUNCTION__, __LINE__);

    if (checkGpsLatitude(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkGpsLatitude fail", __FUNCTION__, __LINE__);

    if (checkGpsLongitude(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkGpsLongitude fail", __FUNCTION__, __LINE__);

    if (checkGpsProcessingMethod(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkGpsProcessingMethod fail", __FUNCTION__, __LINE__);

    if (checkGpsTimeStamp(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkGpsTimeStamp fail", __FUNCTION__, __LINE__);

#if 0
    if (checkCityId(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkCityId fail", __FUNCTION__, __LINE__);

    if (checkWeatherId(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkWeatherId fail", __FUNCTION__, __LINE__);
#endif

    if (checkBrightness(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkBrightness fail", __FUNCTION__, __LINE__);

    if (checkSaturation(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkSaturation fail", __FUNCTION__, __LINE__);

    if (checkSharpness(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkSharpness fail", __FUNCTION__, __LINE__);

    if (checkHue(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkHue fail", __FUNCTION__, __LINE__);

#ifdef SAMSUNG_COMPANION
    if (getSceneMode() != SCENE_MODE_HDR) {
        if (checkRTDrc(params) != NO_ERROR)
            CLOGE("ERR(%s[%d]): checkRTDrc fail", __FUNCTION__, __LINE__);
    }

    if (checkPaf(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkPaf fail", __FUNCTION__, __LINE__);

    if (getSceneMode() != SCENE_MODE_HDR) {
        if (checkRTHdr(params) != NO_ERROR)
            CLOGE("ERR(%s[%d]): checkRTHdr fail", __FUNCTION__, __LINE__);
    }
#endif

    if (checkIso(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkIso fail", __FUNCTION__, __LINE__);

    if (checkContrast(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkContrast fail", __FUNCTION__, __LINE__);

    if (checkHdrMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkHdrMode fail", __FUNCTION__, __LINE__);

    if (checkWdrMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkWdrMode fail", __FUNCTION__, __LINE__);

    if (checkShotMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkShotMode fail", __FUNCTION__, __LINE__);

    if (checkAntiShake(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkAntiShake fail", __FUNCTION__, __LINE__);

    if (checkVRMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkVRMode fail", __FUNCTION__, __LINE__);

    if (checkGamma(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkGamma fail", __FUNCTION__, __LINE__);

    if (checkSlowAe(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkSlowAe fail", __FUNCTION__, __LINE__);

    if (checkScalableSensorMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkScalableSensorMode fail", __FUNCTION__, __LINE__);

    if (checkImageUniqueId(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkImageUniqueId fail", __FUNCTION__, __LINE__);

    if (checkSeriesShotMode(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkSeriesShotMode fail", __FUNCTION__, __LINE__);

#ifdef BURST_CAPTURE
    if (checkSeriesShotFilePath(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkSeriesShotFilePath fail", __FUNCTION__, __LINE__);
#endif

#ifdef SAMSUNG_DNG
    if (checkDngFilePath(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkDngFilePath fail", __FUNCTION__, __LINE__);
#endif

#ifdef SAMSUNG_LLV
    if (checkLLV(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkLLV fail", __FUNCTION__, __LINE__);
#endif

#ifdef SAMSUNG_DOF
    if (checkMoveLens(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkMoveLens fail", __FUNCTION__, __LINE__);
#endif

#ifdef SAMSUNG_OIS
    if (checkOIS(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkOIS fail", __FUNCTION__, __LINE__);
#endif

    if (m_getRestartPreviewChecked() == true) {
        CLOGD("DEBUG(%s[%d]):Need restart preview", __FUNCTION__, __LINE__);
        m_setRestartPreview(m_flagRestartPreviewChecked);
    }

    if (checkSetfileYuvRange() != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkSetfileYuvRange fail", __FUNCTION__, __LINE__);

#ifdef SAMSUNG_QUICKSHOT
    if (checkQuickShot(params) != NO_ERROR)
        CLOGE("ERR(%s[%d]): checkQuichShot fail", __FUNCTION__, __LINE__);
#endif

    checkHorizontalViewAngle();

    return ret;
}

CameraParameters ExynosCameraParameters::getParameters() const
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    return m_params;
}

void ExynosCameraParameters::setDefaultCameraInfo(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    m_setHwSensorSize(m_staticInfo->maxSensorW, m_staticInfo->maxSensorH);
    m_setHwPreviewSize(m_staticInfo->maxPreviewW, m_staticInfo->maxPreviewH);
    m_setHwPictureSize(m_staticInfo->maxPictureW, m_staticInfo->maxPictureH);

    /* Initalize BNS scale ratio, step:500, ex)1500->x1.5 scale down */
    m_setBnsScaleRatio(1000);
    /* Initalize Binning scale ratio */
    m_setBinningScaleRatio(1000);
    /* Set Default VideoSize to FHD */
    m_setVideoSize(1920,1080);
}

void ExynosCameraParameters::setDefaultParameter(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;
    CameraParameters p;
    String8 tempStr;
    char strBuf[256];

    m_cameraInfo.autoFocusMacroPosition = ExynosCameraActivityAutofocus::AUTOFOCUS_MACRO_POSITION_BASE;

    /* Preview Size */
    getMaxPreviewSize(&m_cameraInfo.previewW, &m_cameraInfo.previewH);
    m_setHwPreviewSize(m_cameraInfo.previewW, m_cameraInfo.previewH);

    tempStr.setTo("");
    if (getResolutionList(tempStr, m_staticInfo, &m_cameraInfo.previewW, &m_cameraInfo.previewH, MODE_PREVIEW, m_cameraId) != NO_ERROR) {
        CLOGE("ERR(%s):getResolutionList(MODE_PREVIEW) fail", __FUNCTION__);

        m_cameraInfo.previewW = 640;
        m_cameraInfo.previewH = 480;
        tempStr = String8::format("%dx%d", m_cameraInfo.previewW, m_cameraInfo.previewH);
    }

    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, tempStr.c_str());
    CLOGD("DEBUG(%s): Default preview size is %dx%d", __FUNCTION__, m_cameraInfo.previewW, m_cameraInfo.previewH);
    p.setPreviewSize(m_cameraInfo.previewW, m_cameraInfo.previewH);

    /* Preview Format */
    tempStr.setTo("");
    tempStr = String8::format("%s,%s", CameraParameters::PIXEL_FORMAT_YUV420SP, CameraParameters::PIXEL_FORMAT_YUV420P);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, tempStr);
    p.setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV420SP);


    /* Video Size */
    getMaxVideoSize(&m_cameraInfo.maxVideoW, &m_cameraInfo.maxVideoH);

    tempStr.setTo("");
    if (getResolutionList(tempStr, m_staticInfo, &m_cameraInfo.maxVideoW, &m_cameraInfo.maxVideoH, MODE_VIDEO, m_cameraId) != NO_ERROR) {
        CLOGE("ERR(%s):getResolutionList(MODE_VIDEO) fail", __FUNCTION__);

        m_cameraInfo.videoW = 640;
        m_cameraInfo.videoH = 480;
        tempStr = String8::format("%dx%d", m_cameraInfo.maxVideoW, m_cameraInfo.maxVideoH);
    }
#ifdef CAMERA_GED_FEATURE
    else {
#ifdef USE_WQHD_RECORDING
        if (m_addHiddenResolutionList(tempStr, m_staticInfo, 2560, 1440, MODE_VIDEO, m_cameraId) != NO_ERROR) {
            CLOGW("WARN(%s):getResolutionList(MODE_VIDEO) fail", __FUNCTION__);
        }
#endif
#ifdef USE_UHD_RECORDING
        if (m_addHiddenResolutionList(tempStr, m_staticInfo, 3840, 2160, MODE_VIDEO, m_cameraId) != NO_ERROR) {
            CLOGW("WARN(%s):getResolutionList(MODE_VIDEO) fail", __FUNCTION__);
        }
#endif
    }
#endif

    CLOGD("DEBUG(%s): KEY_SUPPORTED_VIDEO_SIZES %s", __FUNCTION__, tempStr.c_str());

    p.set(CameraParameters::KEY_SUPPORTED_VIDEO_SIZES, tempStr.c_str());

    CLOGD("DEBUG(%s): Max video size is %dx%d", __FUNCTION__, m_cameraInfo.maxVideoW, m_cameraInfo.maxVideoH);
    CLOGD("DEBUG(%s): Default video size is %dx%d", __FUNCTION__, m_cameraInfo.videoW, m_cameraInfo.videoH);
    p.setVideoSize(m_cameraInfo.videoW, m_cameraInfo.videoH);

    /* Video Format */
    if (getAdaptiveCSCRecording() == true) {
        CLOGI("INFO(%s[%d]):video_frame_foramt == YUV420SP_NV21", __FUNCTION__, __LINE__);
        p.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420SP_NV21);
    } else {
        CLOGI("INFO(%s[%d]):video_frame_foramt == YUV420SP", __FUNCTION__, __LINE__);
        p.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420SP);
    }

    /* Preferred preview size for Video */
    tempStr.setTo("");
    tempStr = String8::format("%dx%d", m_cameraInfo.previewW, m_cameraInfo.previewH);
    p.set(CameraParameters::KEY_PREFERRED_PREVIEW_SIZE_FOR_VIDEO, tempStr.c_str());

    /* Picture Size */
    getMaxPictureSize(&m_cameraInfo.pictureW, &m_cameraInfo.pictureH);

    tempStr.setTo("");
    if (getResolutionList(tempStr, m_staticInfo, &m_cameraInfo.pictureW, &m_cameraInfo.pictureH, MODE_PICTURE, m_cameraId) != NO_ERROR) {
        CLOGE("ERR(%s):m_getResolutionList(MODE_PICTURE) fail", __FUNCTION__);

        m_cameraInfo.pictureW = 640;
        m_cameraInfo.pictureW = 480;
        tempStr = String8::format("%dx%d", m_cameraInfo.pictureW, m_cameraInfo.pictureH);
    }

    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, tempStr.c_str());
    CLOGD("DEBUG(%s): Default picture size is %dx%d", __FUNCTION__, m_cameraInfo.pictureW, m_cameraInfo.pictureH);
    p.setPictureSize(m_cameraInfo.pictureW, m_cameraInfo.pictureH);

    /* Picture Format */
    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, CameraParameters::PIXEL_FORMAT_JPEG);
    p.setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);

    /* Jpeg Quality */
    p.set(CameraParameters::KEY_JPEG_QUALITY, "96"); /* maximum quality */

    /* Thumbnail Size */
    getMaxThumbnailSize(&m_cameraInfo.thumbnailW, &m_cameraInfo.thumbnailH);

    tempStr.setTo("");
    if (getResolutionList(tempStr, m_staticInfo, &m_cameraInfo.thumbnailW, &m_cameraInfo.thumbnailH, MODE_THUMBNAIL, m_cameraId) != NO_ERROR) {
        tempStr = String8::format("%dx%d", m_cameraInfo.thumbnailW, m_cameraInfo.thumbnailH);
    }
    /* 0x0 is no thumbnail mode */
    tempStr.append(",0x0");
    p.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, tempStr.c_str());
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH,  m_cameraInfo.thumbnailW);
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, m_cameraInfo.thumbnailH);

    /* Thumbnail Quality */
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "100");

    /* Exposure */
    p.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, getMinExposureCompensation());
    p.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, getMaxExposureCompensation());
    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, 0);
    p.setFloat(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, getExposureCompensationStep());

    /* Auto Exposure Lock supported */
    if (getAutoExposureLockSupported() == true)
        p.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, "true");
    else
        p.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK_SUPPORTED, "false");

    /* Face Detection */
    p.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_HW, getMaxNumDetectedFaces());
    p.set(CameraParameters::KEY_MAX_NUM_DETECTED_FACES_SW, 0);

    /* Video Sanptshot Supported */
    if (getVideoSnapshotSupported() == true)
        p.set(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED, "true");
    else
        p.set(CameraParameters::KEY_VIDEO_SNAPSHOT_SUPPORTED, "false");

    /* Video Stabilization Supported */
    if (getVideoStabilizationSupported() == true)
        p.set(CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED, "true");
    else
        p.set(CameraParameters::KEY_VIDEO_STABILIZATION_SUPPORTED, "false");

    /* Focus Mode */
    int focusMode = getSupportedFocusModes();
    tempStr.setTo("");
    if (focusMode & FOCUS_MODE_AUTO) {
        tempStr.append(CameraParameters::FOCUS_MODE_AUTO);
    } else if (focusMode & FOCUS_MODE_FIXED){
        tempStr.append(CameraParameters::FOCUS_MODE_FIXED);
    }
    if (focusMode & FOCUS_MODE_INFINITY) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FOCUS_MODE_INFINITY);
    }
    if (focusMode & FOCUS_MODE_MACRO) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FOCUS_MODE_MACRO);
    }
    if (focusMode & FOCUS_MODE_EDOF) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FOCUS_MODE_EDOF);
    }
    if (focusMode & FOCUS_MODE_CONTINUOUS_VIDEO) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO);
    }
    if (focusMode & FOCUS_MODE_CONTINUOUS_PICTURE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE);
    }
    if (focusMode & FOCUS_MODE_CONTINUOUS_PICTURE_MACRO) {
        tempStr.append(",");
        tempStr.append("continuous-picture-macro");
    }

    p.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES,
          tempStr.c_str());

    if (focusMode & FOCUS_MODE_AUTO)
        p.set(CameraParameters::KEY_FOCUS_MODE,
              CameraParameters::FOCUS_MODE_AUTO);
    else if (focusMode & FOCUS_MODE_CONTINUOUS_PICTURE)
        p.set(CameraParameters::KEY_FOCUS_MODE,
              CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE);
    else if (focusMode & FOCUS_MODE_CONTINUOUS_VIDEO)
        p.set(CameraParameters::KEY_FOCUS_MODE,
              CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO);
    else if (focusMode & FOCUS_MODE_FIXED)
        p.set(CameraParameters::KEY_FOCUS_MODE,
          CameraParameters::FOCUS_MODE_FIXED);
    else
        p.set(CameraParameters::KEY_FOCUS_MODE,
          CameraParameters::FOCUS_MODE_INFINITY);

/*TODO: This values will be changed */
#define BACK_CAMERA_AUTO_FOCUS_DISTANCES_STR       "0.10,1.20,Infinity"
#define FRONT_CAMERA_FOCUS_DISTANCES_STR           "0.20,0.25,Infinity"

#define BACK_CAMERA_MACRO_FOCUS_DISTANCES_STR      "0.10,0.20,Infinity"
#define BACK_CAMERA_INFINITY_FOCUS_DISTANCES_STR   "0.10,1.20,Infinity"

    /* Focus Distances */
    if (getCameraId() == CAMERA_ID_BACK)
        p.set(CameraParameters::KEY_FOCUS_DISTANCES,
              BACK_CAMERA_AUTO_FOCUS_DISTANCES_STR);
    else
        p.set(CameraParameters::KEY_FOCUS_DISTANCES,
              FRONT_CAMERA_FOCUS_DISTANCES_STR);

    p.set(CameraParameters::FOCUS_DISTANCE_INFINITY, "Infinity");

    /* Max number of Focus Areas */
    p.set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS, 0);
    if (focusMode & FOCUS_MODE_TOUCH) {
        p.set(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS, 1);
        p.set(CameraParameters::KEY_FOCUS_AREAS, "(0,0,0,0,0)");
    }

    /* Flash */
    int flashMode = getSupportedFlashModes();
    tempStr.setTo("");
    if (flashMode & FLASH_MODE_OFF) {
        tempStr.append(CameraParameters::FLASH_MODE_OFF);
    }
    if (flashMode & FLASH_MODE_AUTO) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FLASH_MODE_AUTO);
    }
    if (flashMode & FLASH_MODE_ON) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FLASH_MODE_ON);
    }
    if (flashMode & FLASH_MODE_RED_EYE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FLASH_MODE_RED_EYE);
    }
    if (flashMode & FLASH_MODE_TORCH) {
        tempStr.append(",");
        tempStr.append(CameraParameters::FLASH_MODE_TORCH);
    }

    p.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, tempStr.c_str());
    p.set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_OFF);

    /* scene mode */
    int sceneMode = getSupportedSceneModes();
    tempStr.setTo("");
    if (sceneMode & SCENE_MODE_AUTO) {
        tempStr.append(CameraParameters::SCENE_MODE_AUTO);
    }
    if (sceneMode & SCENE_MODE_ACTION) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_ACTION);
    }
    if (sceneMode & SCENE_MODE_PORTRAIT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_PORTRAIT);
    }
    if (sceneMode & SCENE_MODE_LANDSCAPE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_LANDSCAPE);
    }
    if (sceneMode & SCENE_MODE_NIGHT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_NIGHT);
    }
    if (sceneMode & SCENE_MODE_NIGHT_PORTRAIT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_NIGHT_PORTRAIT);
    }
    if (sceneMode & SCENE_MODE_THEATRE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_THEATRE);
    }
    if (sceneMode & SCENE_MODE_BEACH) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_BEACH);
    }
    if (sceneMode & SCENE_MODE_SNOW) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_SNOW);
    }
    if (sceneMode & SCENE_MODE_SUNSET) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_SUNSET);
    }
    if (sceneMode & SCENE_MODE_STEADYPHOTO) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_STEADYPHOTO);
    }
    if (sceneMode & SCENE_MODE_FIREWORKS) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_FIREWORKS);
    }
    if (sceneMode & SCENE_MODE_SPORTS) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_SPORTS);
    }
    if (sceneMode & SCENE_MODE_PARTY) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_PARTY);
    }
    if (sceneMode & SCENE_MODE_CANDLELIGHT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_CANDLELIGHT);
    }
#ifdef SAMSUNG_COMPANION
    if (sceneMode & SCENE_MODE_HDR) {
        tempStr.append(",");
        tempStr.append(CameraParameters::SCENE_MODE_HDR);
    }
#endif

    p.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES,
          tempStr.c_str());
    p.set(CameraParameters::KEY_SCENE_MODE,
          CameraParameters::SCENE_MODE_AUTO);

    if (getHalVersion() != IS_HAL_VER_3_2) {
        /* effect */
        int effect = getSupportedColorEffects();
        tempStr.setTo("");
        if (effect & EFFECT_NONE) {
            tempStr.append(CameraParameters::EFFECT_NONE);
        }
        if (effect & EFFECT_MONO) {
            tempStr.append(",");
            tempStr.append(CameraParameters::EFFECT_MONO);
        }
        if (effect & EFFECT_NEGATIVE) {
            tempStr.append(",");
            tempStr.append(CameraParameters::EFFECT_NEGATIVE);
        }
        if (effect & EFFECT_SOLARIZE) {
            tempStr.append(",");
            tempStr.append(CameraParameters::EFFECT_SOLARIZE);
        }
        if (effect & EFFECT_SEPIA) {
            tempStr.append(",");
            tempStr.append(CameraParameters::EFFECT_SEPIA);
        }
        if (effect & EFFECT_POSTERIZE) {
            tempStr.append(",");
            tempStr.append(CameraParameters::EFFECT_POSTERIZE);
        }
        if (effect & EFFECT_WHITEBOARD) {
            tempStr.append(",");
            tempStr.append(CameraParameters::EFFECT_WHITEBOARD);
        }
        if (effect & EFFECT_BLACKBOARD) {
            tempStr.append(",");
            tempStr.append(CameraParameters::EFFECT_BLACKBOARD);
        }
        if (effect & EFFECT_AQUA) {
            tempStr.append(",");
            tempStr.append(CameraParameters::EFFECT_AQUA);
        }

        p.set(CameraParameters::KEY_SUPPORTED_EFFECTS, tempStr.c_str());
        p.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_NONE);
    }

    /* white balance */
    int whiteBalance = getSupportedWhiteBalance();
    tempStr.setTo("");
    if (whiteBalance & WHITE_BALANCE_AUTO) {
        tempStr.append(CameraParameters::WHITE_BALANCE_AUTO);
    }
    if (whiteBalance & WHITE_BALANCE_INCANDESCENT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_INCANDESCENT);
    }
    if (whiteBalance & WHITE_BALANCE_FLUORESCENT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_FLUORESCENT);
    }
    if (whiteBalance & WHITE_BALANCE_WARM_FLUORESCENT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_WARM_FLUORESCENT);
    }
    if (whiteBalance & WHITE_BALANCE_DAYLIGHT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_DAYLIGHT);
    }
    if (whiteBalance & WHITE_BALANCE_CLOUDY_DAYLIGHT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT);
    }
    if (whiteBalance & WHITE_BALANCE_TWILIGHT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_TWILIGHT);
    }
    if (whiteBalance & WHITE_BALANCE_SHADE) {
        tempStr.append(",");
        tempStr.append(CameraParameters::WHITE_BALANCE_SHADE);
    }

    p.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE,
          tempStr.c_str());
    p.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);

    /* Auto Whitebalance Lock supported */
    if (getAutoWhiteBalanceLockSupported() == true)
        p.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, "true");
    else
        p.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK_SUPPORTED, "false");

    /*  anti banding */
    tempStr.setTo("");
    int antiBanding = getSupportedAntibanding();
#ifdef USE_CSC_FEATURE
    /* The Supported List always includes the AUTO Mode - Google */
    if (antiBanding & ANTIBANDING_AUTO) {
        tempStr.append(CameraParameters::ANTIBANDING_AUTO);
    }

    m_chooseAntiBandingFrequency();
    /* 50Hz or 60Hz */
    tempStr.append(",");
    tempStr.append(m_antiBanding);
#else

    if (antiBanding & ANTIBANDING_AUTO) {
        tempStr.append(CameraParameters::ANTIBANDING_AUTO);
    }
    if (antiBanding & ANTIBANDING_50HZ) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ANTIBANDING_50HZ);
    }
    if (antiBanding & ANTIBANDING_60HZ) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ANTIBANDING_60HZ);
    }
    if (antiBanding & ANTIBANDING_OFF) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ANTIBANDING_OFF);
    }
#endif

    p.set(CameraParameters::KEY_SUPPORTED_ANTIBANDING,
          tempStr.c_str());

#ifdef USE_CSC_FEATURE
    p.set(CameraParameters::KEY_ANTIBANDING, m_antiBanding);
#else
    p.set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_AUTO);
#endif

    /* rotation */
    p.set(CameraParameters::KEY_ROTATION, 0);

    /* view angle */
    setHorizontalViewAngle(640, 480);
    p.setFloat(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, getHorizontalViewAngle());
    p.setFloat(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, getVerticalViewAngle());

    /* metering */
    p.set(CameraParameters::KEY_MAX_NUM_METERING_AREAS, getMaxNumMeteringAreas());
    p.set(CameraParameters::KEY_METERING_AREAS, "");

    /* zoom */
    if (getZoomSupported() == true) {
        int maxZoom = getMaxZoomLevel();
        ALOGI("INFO(%s):getMaxZoomLevel(%d)", __FUNCTION__, maxZoom);

        if (0 < maxZoom) {
            p.set(CameraParameters::KEY_ZOOM_SUPPORTED, "true");

            if (getSmoothZoomSupported() == true)
                p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "true");
            else
                p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "false");

            p.set(CameraParameters::KEY_MAX_ZOOM, maxZoom - 1);
            p.set(CameraParameters::KEY_ZOOM, ZOOM_LEVEL_0);

            int max_zoom_ratio = (int)getMaxZoomRatio();
            tempStr.setTo("");
            if (getZoomRatioList(tempStr, maxZoom, max_zoom_ratio, m_staticInfo->zoomRatioList) == NO_ERROR)
                p.set(CameraParameters::KEY_ZOOM_RATIOS, tempStr.c_str());
            else
                p.set(CameraParameters::KEY_ZOOM_RATIOS, "100");

            p.set("constant-growth-rate-zoom-supported", "true");

            ALOGI("INFO(%s):maxZoom=%d, max_zoom_ratio= %d, zoomRatioList=%s", "setDefaultParameter", maxZoom, max_zoom_ratio, tempStr.c_str());
        } else {
            p.set(CameraParameters::KEY_ZOOM_SUPPORTED, "false");
            p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "false");
            p.set(CameraParameters::KEY_MAX_ZOOM, ZOOM_LEVEL_0);
            p.set(CameraParameters::KEY_ZOOM, ZOOM_LEVEL_0);
        }
    } else {
        p.set(CameraParameters::KEY_ZOOM_SUPPORTED, "false");
        p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "false");
        p.set(CameraParameters::KEY_MAX_ZOOM, ZOOM_LEVEL_0);
        p.set(CameraParameters::KEY_ZOOM, ZOOM_LEVEL_0);
    }

    /* fps */
    uint32_t minFpsRange = 15;
    uint32_t maxFpsRange = 30;

    getPreviewFpsRange(&minFpsRange, &maxFpsRange);
#ifdef TEST_GED_HIGH_SPEED_RECORDING
    maxFpsRange = 120;
#endif
    CLOGI("INFO(%s[%d]):minFpsRange=%d, maxFpsRange=%d", "getPreviewFpsRange", __LINE__, (int)minFpsRange, (int)maxFpsRange);
    int minFps = (minFpsRange == 0) ? 0 : (int)minFpsRange;
    int maxFps = (maxFpsRange == 0) ? 0 : (int)maxFpsRange;

    tempStr.setTo("");
    snprintf(strBuf, 256, "%d", minFps);
    tempStr.append(strBuf);

    for (int i = minFps + 1; i <= maxFps; i++) {
        snprintf(strBuf, 256, ",%d", i);
        tempStr.append(strBuf);
    }
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES,  tempStr.c_str());

    minFpsRange = minFpsRange * 1000;
    maxFpsRange = maxFpsRange * 1000;

    tempStr.setTo("");
    getSupportedFpsList(tempStr, minFpsRange, maxFpsRange, m_cameraId, m_staticInfo);
    CLOGI("INFO(%s):supportedFpsList=%s", "setDefaultParameter", tempStr.c_str());
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, tempStr.c_str());
    /* p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(15000,30000),(30000,30000)"); */

    /* limit 30 fps on default setting. */
    if (30 < maxFps)
        maxFps = 30;
    p.setPreviewFrameRate(maxFps);

    if (30000 < maxFpsRange)
        maxFpsRange = 30000;
    snprintf(strBuf, 256, "%d,%d", maxFpsRange/2, maxFpsRange);
    p.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, strBuf);

    /* focal length */
    int num = 0;
    int den = 0;
    int precision = 0;
    getFocalLength(&num, &den);

    switch (den) {
    default:
    case 1000:
        precision = 3;
        break;
    case 100:
        precision = 2;
        break;
    case 10:
        precision = 1;
        break;
    case 1:
        precision = 0;
        break;
    }
    snprintf(strBuf, 256, "%.*f", precision, ((float)num / (float)den));
    p.set(CameraParameters::KEY_FOCAL_LENGTH, strBuf);

    /* Additional params. */
    p.set("contrast", "auto");
    p.set("iso", "auto");

    // Set supported ISO values
    int isoValues = getSupportedISO();
    tempStr.setTo("");
    if (isoValues & ISO_AUTO) {
        tempStr.append(CameraParameters::ISO_AUTO);
    }
    if (isoValues & ISO_100) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ISO_100);
    }
    if (isoValues & ISO_200) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ISO_200);
    }
    if (isoValues & ISO_400) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ISO_400);
    }
    if (isoValues & ISO_800) {
        tempStr.append(",");
        tempStr.append(CameraParameters::ISO_800);
    }

    p.set("iso-values",
          tempStr.c_str());

    p.set("wdr", 0);
    p.set("hdr-mode", 0);
    p.set("metering", "center");    /* For Samsung SDK */

    // Set Supported Metering Mode
    int meteringModes = getSupportedMeteringMode();
    tempStr.setTo("");
    if (meteringModes & METERING_MODE_MATRIX) {
        tempStr.append(CameraParameters::METERING_MATRIX);
    }
    if (meteringModes & METERING_MODE_CENTER) {
        tempStr.append(",");
        tempStr.append(CameraParameters::METERING_CENTER);
    }
    if (meteringModes & METERING_MODE_SPOT) {
        tempStr.append(",");
        tempStr.append(CameraParameters::METERING_SPOT);
    }
#ifdef SAMSUNG_TN_FEATURE
    /* For Samsung SDK */
    p.set(CameraParameters::KEY_SUPPORTED_METERING_MODE,
        tempStr.c_str());
#endif
    p.set("brightness", 0);
    p.set("brightness-max", 2);
    p.set("brightness-min", -2);

    p.set("saturation", 0);
    p.set("saturation-max", 2);
    p.set("saturation-min", -2);

    p.set("sharpness", 0);
    p.set("sharpness-max", 2);
    p.set("sharpness-min", -2);

    p.set("hue", 0);
    p.set("hue-max", 2);
    p.set("hue-min", -2);

    /* For Series shot */
    p.set("burst-capture", 0);
    p.set("best-capture", 0);

    /* fnumber */
    getFnumber(&num, &den);
    p.set("fnumber-value-numerator", num);
    p.set("fnumber-value-denominator", den);

    /* max aperture value */
    getApertureValue(&num, &den);
    p.set("maxaperture-value-numerator", num);
    p.set("maxaperture-value-denominator", den);

    /* focal length */
    getFocalLength(&num, &den);
    p.set("focallength-value-numerator", num);
    p.set("focallength-value-denominator", den);

    /* focal length in 35mm film */
    int focalLengthIn35mmFilm = 0;
    focalLengthIn35mmFilm = getFocalLengthIn35mmFilm();
    p.set("focallength-35mm-value", focalLengthIn35mmFilm);

#if defined(USE_3RD_BLACKBOX) /* KOR ONLY */
    /* scale mode */
    bool supportedScalableMode = getSupportedScalableSensor();
    if (supportedScalableMode == true)
        p.set("scale_mode", -1);
#endif

#if defined(TEST_APP_HIGH_SPEED_RECORDING)
    p.set("fast-fps-mode", 0);
#endif

#ifdef SAMSUNG_LLV
    /* LLV */
    p.set("llv_mode", 0);
#endif

#ifdef SAMSUNG_COMPANION
    p.set(CameraParameters::KEY_DYNAMIC_RANGE_CONTROL, "off");

#ifdef SAMSUNG_TN_FEATURE
    /* PHASE_AF */
    p.set(CameraParameters::KEY_SUPPORTED_PHASE_AF, "off,on");  /* For Samsung SDK */
    p.set(CameraParameters::KEY_PHASE_AF, "off");

    /* RT_HDR */
    p.set(CameraParameters::KEY_SUPPORTED_RT_HDR, "off,on,auto"); /* For Samsung SDK */
    p.set(CameraParameters::KEY_RT_HDR, "off");
#endif
#else
    p.set(CameraParameters::KEY_DYNAMIC_RANGE_CONTROL, "off");

#ifdef SAMSUNG_TN_FEATURE
    /* PHASE_AF */
    p.set(CameraParameters::KEY_SUPPORTED_PHASE_AF, "off"); /* For Samsung SDK */
    p.set(CameraParameters::KEY_PHASE_AF, "off");

    /* RT_HDR */
    p.set(CameraParameters::KEY_SUPPORTED_RT_HDR, "off"); /* For Samsung SDK */
    p.set(CameraParameters::KEY_RT_HDR, "off");
#endif
#endif
    p.set("imageuniqueid-value", 0);

#if defined(SAMSUNG_OIS)
    p.set("ois-supported", "true");
    p.set("ois", "still");
#else
    //p.set("ois-supported", "false");
#endif

#ifdef SAMSUNG_VRMODE
    p.set("vrmode-supported", "true");
#endif

    p.set("drc", "false");
    p.set("3dnr", "false");
    p.set("odc", "false");
#ifdef USE_FADE_IN_ENTRANCE
    p.set("entrance", "false");
#endif

#ifdef SAMSUNG_MANUAL_FOCUS
    p.set("focus-distance", -1);
#endif
#ifdef SAMSUNG_QUICKSHOT
    p.set("quick-shot", 0);
#endif
    p.set("effectrecording-hint", 0);

    /* Exposure Time */
    if (getMinExposureTime() != 0 && getMaxExposureTime() != 0) {
        p.set("min-exposure-time", getMinExposureTime());
        p.set("max-exposure-time", getMaxExposureTime());
        p.set("exposure-time", 0);
    }

    /* WB Kelvin */
    if (getMinWBK() != 0 && getMaxWBK() != 0) {
        p.set("min-wb-k", getMinWBK());
        p.set("max-wb-k", getMaxWBK());
        p.set("wb-k", 0);
    }

    m_params = p;

    /* make sure m_secCamera has all the settings we do.  applications
     * aren't required to call setParameters themselves (only if they
     * want to change something.
     */
    ret = setParameters(p);
    if (ret < 0)
        CLOGE("ERR(%s[%d]):setParameters is fail", __FUNCTION__, __LINE__);
}

status_t ExynosCameraParameters::checkVisionMode(const CameraParameters& params)
{
    status_t ret;

    /* Check vision mode */
    int intelligent_mode = params.getInt("intelligent-mode");
    CLOGD("DEBUG(%s):intelligent_mode : %d", "setParameters", intelligent_mode);

    ret = m_setIntelligentMode(intelligent_mode);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s): Inavalid Intelligent mode", "setParameters");
        return ret;
    }
    m_params.set("intelligent-mode", intelligent_mode);

    CLOGD("DEBUG(%s):intelligent_mode(%d) getVisionMode(%d)", "setParameters", intelligent_mode, getVisionMode());

    /* Smart stay need to skip more frames */
    int skipCompensation = m_frameSkipCounter.getCompensation();
    if (intelligent_mode == 1) {
        m_frameSkipCounter.setCompensation(skipCompensation + SMART_STAY_SKIP_COMPENSATION);
    } else {
        m_frameSkipCounter.setCompensation(skipCompensation);
    }

    if (getVisionMode() == true) {
        /* preset for each vision mode */
        switch (intelligent_mode) {
        case 2:
            m_setVisionModeFps(10);
            break;
        case 3:
            m_setVisionModeFps(5);
            break;
        default:
            m_setVisionModeFps(10);
            break;
        }

/* Vision mode custom frame rate will be enabled when application ready */
#if 0
        /* If user wants to set custom fps, vision mode set max fps to frame rate */
        int minFps = -1;
        int maxFps = -1;
        params.getPreviewFpsRange(&minFps, &maxFps);

        if (minFps > 0 && maxFps > 0) {
            CLOGD("DEBUG(%s): set user frame rate (%d)", __FUNCTION__, maxFps / 1000);
            m_setVisionModeFps(maxFps / 1000);
        }
#endif

        /* smart-screen-exposure */
        int newVisionAeTarget = params.getInt("smart-screen-exposure");
        if (0 < newVisionAeTarget) {
            CLOGD("DEBUG(%s):newVisionAeTarget : %d", "setParameters", newVisionAeTarget);
            m_setVisionModeAeTarget(newVisionAeTarget);
            m_params.set("smart-screen-exposure", newVisionAeTarget);
        }

        return OK;
    } else {
        return NO_ERROR;
    }
}

status_t ExynosCameraParameters::m_setIntelligentMode(int intelligentMode)
{
    status_t ret = NO_ERROR;
    bool visionMode = false;

    m_cameraInfo.intelligentMode = intelligentMode;

    if (intelligentMode > 1) {
        if (m_staticInfo->visionModeSupport == true) {
            visionMode = true;
        } else {
            CLOGE("ERR(%s): tried to set vision mode(not supported)", "setParameters");
            ret = BAD_VALUE;
        }
    } else if (getVisionMode()) {
        CLOGE("ERR(%s[%d]):vision mode can not change before stoppreview", __FUNCTION__, __LINE__);
        visionMode = true;
    }

    m_setVisionMode(visionMode);

    return ret;
 }

int ExynosCameraParameters::getIntelligentMode(void)
{
    return m_cameraInfo.intelligentMode;
}

void ExynosCameraParameters::m_setVisionMode(bool vision)
{
    m_cameraInfo.visionMode = vision;
}

bool ExynosCameraParameters::getVisionMode(void)
{
    return m_cameraInfo.visionMode;
}

void ExynosCameraParameters::m_setVisionModeFps(int fps)
{
    m_cameraInfo.visionModeFps = fps;
}

int ExynosCameraParameters::getVisionModeFps(void)
{
    return m_cameraInfo.visionModeFps;
}

void ExynosCameraParameters::m_setVisionModeAeTarget(int ae)
{
    m_cameraInfo.visionModeAeTarget = ae;
}

int ExynosCameraParameters::getVisionModeAeTarget(void)
{
    return m_cameraInfo.visionModeAeTarget;
}

status_t ExynosCameraParameters::checkRecordingHint(const CameraParameters& params)
{
    /* recording hint */
    bool recordingHint = false;
    const char *newRecordingHint = params.get(CameraParameters::KEY_RECORDING_HINT);

    if (newRecordingHint != NULL) {
        CLOGD("DEBUG(%s):newRecordingHint : %s", "setParameters", newRecordingHint);

        recordingHint = (strcmp(newRecordingHint, "true") == 0) ? true : false;

        m_setRecordingHint(recordingHint);

        m_params.set(CameraParameters::KEY_RECORDING_HINT, newRecordingHint);

    } else {
        /* to confirm that recordingHint value is checked up (whatever value is) */
        m_setRecordingHint(m_cameraInfo.recordingHint);

        recordingHint = getRecordingHint();
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setRecordingHint(bool hint)
{
    m_cameraInfo.recordingHint = hint;

    if (hint) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_ON);
    } else if (!hint && !getDualRecordingHint() && !getEffectRecordingHint()) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_OFF);
    }

    /* RecordingHint is confirmed */
    m_flagCheckRecordingHint = true;
}

bool ExynosCameraParameters::getRecordingHint(void)
{
    /*
     * Before setParameters, we cannot know recordingHint is valid or not
     * So, check and make assert for fast debugging
     */
    if (m_flagCheckRecordingHint == false)
        android_printAssert(NULL, LOG_TAG, "Cannot call getRecordingHint befor setRecordingHint, assert!!!!");

    return m_cameraInfo.recordingHint;
}


status_t ExynosCameraParameters::checkDualMode(const CameraParameters& params)
{
    /* dual_mode */
    bool flagDualMode = false;
    int newDualMode = params.getInt("dual_mode");

    if (newDualMode == 1) {
        CLOGD("DEBUG(%s):newDualMode : %d", "setParameters", newDualMode);
        flagDualMode = true;
    }

    m_setDualMode(flagDualMode);
    m_params.set("dual_mode", newDualMode);

    return NO_ERROR;
}

void ExynosCameraParameters::m_setDualMode(bool dual)
{
    m_cameraInfo.dualMode = dual;
    /* dualMode is confirmed */
    m_flagCheckDualMode = true;
}

bool ExynosCameraParameters::getDualMode(void)
{
    /*
    * Before setParameters, we cannot know dualMode is valid or not
    * So, check and make assert for fast debugging
    */
    if (m_flagCheckDualMode == false)
        android_printAssert(NULL, LOG_TAG, "Cannot call getDualMode befor checkDualMode, assert!!!!");

    return m_cameraInfo.dualMode;
}

status_t ExynosCameraParameters::checkDualRecordingHint(const CameraParameters& params)
{
    /* dual recording hint */
    bool flagDualRecordingHint = false;
    int newDualRecordingHint = params.getInt("dualrecording-hint");

    if (newDualRecordingHint == 1) {
        CLOGD("DEBUG(%s):newDualRecordingHint : %d", "setParameters", newDualRecordingHint);
        flagDualRecordingHint = true;
    }

    m_setDualRecordingHint(flagDualRecordingHint);
    m_params.set("dualrecording-hint", newDualRecordingHint);

    return NO_ERROR;
}

void ExynosCameraParameters::m_setDualRecordingHint(bool hint)
{
    m_cameraInfo.dualRecordingHint = hint;

    if (hint) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_ON);
    } else if (!hint && !getRecordingHint() && !getEffectRecordingHint()) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_OFF);
    }
}

bool ExynosCameraParameters::getDualRecordingHint(void)
{
    return m_cameraInfo.dualRecordingHint;
}

status_t ExynosCameraParameters::checkEffectHint(const CameraParameters& params)
{
    /* effect hint */
    bool flagEffectHint = false;
    int newEffectHint = params.getInt("effect_hint");

    if (newEffectHint < 0)
        return NO_ERROR;

    if (newEffectHint == 1) {
        CLOGD("DEBUG(%s[%d]):newEffectHint : %d", "setParameters", __LINE__, newEffectHint);
        flagEffectHint = true;
    }

    m_setEffectHint(newEffectHint);
    m_params.set("effect_hint", newEffectHint);

    return NO_ERROR;
}

void ExynosCameraParameters::m_setEffectHint(bool hint)
{
    m_cameraInfo.effectHint = hint;
}

bool ExynosCameraParameters::getEffectHint(void)
{
    return m_cameraInfo.effectHint;
}

status_t ExynosCameraParameters::checkEffectRecordingHint(const CameraParameters& params)
{
    /* Effect recording hint */
    bool flagEffectRecordingHint = false;
    int newEffectRecordingHint = params.getInt("effectrecording-hint");
    int curEffectRecordingHint = m_params.getInt("effectrecording-hint");

    if (newEffectRecordingHint < 0) {
        CLOGV("DEBUG(%s):Invalid newEffectRecordingHint", "setParameters");
        return NO_ERROR;
    }

    if (newEffectRecordingHint != curEffectRecordingHint) {
        CLOGD("DEBUG(%s):newEffectRecordingHint : %d", "setParameters", newEffectRecordingHint);
        if (newEffectRecordingHint == 1)
            flagEffectRecordingHint = true;
        m_setEffectRecordingHint(flagEffectRecordingHint);
        m_params.set("effectrecording-hint", newEffectRecordingHint);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setEffectRecordingHint(bool hint)
{
    ExynosCameraActivityAutofocus *autoFocusMgr = m_activityControl->getAutoFocusMgr();
    ExynosCameraActivityFlash *flashMgr = m_activityControl->getFlashMgr();

    m_cameraInfo.effectRecordingHint = hint;

    if (hint) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_ON);
    } else if (!hint && !getRecordingHint() && !getDualRecordingHint()) {
        setMetaVideoMode(&m_metadata, AA_VIDEOMODE_OFF);
    }
    autoFocusMgr->setRecordingHint(hint);
    flashMgr->setRecordingHint(hint);

}

bool ExynosCameraParameters::getEffectRecordingHint(void)
{
    return m_cameraInfo.effectRecordingHint;
}

status_t ExynosCameraParameters::checkPreviewFps(const CameraParameters& params)
{
    int ret = 0;

    ret = checkPreviewFpsRange(params);
    if (ret == BAD_VALUE) {
        CLOGE("ERR(%s): Inavalid value", "setParameters");
        return ret;
    } else if (ret != NO_ERROR) {
        ret = checkPreviewFrameRate(params);
    }

    return ret;
}

status_t ExynosCameraParameters::checkPreviewFpsRange(const CameraParameters& params)
{
    int newMinFps = 0;
    int newMaxFps = 0;
    int newFrameRate = params.getPreviewFrameRate();
    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;

    params.getPreviewFpsRange(&newMinFps, &newMaxFps);
    if (newMinFps <= 0 || newMaxFps <= 0 || newMinFps > newMaxFps) {
        CLOGE("PreviewFpsRange is invalid, newMin(%d), newMax(%d)", newMinFps, newMaxFps);
        return BAD_VALUE;
    }

    ALOGI("INFO(%s):Original FpsRange[Min=%d, Max=%d]", __FUNCTION__, newMinFps, newMaxFps);

    if (m_adjustPreviewFpsRange(newMinFps, newMaxFps) != NO_ERROR) {
        CLOGE("Fail to adjust preview fps range");
        return INVALID_OPERATION;
    }

    newMinFps = newMinFps / 1000;
    newMaxFps = newMaxFps / 1000;
    if (FRAME_RATE_MAX < newMaxFps || newMaxFps < newMinFps) {
        CLOGE("PreviewFpsRange is out of bound");
        return INVALID_OPERATION;
    }

    getPreviewFpsRange(&curMinFps, &curMaxFps);
    CLOGI("INFO(%s):curFpsRange[Min=%d, Max=%d], newFpsRange[Min=%d, Max=%d], [curFrameRate=%d]",
        "checkPreviewFpsRange", curMinFps, curMaxFps, newMinFps, newMaxFps, m_params.getPreviewFrameRate());

    if (curMinFps != (uint32_t)newMinFps || curMaxFps != (uint32_t)newMaxFps) {
        m_setPreviewFpsRange((uint32_t)newMinFps, (uint32_t)newMaxFps);

        char newFpsRange[256];
        memset (newFpsRange, 0, 256);
        snprintf(newFpsRange, 256, "%d,%d", newMinFps * 1000, newMaxFps * 1000);

        CLOGI("DEBUG(%s):set PreviewFpsRange(%s)", __FUNCTION__, newFpsRange);
        CLOGI("DEBUG(%s):set PreviewFrameRate(curFps=%d->newFps=%d)", __FUNCTION__, m_params.getPreviewFrameRate(), newMaxFps);
        m_params.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, newFpsRange);
        m_params.setPreviewFrameRate(newMaxFps);
    }

    getPreviewFpsRange(&curMinFps, &curMaxFps);
    m_activityControl->setFpsValue(curMinFps);

    /* For backword competivity */
    m_params.setPreviewFrameRate(newFrameRate);

    return NO_ERROR;
}

status_t ExynosCameraParameters::m_adjustPreviewFpsRange(int &newMinFps, int &newMaxFps)
{
    bool flagSpecialMode = false;
    int curSceneMode = 0;
    int curShotMode = 0;

    if (getDualMode() == true) {
        flagSpecialMode = true;

        /* when dual mode, fps is limited by 24fps */
        if (24000 < newMaxFps)
            newMaxFps = 24000;

        /* set fixed fps. */
        newMinFps = newMaxFps;
        ALOGD("DEBUG(%s[%d]):dualMode(true), newMaxFps=%d", __FUNCTION__, __LINE__, newMaxFps);
    }

    if (getDualRecordingHint() == true) {
        flagSpecialMode = true;

        /* when dual recording mode, fps is limited by 24fps */
        if (24000 < newMaxFps)
            newMaxFps = 24000;

        /* set fixed fps. */
        newMinFps = newMaxFps;
        ALOGD("DEBUG(%s[%d]):dualRecordingHint(true), newMaxFps=%d", __FUNCTION__, __LINE__, newMaxFps);
    }

    if (getEffectHint() == true) {
        flagSpecialMode = true;
#if 0   /* Don't use to set fixed fps in the hal side. */
        /* when effect mode, fps is limited by 24fps */
        if (24000 < newMaxFps)
            newMaxFps = 24000;

        /* set fixed fps due to GPU preformance. */
        newMinFps = newMaxFps;
#endif
        ALOGD("DEBUG(%s[%d]):effectHint(true), newMaxFps=%d", __FUNCTION__, __LINE__, newMaxFps);
    }

    if (getRecordingHint() == true) {
        flagSpecialMode = true;
#if 0   /* Don't use to set fixed fps in the hal side. */
#ifdef USE_VARIABLE_FPS_OF_FRONT_RECORDING
        if (getCameraId() == CAMERA_ID_FRONT && getSamsungCamera() == true) {
            /* Supported the variable frame rate for Image Quality Performacne */
            ALOGD("DEBUG(%s[%d]):RecordingHint(true),newMinFps=%d,newMaxFps=%d", __FUNCTION__, __LINE__, newMinFps, newMaxFps);
        } else
#endif
        {
            /* set fixed fps. */
            newMinFps = newMaxFps;
        }
        ALOGD("DEBUG(%s[%d]):RecordingHint(true), newMaxFps=%d", __FUNCTION__, __LINE__, newMaxFps);
#endif
        ALOGD("DEBUG(%s[%d]):RecordingHint(true),newMinFps=%d,newMaxFps=%d", __FUNCTION__, __LINE__, newMinFps, newMaxFps);
    }

    if (flagSpecialMode == true) {
        CLOGD("DEBUG(%s[%d]):special mode enabled, newMaxFps=%d", __FUNCTION__, __LINE__, newMaxFps);
        goto done;
    }

    curSceneMode = getSceneMode();
    switch (curSceneMode) {
    case SCENE_MODE_ACTION:
        if (getHighSpeedRecording() == true){
            newMinFps = newMaxFps;
        } else {
            newMinFps = 30000;
            newMaxFps = 30000;
        }
        break;
    case SCENE_MODE_PORTRAIT:
    case SCENE_MODE_LANDSCAPE:
        if (getHighSpeedRecording() == true){
            newMinFps = newMaxFps / 2;
        } else {
            newMinFps = 15000;
            newMaxFps = 30000;
        }
        break;
    case SCENE_MODE_NIGHT:
        /* for Front MMS mode FPS */
        if (getCameraId() == CAMERA_ID_FRONT && getRecordingHint() == true)
            break;

        if (getHighSpeedRecording() == true){
            newMinFps = newMaxFps / 4;
        } else {
            newMinFps = 8000;
            newMaxFps = 30000;
        }
        break;
    case SCENE_MODE_NIGHT_PORTRAIT:
    case SCENE_MODE_THEATRE:
    case SCENE_MODE_BEACH:
    case SCENE_MODE_SNOW:
    case SCENE_MODE_SUNSET:
    case SCENE_MODE_STEADYPHOTO:
    case SCENE_MODE_FIREWORKS:
    case SCENE_MODE_SPORTS:
    case SCENE_MODE_PARTY:
    case SCENE_MODE_CANDLELIGHT:
        if (getHighSpeedRecording() == true){
            newMinFps = newMaxFps / 2;
        } else {
            newMinFps = 15000;
            newMaxFps = 30000;
        }
        break;
    default:
        break;
    }

    curShotMode = getShotMode();
    switch (curShotMode) {
    case SHOT_MODE_DRAMA:
    case SHOT_MODE_3DTOUR:
    case SHOT_MODE_3D_PANORAMA:
    case SHOT_MODE_LIGHT_TRACE:
        newMinFps = 30000;
        newMaxFps = 30000;
        break;
    case SHOT_MODE_ANIMATED_SCENE:
        newMinFps = 15000;
        newMaxFps = 15000;
        break;
#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    case THIRD_PARTY_BLACKBOX_MODE:
        ALOGI("INFO(%s): limit the maximum 30 fps range in THIRD_PARTY_BLACKBOX_MODE(%d,%d)", __FUNCTION__, newMinFps, newMaxFps);
        if (newMinFps > 30000) {
            newMinFps = 30000;
        }
        if (newMaxFps > 30000) {
            newMaxFps = 30000;
        }
        break;
    case THIRD_PARTY_VTCALL_MODE:
        ALOGI("INFO(%s): limit the maximum 15 fps range in THIRD_PARTY_VTCALL_MODE(%d,%d)", __FUNCTION__, newMinFps, newMaxFps);
        if (newMinFps > 15000) {
            newMinFps = 15000;
        }
        if (newMaxFps > 15000) {
            newMaxFps = 15000;
        }
        break;
    case THIRD_PARTY_HANGOUT_MODE:
        ALOGI("INFO(%s): change fps range 15000,15000 in THIRD_PARTY_HANGOUT_MODE", __FUNCTION__);
        newMinFps = 15000;
        newMaxFps = 15000;
        break;
#endif
    default:
        break;
    }

done:
    if (newMinFps != newMaxFps) {
        if (m_getSupportedVariableFpsList(newMinFps, newMaxFps, &newMinFps, &newMaxFps) == false)
            newMinFps = newMaxFps / 2;
    }

    return NO_ERROR;
}

void ExynosCameraParameters::updatePreviewFpsRange(void)
{
    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;
    int newMinFps = 0;
    int newMaxFps = 0;

    getPreviewFpsRange(&curMinFps, &curMaxFps);
    newMinFps = curMinFps * 1000;
    newMaxFps = curMaxFps * 1000;

    if (m_adjustPreviewFpsRange(newMinFps, newMaxFps) != NO_ERROR) {
        CLOGE("Fils to adjust preview fps range");
        return;
    }

    newMinFps = newMinFps / 1000;
    newMaxFps = newMaxFps / 1000;

    if (curMinFps != (uint32_t)newMinFps || curMaxFps != (uint32_t)newMaxFps) {
        m_setPreviewFpsRange((uint32_t)newMinFps, (uint32_t)newMaxFps);
    }
}

status_t ExynosCameraParameters::checkPreviewFrameRate(const CameraParameters& params)
{
    int newFrameRate = params.getPreviewFrameRate();
    int curFrameRate = m_params.getPreviewFrameRate();
    int newMinFps = 0;
    int newMaxFps = 0;
    int tempFps = 0;

    if (newFrameRate < 0) {
        return BAD_VALUE;
    }
    CLOGD("DEBUG(%s):curFrameRate=%d, newFrameRate=%d", __FUNCTION__, curFrameRate, newFrameRate);
    if (newFrameRate != curFrameRate) {
        tempFps = newFrameRate * 1000;

        if (m_getSupportedVariableFpsList(tempFps / 2, tempFps, &newMinFps, &newMaxFps) == false) {
            newMinFps = tempFps / 2;
            newMaxFps = tempFps;
        }

        char newFpsRange[256];
        memset (newFpsRange, 0, 256);
        snprintf(newFpsRange, 256, "%d,%d", newMinFps, newMaxFps);
        m_params.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, newFpsRange);

        if (checkPreviewFpsRange(m_params) == true) {
            m_params.setPreviewFrameRate(newFrameRate);
            CLOGD("DEBUG(%s):setPreviewFrameRate(newFrameRate=%d)", __FUNCTION__, newFrameRate);
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraParameters::checkPreviewFpsRange(uint32_t minFps, uint32_t maxFps)
{
    status_t ret = NO_ERROR;
    uint32_t curMinFps = 0, curMaxFps = 0;

    getPreviewFpsRange(&curMinFps, &curMaxFps);
    if (curMinFps != minFps || curMaxFps != maxFps)
        m_setPreviewFpsRange(minFps, maxFps);

    return ret;
}

void ExynosCameraParameters::m_setPreviewFpsRange(uint32_t min, uint32_t max)
{
    setMetaCtlAeTargetFpsRange(&m_metadata, min, max);
    setMetaCtlSensorFrameDuration(&m_metadata, (uint64_t)((1000 * 1000 * 1000) / (uint64_t)max));

    ALOGI("INFO(%s):fps min(%d) max(%d)", __FUNCTION__, min, max);
}

void ExynosCameraParameters::getPreviewFpsRange(uint32_t *min, uint32_t *max)
{
    /* ex) min = 15 , max = 30 */
    getMetaCtlAeTargetFpsRange(&m_metadata, min, max);
}

bool ExynosCameraParameters::m_getSupportedVariableFpsList(int min, int max, int *newMin, int *newMax)
{
    int (*sizeList)[2];

    if (getCameraId() == CAMERA_ID_BACK) {
        /* Try to find exactly same in REAR LIST*/
        sizeList = m_staticInfo->rearFPSList;
        for (int i = 0; i < m_staticInfo->rearFPSListMax; i++) {
            if (sizeList[i][1] == max && sizeList[i][0] == min) {
                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                return true;
            }
        }
        /* Try to find exactly same in HIDDEN REAR LIST*/
        sizeList = m_staticInfo->hiddenRearFPSList;
        for (int i = 0; i < m_staticInfo->hiddenRearFPSListMax; i++) {
            if (sizeList[i][1] == max && sizeList[i][0] == min) {
                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                return true;
            }
        }
        /* Try to find similar fps in REAR LIST*/
        sizeList = m_staticInfo->rearFPSList;
        for (int i = 0; i < m_staticInfo->rearFPSListMax; i++) {
            if (max <= sizeList[i][1] && sizeList[i][0] <= min) {
                if(sizeList[i][1] == sizeList[i][0])
                    continue;

                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                CLOGW("WARN(%s):calibrate new fps(%d/%d -> %d/%d)", __FUNCTION__, min, max, *newMin, *newMax);

                return true;
            }
        }
        /* Try to find similar fps in HIDDEN REAR LIST*/
        sizeList = m_staticInfo->hiddenRearFPSList;
        for (int i = 0; i < m_staticInfo->hiddenRearFPSListMax; i++) {
            if (max <= sizeList[i][1] && sizeList[i][0] <= min) {
                if(sizeList[i][1] == sizeList[i][0])
                    continue;

                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                CLOGW("WARN(%s):calibrate new fps(%d/%d -> %d/%d)", __FUNCTION__, min, max, *newMin, *newMax);

                return true;
            }
        }
    } else {
        /* Try to find exactly same in FRONT LIST*/
        sizeList = m_staticInfo->frontFPSList;
        for (int i = 0; i < m_staticInfo->frontFPSListMax; i++) {
            if (sizeList[i][1] == max && sizeList[i][0] == min) {
                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                return true;
            }
        }
        /* Try to find exactly same in HIDDEN FRONT LIST*/
        sizeList = m_staticInfo->hiddenFrontFPSList;
        for (int i = 0; i < m_staticInfo->hiddenFrontFPSListMax; i++) {
            if (sizeList[i][1] == max && sizeList[i][0] == min) {
                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                return true;
            }
        }
        /* Try to find similar fps in FRONT LIST*/
        sizeList = m_staticInfo->frontFPSList;
        for (int i = 0; i < m_staticInfo->frontFPSListMax; i++) {
            if (max <= sizeList[i][1] && sizeList[i][0] <= min) {
                if(sizeList[i][1] == sizeList[i][0])
                    continue;

                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                CLOGW("WARN(%s):calibrate new fps(%d/%d -> %d/%d)", __FUNCTION__, min, max, *newMin, *newMax);

                return true;
            }
        }
        /* Try to find similar fps in HIDDEN FRONT LIST*/
        sizeList = m_staticInfo->hiddenFrontFPSList;
        for (int i = 0; i < m_staticInfo->hiddenFrontFPSListMax; i++) {
            if (max <= sizeList[i][1] && sizeList[i][0] <= min) {
                if(sizeList[i][1] == sizeList[i][0])
                    continue;

                *newMin = sizeList[i][0];
                *newMax = sizeList[i][1];

                CLOGW("WARN(%s):calibrate new fps(%d/%d -> %d/%d)", __FUNCTION__, min, max, *newMin, *newMax);

                return true;
            }
        }
    }

    return false;
}

status_t ExynosCameraParameters::checkVideoSize(const CameraParameters& params)
{
    /*  Video size */
    int newVideoW = 0;
    int newVideoH = 0;

    params.getVideoSize(&newVideoW, &newVideoH);

    if (0 < newVideoW && 0 < newVideoH &&
        m_isSupportedVideoSize(newVideoW, newVideoH) == false) {
        return BAD_VALUE;
    }

    CLOGI("INFO(%s):newVideo Size (%dx%d), ratioId(%d)",
        "setParameters", newVideoW, newVideoH, m_cameraInfo.videoSizeRatioId);
    m_setVideoSize(newVideoW, newVideoH);
    m_params.setVideoSize(newVideoW, newVideoH);

    return NO_ERROR;
}

bool ExynosCameraParameters::m_isSupportedVideoSize(const int width,
                                                    const int height)
{
    int maxWidth = 0;
    int maxHeight = 0;
    int (*sizeList)[SIZE_OF_RESOLUTION];

    getMaxVideoSize(&maxWidth, &maxHeight);

    if (maxWidth < width || maxHeight < height) {
        CLOGE("ERR(%s):invalid video Size(maxSize(%d/%d) size(%d/%d)",
                __FUNCTION__, maxWidth, maxHeight, width, height);
        return false;
    }

    if (getCameraId() == CAMERA_ID_BACK) {
        sizeList = m_staticInfo->rearVideoList;
        for (int i = 0; i < m_staticInfo->rearVideoListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.videoSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    } else {
        sizeList = m_staticInfo->frontVideoList;
        for (int i = 0; i <  m_staticInfo->frontVideoListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.videoSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    }

    if (getCameraId() == CAMERA_ID_BACK) {
        sizeList = m_staticInfo->hiddenRearVideoList;
        for (int i = 0; i < m_staticInfo->hiddenRearVideoListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.videoSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    } else {
        sizeList = m_staticInfo->hiddenFrontVideoList;
        for (int i = 0; i < m_staticInfo->hiddenFrontVideoListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.videoSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    }

    CLOGE("ERR(%s):Invalid video size(%dx%d)", __FUNCTION__, width, height);

    return false;
}

bool ExynosCameraParameters::m_isUHDRecordingMode(void)
{
    bool isUHDRecording = false;
    int videoW = 0, videoH = 0;
    getVideoSize(&videoW, &videoH);

    if (((videoW == 3840 && videoH == 2160) || (videoW == 2560 && videoH == 1440)) && getRecordingHint() == true)
        isUHDRecording = true;

#if 0
    /* we need to make WQHD SCP(LCD size), when FHD recording for clear rendering */
    int hwPreviewW = 0, hwPreviewH = 0;
    getHwPreviewSize(&hwPreviewW, &hwPreviewH);

#ifdef SUPPORT_SW_VDIS
    if(m_swVDIS_Mode) {
        m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);
    }
#endif /*SUPPORT_SW_VDIS*/

    /* regard align margin(ex:1920x1088), check size more than 1920x1088 */
    /* if (1920 < hwPreviewW && 1080 < hwPreviewH) */
    if ((ALIGN_UP(1920, CAMERA_MAGIC_ALIGN) < hwPreviewW) &&
        (ALIGN_UP(1080, CAMERA_MAGIC_ALIGN) < hwPreviewH) &&
        (getRecordingHint() == true)) {
        isUHDRecording = true;
    }
#endif

    return isUHDRecording;
}

void ExynosCameraParameters::m_set3aaIspOtf(bool enable)
{
    m_flagCheck3aaIspOtf = true;

    m_flag3aaIspOtf = enable;
}

bool ExynosCameraParameters::m_get3aaIspOtf(bool query)
{
    bool ret = query;

    if (m_flagCheck3aaIspOtf == true)
        ret = m_flag3aaIspOtf;

    return ret;
}

void ExynosCameraParameters::m_setIspTpuOtf(bool enable)
{
    m_flagCheckIspTpuOtf = true;

    m_flagIspTpuOtf = enable;
}

bool ExynosCameraParameters::m_getIspTpuOtf(bool query)
{
    bool ret = query;

    if (m_flagCheckIspTpuOtf == true)
        ret = m_flagIspTpuOtf;

    return ret;
}

void ExynosCameraParameters::m_setVideoSize(int w, int h)
{
    m_cameraInfo.videoW = w;
    m_cameraInfo.videoH = h;
}

bool ExynosCameraParameters::getUHDRecordingMode(void)
{
    return m_isUHDRecordingMode();
}

bool ExynosCameraParameters::getFaceDetectionMode(bool flagCheckingRecording)
{
    bool ret = true;

    /* turn off when dual mode back camera */
    if (getDualMode() == true &&
        getCameraId() == CAMERA_ID_BACK) {
        ret = false;
    }

    /* turn off when vt mode */
    if (getVtMode() != 0)
        ret = false;

    /* when stopRecording, ignore recording hint */
    if (flagCheckingRecording == true) {
        /* when recording mode, turn off back camera */
        if (getRecordingHint() == true) {
            if (getCameraId() == CAMERA_ID_BACK)
                ret = false;
        }
    }

    return ret;
}

void ExynosCameraParameters::getVideoSize(int *w, int *h)
{
    *w = m_cameraInfo.videoW;
    *h = m_cameraInfo.videoH;
}

void ExynosCameraParameters::getMaxVideoSize(int *w, int *h)
{
    *w = m_staticInfo->maxVideoW;
    *h = m_staticInfo->maxVideoH;
}

int ExynosCameraParameters::getVideoFormat(void)
{
    if (getAdaptiveCSCRecording() == true) {
        return V4L2_PIX_FMT_NV21M;
    } else {
        return V4L2_PIX_FMT_NV12M;
    }
}

bool ExynosCameraParameters::getReallocBuffer() {
    Mutex::Autolock lock(m_reallocLock);
    return m_reallocBuffer;
}

bool ExynosCameraParameters::setReallocBuffer(bool enable) {
    Mutex::Autolock lock(m_reallocLock);
    m_reallocBuffer = enable;
    return m_reallocBuffer;
}

status_t ExynosCameraParameters::checkFastFpsMode(const CameraParameters& params)
{
#ifdef TEST_GED_HIGH_SPEED_RECORDING
    int fastFpsMode  = getFastFpsMode();
#else
    int fastFpsMode  = params.getInt("fast-fps-mode");
#endif
    int tempShotMode = params.getInt("shot-mode");
    int prevFpsMode = getFastFpsMode();

    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;
    uint32_t newMinFps = curMinFps;
    uint32_t newMaxFps = curMaxFps;

    bool recordingHint = getRecordingHint();
    bool isShotModeAnimated = false;
    bool flagHighSpeed = false;
    int newVideoW = 0;
    int newVideoH = 0;

    params.getVideoSize(&newVideoW, &newVideoH);
    getPreviewFpsRange(&curMinFps, &curMaxFps);

    // Workaround : Should be removed later once application fixes this.
    if( (curMinFps == 60 && curMaxFps == 60) && (newVideoW == 1920 && newVideoH == 1080) ) {
        fastFpsMode = 1;
    }

    CLOGD("DEBUG(%s):fast-fps-mode : %d", "setParameters", fastFpsMode);

#if (!USE_HIGHSPEED_RECORDING)
    fastFpsMode = -1;
    CLOGD("DEBUG(%s):fast-fps-mode not supported. set to (%d).", "setParameters", fastFpsMode);
#endif

    CLOGI("INFO(%s):curFpsRange[Min=%d, Max=%d], [curFrameRate=%d]",
        "checkPreviewFpsRange", curMinFps, curMaxFps, m_params.getPreviewFrameRate());


    if (fastFpsMode <= 0 || fastFpsMode > 3) {
        m_setHighSpeedRecording(false);
        setConfigMode(CONFIG_MODE::NORMAL);
        if( fastFpsMode != prevFpsMode) {
            setFastFpsMode(fastFpsMode);
            m_params.set("fast-fps-mode", fastFpsMode);
            setReallocBuffer(true);
            m_setRestartPreviewChecked(true);
        }
        return NO_ERROR;
    } else {
        if( fastFpsMode == prevFpsMode ) {
            CLOGE("INFO(%s):mode is not changed fastFpsMode(%d) prevFpsMode(%d)", "checkFastFpsMode", fastFpsMode, prevFpsMode);
            return NO_ERROR;
        }
    }

    if (tempShotMode == SHOT_MODE_ANIMATED_SCENE) {
        if (curMinFps == 15 && curMaxFps == 15)
            isShotModeAnimated = true;
    }

    if ((recordingHint == true) && !(isShotModeAnimated)) {

        CLOGD("DEBUG(%s):Set High Speed Recording", "setParameters");

        switch(fastFpsMode) {
            case 1:
                newMinFps = 60;
                newMaxFps = 60;
                setConfigMode(CONFIG_MODE::HIGHSPEED_60);
                break;
            case 2:
                newMinFps = 120;
                newMaxFps = 120;
                setConfigMode(CONFIG_MODE::HIGHSPEED_120);
                break;
            case 3:
                newMinFps = 240;
                newMaxFps = 240;
                setConfigMode(CONFIG_MODE::HIGHSPEED_240);
                break;
        }
        if( fastFpsMode != prevFpsMode ) {
            setFastFpsMode(fastFpsMode);
            m_params.set("fast-fps-mode", fastFpsMode);
        } else {
            CLOGI("INFO(%s):mode is not changed fastFpsMode(%d) prevFpsMode(%d)", "checkFastFpsMode", fastFpsMode, prevFpsMode);
            return NO_ERROR;
        }
        CLOGI("INFO(%s):fastFpsMode(%d) prevFpsMode(%d)", "checkFastFpsMode", fastFpsMode, prevFpsMode);
        setReallocBuffer(true);
        m_setRestartPreviewChecked(true);

        flagHighSpeed = m_adjustHighSpeedRecording(curMinFps, curMaxFps, newMinFps, newMaxFps);
        m_setHighSpeedRecording(flagHighSpeed);
        m_setPreviewFpsRange(newMinFps, newMaxFps);

        CLOGI("INFO(%s):m_setPreviewFpsRange(newFpsRange[Min=%d, Max=%d])", "checkFastFpsMode", newMinFps, newMaxFps);
#ifdef TEST_GED_HIGH_SPEED_RECORDING
        m_params.setPreviewFrameRate(newMaxFps);
        CLOGD("DEBUG(%s):setPreviewFrameRate (newMaxFps=%d)", "checkFastFpsMode", newMaxFps);
#endif
        updateHwSensorSize();
    }

    return NO_ERROR;
};

void ExynosCameraParameters::setFastFpsMode(int fpsMode)
{
    m_fastFpsMode = fpsMode;
}

int ExynosCameraParameters::getFastFpsMode(void)
{
    return m_fastFpsMode;
}

void ExynosCameraParameters::m_setHighSpeedRecording(bool highSpeed)
{
    m_cameraInfo.highSpeedRecording = highSpeed;
}

bool ExynosCameraParameters::getHighSpeedRecording(void)
{
    return m_cameraInfo.highSpeedRecording;
}

bool ExynosCameraParameters::m_adjustHighSpeedRecording(int curMinFps, int curMaxFps, __unused int newMinFps, int newMaxFps)
{
    bool flagHighSpeedRecording = false;
    bool restartPreview = false;

    /* setting high speed */
    if (30 < newMaxFps) {
        flagHighSpeedRecording = true;
        /* 30 -> 60/120 */
        if (curMaxFps <= 30)
            restartPreview = true;
        /* 60 -> 120 */
        else if (curMaxFps <= 60 && 120 <= newMaxFps)
            restartPreview = true;
        /* 120 -> 60 */
        else if (curMaxFps <= 120 && newMaxFps <= 60)
            restartPreview = true;
        /* variable 60 -> fixed 60 */
        else if (curMinFps < 60 && newMaxFps <= 60)
            restartPreview = true;
        /* variable 120 -> fixed 120 */
        else if (curMinFps < 120 && newMaxFps <= 120)
            restartPreview = true;
    } else if (newMaxFps <= 30) {
        flagHighSpeedRecording = false;
        if (30 < curMaxFps)
            restartPreview = true;
    }

    if (restartPreview == true &&
        getPreviewRunning() == true) {
        CLOGD("DEBUG(%s[%d]):setRestartPreviewChecked true", __FUNCTION__, __LINE__);
        m_setRestartPreviewChecked(true);
    }

    return flagHighSpeedRecording;
}

void ExynosCameraParameters::m_setRestartPreviewChecked(bool restart)
{
    CLOGD("DEBUG(%s):setRestartPreviewChecked(during SetParameters) %s", __FUNCTION__, restart ? "true" : "false");
    Mutex::Autolock lock(m_parameterLock);

    m_flagRestartPreviewChecked = restart;
}

bool ExynosCameraParameters::m_getRestartPreviewChecked(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_flagRestartPreviewChecked;
}

void ExynosCameraParameters::m_setRestartPreview(bool restart)
{
    CLOGD("DEBUG(%s):setRestartPreview %s", __FUNCTION__, restart ? "true" : "false");
    Mutex::Autolock lock(m_parameterLock);

    m_flagRestartPreview = restart;

}

void ExynosCameraParameters::setPreviewRunning(bool enable)
{
    Mutex::Autolock lock(m_parameterLock);

    m_previewRunning = enable;
    m_flagRestartPreviewChecked = false;
    m_flagRestartPreview = false;
}

void ExynosCameraParameters::setPictureRunning(bool enable)
{
    Mutex::Autolock lock(m_parameterLock);

    m_pictureRunning = enable;
}

#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
void ExynosCameraParameters::setRomReadThreadDone(bool enable)
{
    Mutex::Autolock lock(m_parameterLock);

    m_romReadThreadDone = enable;
}
#endif

void ExynosCameraParameters::setRecordingRunning(bool enable)
{
    Mutex::Autolock lock(m_parameterLock);

    m_recordingRunning = enable;
}

bool ExynosCameraParameters::getPreviewRunning(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_previewRunning;
}

bool ExynosCameraParameters::getPictureRunning(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_pictureRunning;
}

bool ExynosCameraParameters::getRecordingRunning(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_recordingRunning;
}

bool ExynosCameraParameters::getRestartPreview(void)
{
    Mutex::Autolock lock(m_parameterLock);

    return m_flagRestartPreview;
}

status_t ExynosCameraParameters::checkVideoStabilization(const CameraParameters& params)
{
    /* video stablization */
    const char *newVideoStabilization = params.get(CameraParameters::KEY_VIDEO_STABILIZATION);
    bool currVideoStabilization = m_flagVideoStabilization;
    bool isVideoStabilization = false;

    if (newVideoStabilization != NULL) {
        CLOGD("DEBUG(%s):newVideoStabilization %s", "setParameters", newVideoStabilization);

        if (!strcmp(newVideoStabilization, "true"))
            isVideoStabilization = true;

        if (currVideoStabilization != isVideoStabilization) {
            m_flagVideoStabilization = isVideoStabilization;
            m_setVideoStabilization(m_flagVideoStabilization);
            m_params.set(CameraParameters::KEY_VIDEO_STABILIZATION, newVideoStabilization);
        }
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setVideoStabilization(bool stabilization)
{
    m_cameraInfo.videoStabilization = stabilization;
}

bool ExynosCameraParameters::getVideoStabilization(void)
{
    return m_cameraInfo.videoStabilization;
}

bool ExynosCameraParameters::updateTpuParameters(void)
{
    status_t ret = NO_ERROR;
    bool flagTpuOn = false;

#if defined(USE_3DNR_ALWAYS)
    if (USE_3DNR_ALWAYS == true) {
        int hwPreviewW = 0, hwPreviewH = 0;
        uint32_t minFpsRange = 0;
        uint32_t maxFpsRange = 0;

        getPreviewFpsRange(&minFpsRange, &maxFpsRange);
        getHwPreviewSize(&hwPreviewW, &hwPreviewH);

        if (3840 <= hwPreviewW && 2160 <= hwPreviewH) {
/* 1. TDNR : UHD Recording Featuring */
#if defined(USE_3DNR_UHDRECORDING)
            if (USE_3DNR_UHDRECORDING == true){
                flagTpuOn = true;
            } else {
                flagTpuOn = false;
            }
#else
            flagTpuOn = false;
#endif /* USE_3DNR_UHDRECORDING */
        } else {
            flagTpuOn = true;
        }

        if (60 < minFpsRange || 60 < maxFpsRange) {
/* 2. TDNR : HighSpeed Recording Featuring */
#if defined(USE_3DNR_HIGHSPEED_RECORDING)
            flagTpuOn = USE_3DNR_HIGHSPEED_RECORDING;
#else
            flagTpuOn = false;
#endif /* USE_3DNR_HIGHSPEED_RECORDING */
        }

/* 3. TDNR : Vt call Featuring */
#if defined(USE_3DNR_VTCALL)
        if (getVtMode() > 0)
            flagTpuOn = USE_3DNR_VTCALL;
#endif /* USE_3DNR_VTCALL */

/* 4. TDNR : dual always disabled. */
        if (getDualMode() == true) {
            flagTpuOn = false;

            CLOGD("DEBUG(%s):Turn off 3dnrMode by dualMode", "setParameters");
        }

/* 5. TDNR : smart stay */
        if (getIntelligentMode() == 1) {
#if defined(USE_3DNR_SMARTSTAY)
            flagTpuOn = USE_3DNR_SMARTSTAY;
#else
            flagTpuOn = false;
#endif /* USE_3DNR_SMARTSTAY */
        }

/* 6. TDNR : captrue preview */
        if (getRecordingHint() == false) {
#if defined(USE_3DNR_PREVIEW)
            flagTpuOn = USE_3DNR_PREVIEW;
#else
            flagTpuOn = false;
#endif
        }

#ifdef USE_LIVE_BROADCAST
        if (getPLBMode() == true) {
            flagTpuOn = false;
        }
#endif
    } else {
        flagTpuOn = false;
    }
#else
    flagTpuOn = false;
#endif /* USE_3DNR_ALWAYS */

    m_setIspTpuOtf((flagTpuOn)? false : true);

    /* 1. update data video stabilization state to actual*/
    bool oldVideoStabilization = m_cameraInfo.videoStabilization;

    m_setVideoStabilization(m_flagVideoStabilization);

    bool flagHWVdisOn  = this->getHWVdisMode();

    if (isIspTpuOtf() == true) {
        if (flagHWVdisOn == true)
            CLOGD("DEBUG(%s[%d]):set flagHWVdisOn = false (by isIspTpuOtf() == true)", __FUNCTION__, __LINE__);

        flagHWVdisOn = false;
    }

    if (setDisEnable(flagHWVdisOn) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDisEnable(%d) fail", __FUNCTION__, __LINE__, flagHWVdisOn);
    }

    CLOGD("%s(%d) video stabilization old(%d) new(%d) flagHWVdisOn(%d)",
        __FUNCTION__, __LINE__, oldVideoStabilization, getVideoStabilization(), flagHWVdisOn);

    /* 2. update data 3DNR state to actual*/
    bool old3dnrMode = m_cameraInfo.is3dnrMode;

    bool flag3dnrOn = m_flag3dnrMode;
    if (isIspTpuOtf() == true) {
        if (flag3dnrOn == true)
            CLOGD("DEBUG(%s[%d]):set flag3dnrOn = false (by isIspTpuOtf() == true)", __FUNCTION__, __LINE__);

        flag3dnrOn = false;
    }

    if (flag3dnrOn == true)
        flag3dnrOn = m_getFlag3dnrOffCase();

    m_set3dnrMode(flag3dnrOn);

    if (setDnrEnable(flag3dnrOn) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDnrEnable(%d) fail", __FUNCTION__, __LINE__, flag3dnrOn);
    }

    CLOGD("%s(%d) 3DNR old(%d) new(%d)", __FUNCTION__, __LINE__, old3dnrMode, flag3dnrOn);

    return true;
}

status_t ExynosCameraParameters::checkSWVdisMode(const CameraParameters& params)
{
    const char *newSwVdis = params.get("sw-vdis");
    bool currSwVdis = getSWVdisMode();
    if (newSwVdis != NULL) {
        CLOGD("DEBUG(%s):newSwVdis %s", "setParameters", newSwVdis);
        bool swVdisMode = true;

        if (!strcmp(newSwVdis, "off"))
            swVdisMode = false;

        m_setSWVdisMode(swVdisMode);
        m_params.set("sw-vdis", newSwVdis);
    }

    return NO_ERROR;
}

status_t ExynosCameraParameters::checkHWVdisMode(__unused const CameraParameters& params)
{
    status_t ret = NO_ERROR;

    bool hwVdisMode = this->getHWVdisMode();

    CLOGD("DEBUG(%s):newHwVdis %d", "setParameters", hwVdisMode);

    ret = setDisEnable(hwVdisMode);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDisEnable(%d) fail", __FUNCTION__, __LINE__, hwVdisMode);
    } else {
        if (m_flagHWVDisMode != hwVdisMode) {
            m_flagHWVDisMode = hwVdisMode;
        }
    }

    return ret;
}

bool ExynosCameraParameters::isSWVdisMode(void)
{
    bool swVDIS_mode = false;
    bool use3DNR_dmaout = false;

    int nPreviewW, nPreviewH;
    getPreviewSize(&nPreviewW, &nPreviewH);

    if ((getRecordingHint() == true) &&
#ifndef SUPPORT_SW_VDIS_FRONTCAM
        (getCameraId() == CAMERA_ID_BACK) &&
#endif /*SUPPORT_SW_VDIS_FRONTCAM*/
        (getHighSpeedRecording() == false) &&
        (use3DNR_dmaout == false) &&
        (getSWVdisUIMode() == true) &&
        ((nPreviewW == 1920 && nPreviewH == 1080) || (nPreviewW == 1280 && nPreviewH == 720)))
    {
        swVDIS_mode = true;
    }

    return swVDIS_mode;
}

bool ExynosCameraParameters::isSWVdisModeWithParam(int nPreviewW, int nPreviewH)
{
    bool swVDIS_mode = false;
    bool use3DNR_dmaout = false;

    if ((getRecordingHint() == true) &&
#ifndef SUPPORT_SW_VDIS_FRONTCAM
        (getCameraId() == CAMERA_ID_BACK) &&
#endif /*SUPPORT_SW_VDIS_FRONTCAM*/
        (getHighSpeedRecording() == false) &&
        (use3DNR_dmaout == false) &&
        (getSWVdisUIMode() == true) &&
        ((nPreviewW == 1920 && nPreviewH == 1080) || (nPreviewW == 1280 && nPreviewH == 720)))
    {
        swVDIS_mode = true;
    }

    return swVDIS_mode;
}

bool ExynosCameraParameters::getHWVdisMode(void)
{
    bool ret = this->getVideoStabilization();

    /*
     * Only true case,
     * we will test whether support or not.
     */
    if (ret == true) {
        switch (getCameraId()) {
        case CAMERA_ID_BACK:
#ifdef SUPPORT_BACK_HW_VDIS
            ret = SUPPORT_BACK_HW_VDIS;
#else
            ret = false;
#endif
            break;
        case CAMERA_ID_FRONT:
#ifdef SUPPORT_FRONT_HW_VDIS
            ret = SUPPORT_FRONT_HW_VDIS;
#else
            ret = false;
#endif
            break;
        default:
            ret = false;
            break;
        }
    }

#ifdef SUPPORT_SW_VDIS
    if (ret == true &&
        this->isSWVdisMode() == true) {
        ret = false;
    }
#endif /*SUPPORT_SW_VDIS*/

    return ret;
}

int ExynosCameraParameters::getHWVdisFormat(void)
{
    return V4L2_PIX_FMT_YUYV;
}

void ExynosCameraParameters::m_setSWVdisMode(bool swVdis)
{
    m_cameraInfo.swVdisMode = swVdis;
}

bool ExynosCameraParameters::getSWVdisMode(void)
{
    return m_cameraInfo.swVdisMode;
}

void ExynosCameraParameters::m_setSWVdisUIMode(bool swVdisUI)
{
    m_cameraInfo.swVdisUIMode = swVdisUI;
}

bool ExynosCameraParameters::getSWVdisUIMode(void)
{
    return m_cameraInfo.swVdisUIMode;
}

status_t ExynosCameraParameters::checkPreviewSize(const CameraParameters& params)
{
    /* preview size */
    int previewW = 0;
    int previewH = 0;
    int newPreviewW = 0;
    int newPreviewH = 0;
    int newCalHwPreviewW = 0;
    int newCalHwPreviewH = 0;

    int curPreviewW = 0;
    int curPreviewH = 0;
    int curHwPreviewW = 0;
    int curHwPreviewH = 0;

    params.getPreviewSize(&previewW, &previewH);
    getPreviewSize(&curPreviewW, &curPreviewH);
    getHwPreviewSize(&curHwPreviewW, &curHwPreviewH);
    m_isHighResolutionMode(params);

    newPreviewW = previewW;
    newPreviewH = previewH;
    if (m_adjustPreviewSize(previewW, previewH, &newPreviewW, &newPreviewH, &newCalHwPreviewW, &newCalHwPreviewH) != OK) {
        ALOGE("ERR(%s): adjustPreviewSize fail, newPreviewSize(%dx%d)", "Parameters", newPreviewW, newPreviewH);
        return BAD_VALUE;
    }

    if (m_isSupportedPreviewSize(newPreviewW, newPreviewH) == false) {
        ALOGE("ERR(%s): new preview size is invalid(%dx%d)", "Parameters", newPreviewW, newPreviewH);
        return BAD_VALUE;
    }

    ALOGI("INFO(%s):Cur Preview size(%dx%d)", "setParameters", curPreviewW, curPreviewH);
    ALOGI("INFO(%s):Cur HwPreview size(%dx%d)", "setParameters", curHwPreviewW, curHwPreviewH);
    ALOGI("INFO(%s):param.preview size(%dx%d)", "setParameters", previewW, previewH);
    ALOGI("INFO(%s):Adjust Preview size(%dx%d), ratioId(%d)", "setParameters", newPreviewW, newPreviewH, m_cameraInfo.previewSizeRatioId);
    ALOGI("INFO(%s):Calibrated HwPreview size(%dx%d)", "setParameters", newCalHwPreviewW, newCalHwPreviewH);

    if (curPreviewW != newPreviewW || curPreviewH != newPreviewH ||
        curHwPreviewW != newCalHwPreviewW || curHwPreviewH != newCalHwPreviewH ||
        getHighResolutionCallbackMode() == true) {
        m_setPreviewSize(newPreviewW, newPreviewH);
        m_setHwPreviewSize(newCalHwPreviewW, newCalHwPreviewH);

        if (getHighResolutionCallbackMode() == true) {
            m_previewSizeChanged = false;
        } else {
            ALOGD("DEBUG(%s):setRestartPreviewChecked true", __FUNCTION__);
#ifdef USE_ISP_BUFFER_SIZE_TO_BDS
            setReallocBuffer(true);
#endif
            m_setRestartPreviewChecked(true);
            m_previewSizeChanged = true;
        }
    } else {
        m_previewSizeChanged = false;
    }

    updateBinningScaleRatio();
    updateBnsScaleRatio();

    m_params.setPreviewSize(newPreviewW, newPreviewH);

    return NO_ERROR;
}

status_t ExynosCameraParameters::m_adjustPreviewSize(__unused int previewW, __unused int previewH,
                                                     int *newPreviewW, int *newPreviewH,
                                                     int *newCalHwPreviewW, int *newCalHwPreviewH)
{
    /* hack : when app give 1446, we calibrate to 1440 */
    if (*newPreviewW == 1446 && *newPreviewH == 1080) {
        CLOGW("WARN(%s):Invalid previewSize(%d/%d). so, calibrate to (1440/%d)", __FUNCTION__, *newPreviewW, *newPreviewH, *newPreviewH);
        *newPreviewW = 1440;
    }

    /* calibrate H/W aligned size*/
    if (getRecordingHint() == true) {
        int videoW = 0, videoH = 0;
        ExynosRect bdsRect;

        getVideoSize(&videoW, &videoH);

        if ((videoW <= *newPreviewW) && (videoH <= *newPreviewH)) {
#ifdef SUPPORT_SW_VDIS
            if (isSWVdisModeWithParam(*newPreviewW, *newPreviewH) == true) {
                m_getSWVdisPreviewSize(*newPreviewW, *newPreviewH, newCalHwPreviewW, newCalHwPreviewH);
            } else
#endif /*SUPPORT_SW_VDIS*/
            {
#if defined(LIMIT_SCP_SIZE_UNTIL_FHD_ON_RECORDING)
                if ((videoW <= 1920 || videoH <= 1080) &&
                    (1920 < *newPreviewW || 1080 < *newPreviewH)) {

                    float videoRatio = ROUND_OFF(((float)videoW / (float)videoH), 2);

                    if (videoRatio == 1.33f) { /* 4:3 */
                        *newCalHwPreviewW = 1440;
                        *newCalHwPreviewH = 1080;
                    } else if (videoRatio == 1.77f) { /* 16:9 */
                        *newCalHwPreviewW = 1920;
                        *newCalHwPreviewH = 1080;
                    } else if (videoRatio == 1.00f) { /* 1:1 */
                        *newCalHwPreviewW = 1088;
                        *newCalHwPreviewH = 1088;
                    } else {
                        *newCalHwPreviewW = *newPreviewW;
                        *newCalHwPreviewH = *newPreviewH;
                    }

                    if (*newCalHwPreviewW != *newPreviewW  ||
                        *newCalHwPreviewH != *newPreviewH) {
                        CLOGW("WARN(%s[%d]):Limit hw preview size until %d x %d when videoSize(%d x %d)",
                            __FUNCTION__, __LINE__, *newCalHwPreviewW, *newCalHwPreviewH, videoW, videoH);
                    }
                } else
#endif
                {
                    *newCalHwPreviewW = *newPreviewW;
                    *newCalHwPreviewH = *newPreviewH;
                }
            }

            if (getHighSpeedRecording() == true) {
                int sizeList[SIZE_LUT_INDEX_END];

                if (m_getPreviewSizeList(sizeList) == NO_ERROR) {
                    /* On high-speed recording, scaling-up by SCC/SCP occurs the IS-ISP performance degradation.
                                    The scaling-up might be done by GSC for recording */
                    if ((sizeList[BDS_W] < *newPreviewW) && (sizeList[BDS_H] < *newPreviewH)) {
                        *newCalHwPreviewW = sizeList[BDS_W];
                        *newCalHwPreviewH = sizeList[BDS_H];
                    }
                } else {
                    ALOGE("ERR(%s):m_getPreviewSizeList() fail", __FUNCTION__);
                }
            }
        } else {
#ifdef SUPPORT_SW_VDIS
            if (isSWVdisModeWithParam(videoW, videoH) == true) {
                m_getSWVdisPreviewSize(videoW, videoH, newCalHwPreviewW, newCalHwPreviewH);
            } else
#endif /*SUPPORT_SW_VDIS*/
            /* video size > preview size : Use BDS size for SCP output size */
            {
                ALOGV("DEBUG(%s[%d]):preview(%dx%d) is smaller than video(%dx%d)",
                        __FUNCTION__, __LINE__, *newPreviewW, *newPreviewH, videoW, videoH);

                /* If the video ratio is differ with preview ratio,
                   the default ratio is set into preview ratio */
                if (SIZE_RATIO(*newPreviewW, *newPreviewH) != SIZE_RATIO(videoW, videoH))
                    ALOGW("WARN(%s): preview ratio(%dx%d) is not matched with video ratio(%dx%d)", __FUNCTION__,
                            *newPreviewW, *newPreviewH, videoW, videoH);

                if (m_isSupportedPreviewSize(*newPreviewW, *newPreviewH) == false) {
                    ALOGE("ERR(%s): new preview size is invalid(%dx%d)",
                            __FUNCTION__, *newPreviewW, *newPreviewH);
                    return BAD_VALUE;
                }

                /*
                 * This call is to get real preview size.
                 * so, HW dis size must not be added.
                 */
                m_getPreviewBdsSize(&bdsRect);

                *newCalHwPreviewW = bdsRect.w;
                *newCalHwPreviewH = bdsRect.h;
            }
        }
    } else if (getHighResolutionCallbackMode() == true) {
        if(CAMERA_LCD_SIZE == LCD_SIZE_1280_720) {
            *newCalHwPreviewW = 1280;
            *newCalHwPreviewH = 720;
        } else {
            *newCalHwPreviewW = 1920;
            *newCalHwPreviewH = 1080;
        }
    } else {
        *newCalHwPreviewW = *newPreviewW;
        *newCalHwPreviewH = *newPreviewH;
    }

    if (getHalVersion() == IS_HAL_VER_3_2) {
#if defined(ENABLE_8MP_FULL_FRAME)
        ExynosRect bdsRect;
        getPreviewBdsSize(&bdsRect);
        *newCalHwPreviewW = bdsRect.w;
        *newCalHwPreviewH = bdsRect.h;
#else
        /* 1. try to get exact ratio */
        if (m_isSupportedPreviewSize(*newPreviewW, *newPreviewH) == false) {
            ALOGE("ERR(%s): new preview size is invalid(%dx%d)", "Parameters", *newPreviewW, *newPreviewH);
        }

        /* 2. get bds size to set size to scp node due to internal scp buffer */
        int sizeList[SIZE_LUT_INDEX_END];
        if (m_getPreviewSizeList(sizeList) == NO_ERROR) {
            *newCalHwPreviewW = sizeList[BDS_W];
            *newCalHwPreviewH = sizeList[BDS_H];
        } else {
            ExynosRect bdsRect;
            getPreviewBdsSize(&bdsRect);
            *newCalHwPreviewW = bdsRect.w;
            *newCalHwPreviewH = bdsRect.h;
        }
#endif
    }

    return NO_ERROR;
}

bool ExynosCameraParameters::m_isSupportedPreviewSize(const int width,
                                                     const int height)
{
    int maxWidth, maxHeight = 0;
    int (*sizeList)[SIZE_OF_RESOLUTION];

    if (getHighResolutionCallbackMode() == true) {
        CLOGD("DEBUG(%s): Burst panorama mode start", __FUNCTION__);
        m_cameraInfo.previewSizeRatioId = SIZE_RATIO_16_9;
        return true;
    }

    getMaxPreviewSize(&maxWidth, &maxHeight);

    if (maxWidth*maxHeight < width*height) {
        CLOGE("ERR(%s):invalid PreviewSize(maxSize(%d/%d) size(%d/%d)",
            __FUNCTION__, maxWidth, maxHeight, width, height);
        return false;
    }

    if (getHalVersion() == IS_HAL_VER_3_2) {
        if (getCameraId() == CAMERA_ID_BACK) {
            sizeList = m_staticInfo->rearVideoList;
            for (int i = 0; i < m_staticInfo->rearVideoListMax; i++) {
                if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                    continue;
                if (sizeList[i][0] == width && sizeList[i][1] == height) {
                    m_cameraInfo.previewSizeRatioId = sizeList[i][2];
                    return true;
                }
            }
        } else {
            sizeList = m_staticInfo->frontVideoList;
            for (int i = 0; i <  m_staticInfo->frontVideoListMax; i++) {
                if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                    continue;
                if (sizeList[i][0] == width && sizeList[i][1] == height) {
                    m_cameraInfo.previewSizeRatioId = sizeList[i][2];
                    return true;
                }
            }
        }
    } else {
        if (getCameraId() == CAMERA_ID_BACK) {
            sizeList = m_staticInfo->rearPreviewList;
            for (int i = 0; i < m_staticInfo->rearPreviewListMax; i++) {
                if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                    continue;
                if (sizeList[i][0] == width && sizeList[i][1] == height) {
                    m_cameraInfo.previewSizeRatioId = sizeList[i][2];
                    return true;
                }
            }
        } else {
            sizeList = m_staticInfo->frontPreviewList;
            for (int i = 0; i <  m_staticInfo->frontPreviewListMax; i++) {
                if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                    continue;
                if (sizeList[i][0] == width && sizeList[i][1] == height) {
                    m_cameraInfo.previewSizeRatioId = sizeList[i][2];
                    return true;
                }
            }
        }

        if (getCameraId() == CAMERA_ID_BACK) {
            sizeList = m_staticInfo->hiddenRearPreviewList;
            for (int i = 0; i < m_staticInfo->hiddenRearPreviewListMax; i++) {
                if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                    continue;
                if (sizeList[i][0] == width && sizeList[i][1] == height) {
                    m_cameraInfo.previewSizeRatioId = sizeList[i][2];
                    return true;
                }
            }
        } else {
            sizeList = m_staticInfo->hiddenFrontPreviewList;
            for (int i = 0; i < m_staticInfo->hiddenFrontPreviewListMax; i++) {
                if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                    continue;
                if (sizeList[i][0] == width && sizeList[i][1] == height) {
                    m_cameraInfo.previewSizeRatioId = sizeList[i][2];
                    return true;
                }
            }
        }
    }

    CLOGE("ERR(%s):Invalid preview size(%dx%d)", __FUNCTION__, width, height);

    return false;
}

int *ExynosCameraParameters::getDualModeSizeTable(void) {
    int *sizeList = NULL;
#ifdef USE_LIVE_BROADCAST_DUAL
    bool plb_mode = getPLBMode();

    CLOGV("(%s[%d]): plb_mode(%d), BinningMode(%d)",
        __FUNCTION__, __LINE__, plb_mode, getBinningMode());

    if (plb_mode == true && getBinningMode()) {
        int index = 0;

        if (m_staticInfo->liveBroadcastSizeLut == NULL
            || m_staticInfo->liveBroadcastSizeLutMax == 0) {
            ALOGE("ERR(%s[%d]):liveBroadcastSizeLut is NULL", __FUNCTION__, __LINE__);
            return sizeList;
        }

        for (index = 0; index < m_staticInfo->liveBroadcastSizeLutMax; index++) {
            if (m_staticInfo->liveBroadcastSizeLut[index][0] == m_cameraInfo.previewSizeRatioId)
                break;
        }

        if (m_staticInfo->liveBroadcastSizeLutMax <= index) {
            ALOGE("ERR(%s[%d]):unsupported video ratioId(%d)",
                    __FUNCTION__, __LINE__, m_cameraInfo.previewSizeRatioId);
            sizeList = NULL;
        } else {
            sizeList = m_staticInfo->liveBroadcastSizeLut[index];
        }
    } else
#endif
    {
        if (getDualRecordingHint() == true
            && m_staticInfo->dualVideoSizeLut != NULL
            && m_cameraInfo.previewSizeRatioId < m_staticInfo->videoSizeLutMax) {
            sizeList = m_staticInfo->dualVideoSizeLut[m_cameraInfo.previewSizeRatioId];
        } else if (m_staticInfo->dualPreviewSizeLut != NULL
                   && m_cameraInfo.previewSizeRatioId < m_staticInfo->previewSizeLutMax) {
            sizeList = m_staticInfo->dualPreviewSizeLut[m_cameraInfo.previewSizeRatioId];
        } else { /* Use Preview LUT as a default */
            if (m_staticInfo->previewSizeLut == NULL) {
                ALOGE("ERR(%s[%d]):previewSizeLut is NULL", __FUNCTION__, __LINE__);
                sizeList = NULL;
                return sizeList;
            } else if (m_staticInfo->previewSizeLutMax <= m_cameraInfo.previewSizeRatioId) {
                ALOGE("ERR(%s[%d]):unsupported preview ratioId(%d)",
                        __FUNCTION__, __LINE__, m_cameraInfo.previewSizeRatioId);
                sizeList = NULL;
                return sizeList;
            }
            sizeList = m_staticInfo->previewSizeLut[m_cameraInfo.previewSizeRatioId];
        }
    }

    return sizeList;
}

int *ExynosCameraParameters::getBnsRecordingSizeTable(void) {
    int *sizeList = NULL;
#ifdef USE_LIVE_BROADCAST
    bool plb_mode = getPLBMode();

    CLOGV("(%s[%d]): plb_mode(%d), BinningMode(%d)",
        __FUNCTION__, __LINE__, plb_mode, getBinningMode());

    if (plb_mode == true && getBinningMode()) {
        int index = 0;

        if (m_staticInfo->liveBroadcastSizeLut == NULL
            || m_staticInfo->liveBroadcastSizeLutMax == 0) {
            ALOGE("ERR(%s[%d]):liveBroadcastSizeLut is NULL", __FUNCTION__, __LINE__);
            return sizeList;
        }

        for (index = 0; index < m_staticInfo->liveBroadcastSizeLutMax; index++) {
            if (m_staticInfo->liveBroadcastSizeLut[index][0] == m_cameraInfo.previewSizeRatioId)
                break;
        }

        if (m_staticInfo->liveBroadcastSizeLutMax <= index) {
            ALOGE("ERR(%s[%d]):unsupported video ratioId(%d)",
                    __FUNCTION__, __LINE__, m_cameraInfo.previewSizeRatioId);
            sizeList = NULL;
        } else {
            sizeList = m_staticInfo->liveBroadcastSizeLut[index];
        }
    } else
#endif
    {
        if (m_staticInfo->videoSizeLutMax <= m_cameraInfo.previewSizeRatioId) {
            ALOGE("ERR(%s[%d]):unsupported video ratioId(%d)",
                    __FUNCTION__, __LINE__, m_cameraInfo.previewSizeRatioId);
            return sizeList;
        }

        sizeList = m_staticInfo->videoSizeBnsLut[m_cameraInfo.previewSizeRatioId];
    }

    return sizeList;
}

int *ExynosCameraParameters::getNormalRecordingSizeTable(void) {
    int *sizeList = NULL;
#ifdef USE_LIVE_BROADCAST
    bool plb_mode = getPLBMode();

    CLOGV("(%s[%d]): plb_mode(%d), BinningMode(%d)",
        __FUNCTION__, __LINE__, plb_mode, getBinningMode());

    if (plb_mode == true && getBinningMode()) {
        int index = 0;

        if (m_staticInfo->liveBroadcastSizeLut == NULL
            || m_staticInfo->liveBroadcastSizeLutMax == 0) {
            ALOGE("ERR(%s[%d]):liveBroadcastSizeLut is NULL", __FUNCTION__, __LINE__);
            return sizeList;
        }

        for (index = 0; index < m_staticInfo->liveBroadcastSizeLutMax; index++) {
            if (m_staticInfo->liveBroadcastSizeLut[index][0] == m_cameraInfo.previewSizeRatioId)
                break;
        }

        if (m_staticInfo->liveBroadcastSizeLutMax <= index) {
            ALOGE("ERR(%s[%d]):unsupported video ratioId(%d)",
                    __FUNCTION__, __LINE__, m_cameraInfo.previewSizeRatioId);
            sizeList = NULL;
        } else {
            sizeList = m_staticInfo->liveBroadcastSizeLut[index];
        }
    } else
#endif
    {
        if (m_staticInfo->videoSizeLut == NULL) {
            ALOGE("ERR(%s[%d]):videoSizeLut is NULL", __FUNCTION__, __LINE__);
            return sizeList;
        } else if (m_staticInfo->videoSizeLutMax <= m_cameraInfo.previewSizeRatioId) {
            ALOGE("ERR(%s[%d]):unsupported video ratioId(%d)",
                    __FUNCTION__, __LINE__, m_cameraInfo.previewSizeRatioId);
            return sizeList;
        }

        sizeList = m_staticInfo->videoSizeLut[m_cameraInfo.previewSizeRatioId];
    }

    return sizeList;
}

int *ExynosCameraParameters::getBinningSizeTable(void) {
    int *sizeList = NULL;
#ifdef USE_LIVE_BROADCAST
    bool plb_mode = getPLBMode();

    CLOGV("(%s[%d]): plb_mode(%d), BinningMode(%d)",
        __FUNCTION__, __LINE__, plb_mode, getBinningMode());

    if (plb_mode == true) {
        int index = 0;

        if (m_staticInfo->liveBroadcastSizeLut == NULL
            || m_staticInfo->liveBroadcastSizeLutMax == 0) {
            ALOGE("ERR(%s[%d]):liveBroadcastSizeLut is NULL", __FUNCTION__, __LINE__);
            return sizeList;
        }

        for (index = 0; index < m_staticInfo->liveBroadcastSizeLutMax; index++) {
            if (m_staticInfo->liveBroadcastSizeLut[index][0] == m_cameraInfo.previewSizeRatioId)
                break;
        }

        if (m_staticInfo->liveBroadcastSizeLutMax <= index) {
            ALOGE("ERR(%s[%d]):unsupported binningSize ratioId(%d)",
                    __FUNCTION__, __LINE__, m_cameraInfo.previewSizeRatioId);
            sizeList = NULL;
        } else {
            sizeList = m_staticInfo->liveBroadcastSizeLut[index];
        }
    } else
#endif
    {
        /*                                                                        */
        /*   VT mode                                                        */
        /*   1: 3G vtmode (176x144, Fixed 7fps)                 */
        /*   2: LTE or WIFI vtmode (640x480, Fixed 15fps)   */
        /*   3: LSI chip not used this mode                          */
        /*   4: CHN vtmode (1920X1080, Fixed 30fps)          */
        /*                                                                         */
        int index = 0;

        if (m_staticInfo->vtcallSizeLut == NULL
            || m_staticInfo->vtcallSizeLutMax == 0) {
            ALOGE("ERR(%s[%d]):vtcallSizeLut is NULL", __FUNCTION__, __LINE__);
            return sizeList;
        }

        for (index = 0; index < m_staticInfo->vtcallSizeLutMax; index++) {
            if (m_staticInfo->vtcallSizeLut[index][0] == m_cameraInfo.previewSizeRatioId)
                break;
        }

        if (m_staticInfo->vtcallSizeLutMax <= index)
            index = 0;

        sizeList = m_staticInfo->vtcallSizeLut[index];
    }

    return sizeList;
}

int *ExynosCameraParameters::getHighSpeedSizeTable(int fpsMode) {
    int index = 0;
    int *sizeList = NULL;
    int videoW = 0, videoH = 0;
    int videoRatioEnum = SIZE_RATIO_16_9;
    float videoRatio = 0.00f;

    /*
        SIZE_RATIO_16_9 = 0,
        SIZE_RATIO_4_3,
        SIZE_RATIO_1_1,
        SIZE_RATIO_3_2,
        SIZE_RATIO_5_4,
        SIZE_RATIO_5_3,
        SIZE_RATIO_11_9,
      */
    getVideoSize(&videoW, &videoH);
    videoRatio = ROUND_OFF(((float)videoW / (float)videoH), 2);
    if (videoRatio == 1.33f) { /* 4:3 */
        videoRatioEnum = SIZE_RATIO_4_3;
    } else if (videoRatio == 1.77f) { /* 16:9 */
        videoRatioEnum = SIZE_RATIO_16_9;
    } else if (videoRatio == 1.00f) { /* 1:1 */
        videoRatioEnum = SIZE_RATIO_1_1;
    } else if (videoRatio == 1.50f) { /* 3:2 */
        videoRatioEnum = SIZE_RATIO_3_2;
    } else if (videoRatio == 1.25f) { /* 5:4 */
        videoRatioEnum = SIZE_RATIO_5_4;
    } else if (videoRatio == 1.66f) { /* 5:3*/
        videoRatioEnum = SIZE_RATIO_5_3;
    } else if (videoRatio == 1.22f) { /* 11:9 */
        videoRatioEnum = SIZE_RATIO_11_9;
    } else {
        videoRatioEnum = SIZE_RATIO_16_9;
    }

    switch(fpsMode) {
        case CONFIG_MODE::HIGHSPEED_60:
            if (m_staticInfo->videoSizeLutHighSpeed60Max == 0
                || m_staticInfo->videoSizeLutHighSpeed60 == NULL) {
                ALOGE("ERR(%s[%d]):videoSizeLutHighSpeed is NULL.fpsMode(%d)", __FUNCTION__, __LINE__, fpsMode);
                sizeList = NULL;
            } else {
                for (index = 0; index < m_staticInfo->videoSizeLutHighSpeed60Max; index++) {
                    if (m_staticInfo->videoSizeLutHighSpeed60[index][RATIO_ID] == videoRatioEnum) {
                        break;
                    }
                }

                if (index >= m_staticInfo->videoSizeLutHighSpeed60Max)
                    index = 0;

                sizeList = m_staticInfo->videoSizeLutHighSpeed60[index];
            }
            break;
        case CONFIG_MODE::HIGHSPEED_120:
            if (m_staticInfo->videoSizeLutHighSpeed120Max == 0
                || m_staticInfo->videoSizeLutHighSpeed120 == NULL) {
                ALOGE("ERR(%s[%d]):videoSizeLutHighSpeed is NULL.fpsMode(%d)", __FUNCTION__, __LINE__, fpsMode);
                sizeList = NULL;
            } else {
                for (index = 0; index < m_staticInfo->videoSizeLutHighSpeed120Max; index++) {
                    if (m_staticInfo->videoSizeLutHighSpeed120[index][RATIO_ID] == videoRatioEnum) {
                        break;
                    }
                }

                if (index >= m_staticInfo->videoSizeLutHighSpeed120Max)
                    index = 0;

                sizeList = m_staticInfo->videoSizeLutHighSpeed120[index];
            }
            break;
        default :
            ALOGE("ERR(%s[%d]):getConfigMode : fpsmode(%d) fail", __FUNCTION__, __LINE__, fpsMode);
            break;
    }

    return sizeList;
}

status_t ExynosCameraParameters::m_getPreviewSizeList(int *sizeList)
{
    int *tempSizeList = NULL;
    int configMode = -1;
    int index = 0;
    int videoRatioEnum = SIZE_RATIO_16_9;

    if (getHalVersion() == IS_HAL_VER_3_2) {
        /* CAMERA2_API use Video Scenario LUT as a default */
        if (m_staticInfo->videoSizeLut == NULL) {
            ALOGE("ERR(%s[%d]):videoSizeLut is NULL", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        } else if (m_staticInfo->videoSizeLutMax <= m_cameraInfo.previewSizeRatioId) {
            ALOGE("ERR(%s[%d]):unsupported video ratioId(%d)",
                    __FUNCTION__, __LINE__, m_cameraInfo.previewSizeRatioId);
            return BAD_VALUE;
        }
#if defined(ENABLE_8MP_FULL_FRAME)
        //tempSizeList = m_staticInfo->videoSizeLut[m_cameraInfo.previewSizeRatioId];
        configMode = this->getConfigMode();
        switch (configMode) {
        case CONFIG_MODE::NORMAL:
            tempSizeList = m_staticInfo->videoSizeLut[m_cameraInfo.previewSizeRatioId];
            break;
        case CONFIG_MODE::HIGHSPEED_120:
            if (m_staticInfo->videoSizeLutHighSpeed120Max == 0
                || m_staticInfo->videoSizeLutHighSpeed120 == NULL) {
                ALOGE("ERR(%s[%d]):videoSizeLutHighSpeed is NULL ", __FUNCTION__, __LINE__);
                tempSizeList = NULL;
            } else {
                for (index = 0; index < m_staticInfo->videoSizeLutHighSpeed120Max; index++) {
                    if (m_staticInfo->videoSizeLutHighSpeed120[index][RATIO_ID] == videoRatioEnum) {
                        break;
                    }
                }

                if (index >= m_staticInfo->videoSizeLutHighSpeed120Max)
                    index = 0;

                tempSizeList = m_staticInfo->videoSizeLutHighSpeed120[index];
            }
            break;
        }
#else
        tempSizeList = m_staticInfo->previewSizeLut[m_cameraInfo.previewSizeRatioId];
#endif
    } else {
        if (getDualMode() == true) {
            tempSizeList = getDualModeSizeTable();
        } else { /* getDualMode() == false */
            if (getRecordingHint() == true) {
                int videoW = 0, videoH = 0;
                getVideoSize(&videoW, &videoH);
                if (getHighSpeedRecording() == true) {
                    int fpsmode = 0;

                    fpsmode = getConfigMode();
                    tempSizeList = getHighSpeedSizeTable(fpsmode);
                }
#ifdef USE_BNS_RECORDING
                else if (m_staticInfo->videoSizeBnsLut != NULL
                         && videoW == 1920 && videoH == 1080) { /* Use BNS Recording only for FHD(16:9) */
                    tempSizeList = getBnsRecordingSizeTable();
                }
#endif
                else { /* Normal Recording Mode */
                    tempSizeList = getNormalRecordingSizeTable();
                }
            }
#ifdef USE_BINNING_MODE
            else if (getBinningMode() == true) {
                tempSizeList = getBinningSizeTable();
            }
#endif
            else { /* Use Preview LUT */
                if (m_staticInfo->previewSizeLut == NULL) {
                    ALOGE("ERR(%s[%d]):previewSizeLut is NULL", __FUNCTION__, __LINE__);
                    return INVALID_OPERATION;
                } else if (m_staticInfo->previewSizeLutMax <= m_cameraInfo.previewSizeRatioId) {
                    ALOGE("ERR(%s[%d]):unsupported preview ratioId(%d)",
                            __FUNCTION__, __LINE__, m_cameraInfo.previewSizeRatioId);
                    return BAD_VALUE;
                }

                tempSizeList = m_staticInfo->previewSizeLut[m_cameraInfo.previewSizeRatioId];
            }
        }
    }

    if (tempSizeList == NULL) {
        ALOGE("ERR(%s[%d]):fail to get LUT", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    for (int i = 0; i < SIZE_LUT_INDEX_END; i++)
        sizeList[i] = tempSizeList[i];

    return NO_ERROR;
}

void ExynosCameraParameters::m_getSWVdisPreviewSize(int w, int h, int *newW, int *newH)
{
    if (w < 0 || h < 0) {
        return;
    }

    if (w == 1920 && h == 1080) {
        *newW = 2304;
        *newH = 1296;
    }
    else if (w == 1280 && h == 720) {
        *newW = 1536;
        *newH = 864;
    }
    else {
        *newW = ALIGN_UP((w * 6) / 5, CAMERA_ISP_ALIGN);
        *newH = ALIGN_UP((h * 6) / 5, CAMERA_ISP_ALIGN);
    }
}

bool ExynosCameraParameters::m_isHighResolutionCallbackSize(const int width, const int height)
{
    bool highResolutionCallbackMode;

    if (width == m_staticInfo->highResolutionCallbackW && height == m_staticInfo->highResolutionCallbackH)
        highResolutionCallbackMode = true;
    else
        highResolutionCallbackMode = false;

    CLOGD("DEBUG(%s):highResolutionCallSize:%s", "setParameters",
        highResolutionCallbackMode == true? "on":"off");

    m_setHighResolutionCallbackMode(highResolutionCallbackMode);

    return highResolutionCallbackMode;
}

void ExynosCameraParameters::m_isHighResolutionMode(const CameraParameters& params)
{
    bool highResolutionCallbackMode;
    int shotmode = params.getInt("shot-mode");

    if ((getRecordingHint() == false) && (shotmode == SHOT_MODE_PANORAMA))
        highResolutionCallbackMode = true;
    else
        highResolutionCallbackMode = false;

    ALOGD("DEBUG(%s):highResolutionMode:%s", "setParameters",
        highResolutionCallbackMode == true? "on":"off");

    m_setHighResolutionCallbackMode(highResolutionCallbackMode);
}

void ExynosCameraParameters::m_setHighResolutionCallbackMode(bool enable)
{
    m_cameraInfo.highResolutionCallbackMode = enable;
}

bool ExynosCameraParameters::getHighResolutionCallbackMode(void)
{
    return m_cameraInfo.highResolutionCallbackMode;
}

status_t ExynosCameraParameters::checkPreviewFormat(const CameraParameters& params)
{
    const char *strNewPreviewFormat = params.getPreviewFormat();
    const char *strCurPreviewFormat = m_params.getPreviewFormat();
    int curHwPreviewFormat = getHwPreviewFormat();
    int newPreviewFormat = 0;
    int hwPreviewFormat = 0;

    CLOGD("DEBUG(%s):newPreviewFormat: %s", "setParameters", strNewPreviewFormat);

    if (!strcmp(strNewPreviewFormat, CameraParameters::PIXEL_FORMAT_RGB565))
        newPreviewFormat = V4L2_PIX_FMT_RGB565;
    else if (!strcmp(strNewPreviewFormat, CameraParameters::PIXEL_FORMAT_RGBA8888))
        newPreviewFormat = V4L2_PIX_FMT_RGB32;
    else if (!strcmp(strNewPreviewFormat, CameraParameters::PIXEL_FORMAT_YUV420SP))
        newPreviewFormat = V4L2_PIX_FMT_NV21;
    else if (!strcmp(strNewPreviewFormat, CameraParameters::PIXEL_FORMAT_YUV420P))
        newPreviewFormat = V4L2_PIX_FMT_YVU420;
    else if (!strcmp(strNewPreviewFormat, "yuv420sp_custom"))
        newPreviewFormat = V4L2_PIX_FMT_NV12T;
    else if (!strcmp(strNewPreviewFormat, "yuv422i"))
        newPreviewFormat = V4L2_PIX_FMT_YUYV;
    else if (!strcmp(strNewPreviewFormat, "yuv422p"))
        newPreviewFormat = V4L2_PIX_FMT_YUV422P;
    else
        newPreviewFormat = V4L2_PIX_FMT_NV21; /* for 3rd party */

    if (m_adjustPreviewFormat(newPreviewFormat, hwPreviewFormat) != NO_ERROR) {
        return BAD_VALUE;
    }

    m_setPreviewFormat(newPreviewFormat);
    m_params.setPreviewFormat(strNewPreviewFormat);
    if (curHwPreviewFormat != hwPreviewFormat) {
        m_setHwPreviewFormat(hwPreviewFormat);
        CLOGI("INFO(%s[%d]): preview format changed cur(%s) -> new(%s)", "Parameters", __LINE__, strCurPreviewFormat, strNewPreviewFormat);

        if (getPreviewRunning() == true) {
            CLOGD("DEBUG(%s[%d]):setRestartPreviewChecked true", __FUNCTION__, __LINE__);
            m_setRestartPreviewChecked(true);
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraParameters::m_adjustPreviewFormat(__unused int &previewFormat, __unused int &hwPreviewFormat)
{
#if 1
    /* HACK : V4L2_PIX_FMT_NV21M is set to FIMC-IS  *
     * and Gralloc. V4L2_PIX_FMT_YVU420 is just     *
     * color format for callback frame.             */
    hwPreviewFormat = V4L2_PIX_FMT_NV21M;
#else
    if (previewFormat == V4L2_PIX_FMT_NV21)
        hwPreviewFormat = V4L2_PIX_FMT_NV21M;
    else if (previewFormat == V4L2_PIX_FMT_YVU420)
        hwPreviewFormat = V4L2_PIX_FMT_YVU420M;
#endif

    return NO_ERROR;
}

void ExynosCameraParameters::m_setPreviewSize(int w, int h)
{
    m_cameraInfo.previewW = w;
    m_cameraInfo.previewH = h;
}

void ExynosCameraParameters::getPreviewSize(int *w, int *h)
{
    *w = m_cameraInfo.previewW;
    *h = m_cameraInfo.previewH;
}

void ExynosCameraParameters::getMaxSensorSize(int *w, int *h)
{
    *w = m_staticInfo->maxSensorW;
    *h = m_staticInfo->maxSensorH;
}

void ExynosCameraParameters::getSensorMargin(int *w, int *h)
{
    *w = m_staticInfo->sensorMarginW;
    *h = m_staticInfo->sensorMarginH;
}

void ExynosCameraParameters::m_adjustSensorMargin(int *sensorMarginW, int *sensorMarginH)
{
    float bnsRatio = 1.00f;
    float binningRatio = 1.00f;
    float sensorMarginRatio = 1.00f;

    bnsRatio = (float)getBnsScaleRatio() / 1000.00f;
    binningRatio = (float)getBinningScaleRatio() / 1000.00f;
    sensorMarginRatio = bnsRatio * binningRatio;
    if ((int)sensorMarginRatio < 1) {
        ALOGW("WARN(%s[%d]):Invalid sensor margin ratio(%f), bnsRatio(%f), binningRatio(%f)",
                __FUNCTION__, __LINE__, sensorMarginRatio, bnsRatio, binningRatio);
        sensorMarginRatio = 1.00f;
    }

    if (getHalVersion() != IS_HAL_VER_3_2) {
        *sensorMarginW = ALIGN_DOWN((int)(*sensorMarginW / sensorMarginRatio), 2);
        *sensorMarginH = ALIGN_DOWN((int)(*sensorMarginH / sensorMarginRatio), 2);
    } else {
       int leftMargin = 0, rightMargin = 0, topMargin = 0, bottomMargin = 0;

       rightMargin = ALIGN_DOWN((int)(m_staticInfo->sensorMarginBase[WIDTH_BASE] / sensorMarginRatio), 2);
       leftMargin = m_staticInfo->sensorMarginBase[LEFT_BASE] + rightMargin;
       bottomMargin = ALIGN_DOWN((int)(m_staticInfo->sensorMarginBase[HEIGHT_BASE] / sensorMarginRatio), 2);
       topMargin = m_staticInfo->sensorMarginBase[TOP_BASE] + bottomMargin;

       *sensorMarginW = leftMargin + rightMargin;
       *sensorMarginH = topMargin + bottomMargin;
    }
}

void ExynosCameraParameters::getMaxPreviewSize(int *w, int *h)
{
    *w = m_staticInfo->maxPreviewW;
    *h = m_staticInfo->maxPreviewH;
}

void ExynosCameraParameters::m_setPreviewFormat(int fmt)
{
    m_cameraInfo.previewFormat = fmt;
}

int ExynosCameraParameters::getPreviewFormat(void)
{
    return m_cameraInfo.previewFormat;
}

void ExynosCameraParameters::m_setHwPreviewSize(int w, int h)
{
    m_cameraInfo.hwPreviewW = w;
    m_cameraInfo.hwPreviewH = h;
}

void ExynosCameraParameters::getHwPreviewSize(int *w, int *h)
{
    if (m_cameraInfo.scalableSensorMode != true) {
        *w = m_cameraInfo.hwPreviewW;
        *h = m_cameraInfo.hwPreviewH;
    } else {
        int newSensorW  = 0;
        int newSensorH = 0;
        m_getScalableSensorSize(&newSensorW, &newSensorH);

        *w = newSensorW;
        *h = newSensorH;
/*
 *    Should not use those value
 *    *w = 1024;
 *    *h = 768;
 *    *w = 1440;
 *    *h = 1080;
 */
        *w = m_cameraInfo.hwPreviewW;
        *h = m_cameraInfo.hwPreviewH;
    }
}

void ExynosCameraParameters::setHwPreviewStride(int stride)
{
    m_cameraInfo.previewStride = stride;
}

int ExynosCameraParameters::getHwPreviewStride(void)
{
    return m_cameraInfo.previewStride;
}

void ExynosCameraParameters::m_setHwPreviewFormat(int fmt)
{
    m_cameraInfo.hwPreviewFormat = fmt;
}

int ExynosCameraParameters::getHwPreviewFormat(void)
{
    return m_cameraInfo.hwPreviewFormat;
}

void ExynosCameraParameters::updateHwSensorSize(void)
{
    int curHwSensorW = 0;
    int curHwSensorH = 0;
    int newHwSensorW = 0;
    int newHwSensorH = 0;
    int maxHwSensorW = 0;
    int maxHwSensorH = 0;

    getHwSensorSize(&newHwSensorW, &newHwSensorH);
    getMaxSensorSize(&maxHwSensorW, &maxHwSensorH);

    if (newHwSensorW > maxHwSensorW || newHwSensorH > maxHwSensorH) {
        CLOGE("ERR(%s):Invalid sensor size (maxSize(%d/%d) size(%d/%d)",
        __FUNCTION__, maxHwSensorW, maxHwSensorH, newHwSensorW, newHwSensorH);
    }

    if (getHighSpeedRecording() == true) {
#if 0
        int sizeList[SIZE_LUT_INDEX_END];
        m_getHighSpeedRecordingSize(sizeList);
        newHwSensorW = sizeList[SENSOR_W];
        newHwSensorH = sizeList[SENSOR_H];
#endif
    } else if (getScalableSensorMode() == true) {
        m_getScalableSensorSize(&newHwSensorW, &newHwSensorH);
    } else {
        getBnsSize(&newHwSensorW, &newHwSensorH);
    }

    getHwSensorSize(&curHwSensorW, &curHwSensorH);
    CLOGI("INFO(%s):curHwSensor size(%dx%d) newHwSensor size(%dx%d)", __FUNCTION__, curHwSensorW, curHwSensorH, newHwSensorW, newHwSensorH);
    if (curHwSensorW != newHwSensorW || curHwSensorH != newHwSensorH) {
        m_setHwSensorSize(newHwSensorW, newHwSensorH);
        CLOGI("INFO(%s):newHwSensor size(%dx%d)", __FUNCTION__, newHwSensorW, newHwSensorH);
    }
}

void ExynosCameraParameters::m_setHwSensorSize(int w, int h)
{
    m_cameraInfo.hwSensorW = w;
    m_cameraInfo.hwSensorH = h;
}

void ExynosCameraParameters::getHwSensorSize(int *w, int *h)
{
    ALOGV("INFO(%s[%d]) getScalableSensorMode()(%d)", __FUNCTION__, __LINE__, getScalableSensorMode());
    int width  = 0;
    int height = 0;
    int sizeList[SIZE_LUT_INDEX_END];

    if (m_cameraInfo.scalableSensorMode != true) {
        /* matched ratio LUT is not existed, use equation */
        if (m_useSizeTable == true
            && m_staticInfo->previewSizeLut != NULL
            && m_cameraInfo.previewSizeRatioId < m_staticInfo->previewSizeLutMax
            && m_getPreviewSizeList(sizeList) == NO_ERROR) {

            width = sizeList[SENSOR_W];
            height = sizeList[SENSOR_H];

        } else {
            width  = m_cameraInfo.hwSensorW;
            height = m_cameraInfo.hwSensorH;
        }
    } else {
        m_getScalableSensorSize(&width, &height);
    }

    *w = width;
    *h = height;
}

void ExynosCameraParameters::updateBnsScaleRatio(void)
{
    int ret = 0;
    uint32_t bnsRatio = DEFAULT_BNS_RATIO * 1000;
    int curPreviewW = 0, curPreviewH = 0;

    if (m_staticInfo->bnsSupport == false)
        return;

    getPreviewSize(&curPreviewW, &curPreviewH);
    if (getDualMode() == true
#ifdef USE_LIVE_BROADCAST_DUAL
        && !getPLBMode()
#endif
    ) {
#if defined(USE_BNS_DUAL_PREVIEW) || defined(USE_BNS_DUAL_RECORDING)
        bnsRatio = 2000;
#endif
    } else if ((getRecordingHint() == true)
/*    || (curPreviewW == curPreviewH)*/) {
#ifdef USE_BNS_RECORDING
        int videoW = 0, videoH = 0;
        getVideoSize(&videoW, &videoH);

        if ((getHighSpeedRecording() == true)) {
            bnsRatio = 1000;
        } else if (videoW == 1920 && videoH == 1080) {
            bnsRatio = 1500;
            ALOGI("INFO(%s[%d]):bnsRatio(%d), videoSize (%d, %d)",
                __FUNCTION__, __LINE__, bnsRatio, videoW, videoH);
        } else
#endif
        {
            bnsRatio = 1000;
        }
        if (bnsRatio != getBnsScaleRatio()) {
            CLOGI("INFO(%s[%d]): restart set due to changing  bnsRatio(%d/%d)",
                __FUNCTION__, __LINE__, bnsRatio, getBnsScaleRatio());
            m_setRestartPreview(true);
        }
    }
#ifdef USE_BINNING_MODE
    else if (getBinningMode() == true) {
        bnsRatio = 1000;
    }
#endif

    if (bnsRatio != getBnsScaleRatio())
        ret = m_setBnsScaleRatio(bnsRatio);

    if (ret < 0)
        CLOGE("ERR(%s[%d]): Cannot update BNS scale ratio(%d)", __FUNCTION__, __LINE__, bnsRatio);
}

status_t ExynosCameraParameters::m_setBnsScaleRatio(int ratio)
{
#define MIN_BNS_RATIO 1000
#define MAX_BNS_RATIO 8000

    if (m_staticInfo->bnsSupport == false) {
        CLOGD("DEBUG(%s[%d]): This camera does not support BNS", __FUNCTION__, __LINE__);
        ratio = MIN_BNS_RATIO;
    }

    if (ratio < MIN_BNS_RATIO || ratio > MAX_BNS_RATIO) {
        CLOGE("ERR(%s[%d]): Out of bound, ratio(%d), min:max(%d:%d)", __FUNCTION__, __LINE__, ratio, MAX_BNS_RATIO, MAX_BNS_RATIO);
        return BAD_VALUE;
    }

    CLOGD("DEBUG(%s[%d]): update BNS ratio(%d -> %d)", __FUNCTION__, __LINE__, m_cameraInfo.bnsScaleRatio, ratio);

    m_cameraInfo.bnsScaleRatio = ratio;

    /* When BNS scale ratio is changed, reset BNS size to MAX sensor size */
    getMaxSensorSize(&m_cameraInfo.bnsW, &m_cameraInfo.bnsH);

    return NO_ERROR;
}

status_t ExynosCameraParameters::m_addHiddenResolutionList(String8 &string8Buf, __unused struct ExynosSensorInfoBase *sensorInfo,
                                   int w, int h, enum MODE mode, int cameraId)

{
    status_t ret = NO_ERROR;
    bool found = false;

    int (*sizeList)[SIZE_OF_RESOLUTION];
    int listSize = 0;

    switch (mode) {
    case MODE_PREVIEW:
        if (cameraId == CAMERA_ID_BACK) {
            sizeList = m_staticInfo->hiddenRearPreviewList;
            listSize = m_staticInfo->hiddenRearPreviewListMax;
        } else {
            sizeList = m_staticInfo->hiddenFrontPreviewList;
            listSize = m_staticInfo->hiddenFrontPreviewListMax;
        }
        break;
    case MODE_PICTURE:
        if (cameraId == CAMERA_ID_BACK) {
            sizeList = m_staticInfo->hiddenRearPictureList;
            listSize = m_staticInfo->hiddenRearPictureListMax;
        } else {
            sizeList = m_staticInfo->hiddenFrontPictureList;
            listSize = m_staticInfo->hiddenFrontPictureListMax;
        }
        break;
    case MODE_VIDEO:
        if (cameraId == CAMERA_ID_BACK) {
            sizeList = m_staticInfo->hiddenRearVideoList;
            listSize = m_staticInfo->hiddenRearVideoListMax;
        } else {
            sizeList = m_staticInfo->hiddenFrontVideoList;
            listSize = m_staticInfo->hiddenFrontVideoListMax;
        }
        break;
    default:
        CLOGE("ERR(%s[%d]): invalid mode(%d)", __FUNCTION__, __LINE__, mode);
        return BAD_VALUE;
        break;
    }

    for (int i = 0; i < listSize; i++) {
        if (w == sizeList[i][0] && h == sizeList[i][1]) {
            found = true;
            break;
        }
    }

    if (found == true) {
        String8 uhdTempStr;
        char strBuf[32];

        snprintf(strBuf, sizeof(strBuf), "%dx%d,", w, h);

        /* append on head of string8Buf */
        uhdTempStr.setTo(strBuf);
        uhdTempStr.append(string8Buf);
        string8Buf.setTo(uhdTempStr);
    } else {
        ret = INVALID_OPERATION;
    }

    return ret;
}

uint32_t ExynosCameraParameters::getBnsScaleRatio(void)
{
    return m_cameraInfo.bnsScaleRatio;
}

void ExynosCameraParameters::setBnsSize(int w, int h)
{
    m_cameraInfo.bnsW = w;
    m_cameraInfo.bnsH = h;

    updateHwSensorSize();

#if 0
    int zoom = getZoomLevel();
    int previewW = 0, previewH = 0;
    getPreviewSize(&previewW, &previewH);
    if (m_setParamCropRegion(zoom, w, h, previewW, previewH) != NO_ERROR)
        CLOGE("ERR(%s):m_setParamCropRegion() fail", __FUNCTION__);
#else
    ExynosRect srcRect, dstRect;
    getPreviewBayerCropSize(&srcRect, &dstRect);
#endif
}

void ExynosCameraParameters::getBnsSize(int *w, int *h)
{
    *w = m_cameraInfo.bnsW;
    *h = m_cameraInfo.bnsH;
}

void ExynosCameraParameters::updateBinningScaleRatio(void)
{
    int ret = 0;
    uint32_t binningRatio = DEFAULT_BINNING_RATIO * 1000;

    if ((getRecordingHint() == true)
        && (getHighSpeedRecording() == true)) {
        int fpsmode = 0;
        fpsmode = getFastFpsMode();
        switch (fpsmode) {
        case 1: /* 60 fps */
            binningRatio = 2000;
            break;
        case 2: /* 120 fps */
        case 3: /* 240 fps */
            binningRatio = 4000;
            break;
        default:
            ALOGE("ERR(%s[%d]): Invalide FastFpsMode(%d)", __FUNCTION__, __LINE__, fpsmode);
        }
    }
#ifdef USE_BINNING_MODE
    else if (getBinningMode() == true) {
        binningRatio = 2000;
    }
#endif

    if (binningRatio != getBinningScaleRatio()) {
        ALOGI("INFO(%s[%d]):New sensor binning ratio(%d)", __FUNCTION__, __LINE__, binningRatio);
        ret = m_setBinningScaleRatio(binningRatio);
    }

    if (ret < 0)
        ALOGE("ERR(%s[%d]): Cannot update BNS scale ratio(%d)", __FUNCTION__, __LINE__, binningRatio);
}

status_t ExynosCameraParameters::m_setBinningScaleRatio(int ratio)
{
#define MIN_BINNING_RATIO 1000
#define MAX_BINNING_RATIO 6000

    if (ratio < MIN_BINNING_RATIO || ratio > MAX_BINNING_RATIO) {
        ALOGE("ERR(%s[%d]): Out of bound, ratio(%d), min:max(%d:%d)",
                __FUNCTION__, __LINE__, ratio, MAX_BINNING_RATIO, MAX_BINNING_RATIO);
        return BAD_VALUE;
    }

    m_cameraInfo.binningScaleRatio = ratio;

    return NO_ERROR;
}

uint32_t ExynosCameraParameters::getBinningScaleRatio(void)
{
    return m_cameraInfo.binningScaleRatio;
}

status_t ExynosCameraParameters::checkPictureSize(const CameraParameters& params)
{
    int curPictureW = 0;
    int curPictureH = 0;
    int newPictureW = 0;
    int newPictureH = 0;
    int curHwPictureW = 0;
    int curHwPictureH = 0;
    int newHwPictureW = 0;
    int newHwPictureH = 0;
    int right_ratio = 177;

    params.getPictureSize(&newPictureW, &newPictureH);

    if (newPictureW < 0 || newPictureH < 0) {
        return BAD_VALUE;
    }

    if (m_adjustPictureSize(&newPictureW, &newPictureH, &newHwPictureW, &newHwPictureH) != NO_ERROR) {
        return BAD_VALUE;
    }

    if (m_isSupportedPictureSize(newPictureW, newPictureH) == false) {
        int maxHwPictureW =0;
        int maxHwPictureH = 0;

        CLOGE("ERR(%s):Invalid picture size(%dx%d)", __FUNCTION__, newPictureW, newPictureH);

        /* prevent wrong size setting */
        getMaxPictureSize(&maxHwPictureW, &maxHwPictureH);
        m_setPictureSize(maxHwPictureW, maxHwPictureH);
        m_setHwPictureSize(maxHwPictureW, maxHwPictureH);
        m_params.setPictureSize(maxHwPictureW, maxHwPictureH);
        CLOGE("ERR(%s):changed picture size to MAX(%dx%d)", __FUNCTION__, maxHwPictureW, maxHwPictureH);

#ifdef FIXED_SENSOR_SIZE
        updateHwSensorSize();
#endif
        return INVALID_OPERATION;
    }
    CLOGI("INFO(%s):newPicture Size (%dx%d), ratioId(%d)",
        "setParameters", newPictureW, newPictureH, m_cameraInfo.pictureSizeRatioId);

    if ((int)(m_staticInfo->maxSensorW * 100 / m_staticInfo->maxSensorH) == right_ratio) {
        setHorizontalViewAngle(newPictureW, newPictureH);
    }
    m_params.setFloat(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, getHorizontalViewAngle());

    getPictureSize(&curPictureW, &curPictureH);
    getHwPictureSize(&curHwPictureW, &curHwPictureH);

    if (curPictureW != newPictureW || curPictureH != newPictureH ||
        curHwPictureW != newHwPictureW || curHwPictureH != newHwPictureH) {

        CLOGI("INFO(%s[%d]): Picture size changed: cur(%dx%d) -> new(%dx%d)",
                "setParameters", __LINE__, curPictureW, curPictureH, newPictureW, newPictureH);
        CLOGI("INFO(%s[%d]): HwPicture size changed: cur(%dx%d) -> new(%dx%d)",
                "setParameters", __LINE__, curHwPictureW, curHwPictureH, newHwPictureW, newHwPictureH);

        m_setPictureSize(newPictureW, newPictureH);
        m_setHwPictureSize(newHwPictureW, newHwPictureH);
        m_params.setPictureSize(newPictureW, newPictureH);

#ifdef FIXED_SENSOR_SIZE
        updateHwSensorSize();
#endif
    }

    return NO_ERROR;
}

status_t ExynosCameraParameters::m_adjustPictureSize(int *newPictureW, int *newPictureH,
                                                 int *newHwPictureW, int *newHwPictureH)
{
    int ret = 0;
    int newX = 0, newY = 0, newW = 0, newH = 0;
    float zoomRatio = getZoomRatio(0) / 1000;

    if ((getRecordingHint() == true && getHighSpeedRecording() == true)
#ifdef USE_BINNING_MODE
        || getBinningMode()
#endif
       )
    {
       int sizeList[SIZE_LUT_INDEX_END];
       if (m_getPreviewSizeList(sizeList) == NO_ERROR) {
           *newPictureW = sizeList[TARGET_W];
           *newPictureH = sizeList[TARGET_H];
           *newHwPictureW = *newPictureW;
           *newHwPictureH = *newPictureH;

           return NO_ERROR;
       } else {
           ALOGE("ERR(%s):m_getPreviewSizeList() fail", __FUNCTION__);
           return BAD_VALUE;
       }
    }

    getMaxPictureSize(newHwPictureW, newHwPictureH);

    if (getCameraId() == CAMERA_ID_BACK) {
        ret = getCropRectAlign(*newHwPictureW, *newHwPictureH,
                *newPictureW, *newPictureH,
                &newX, &newY, &newW, &newH,
                CAMERA_ISP_ALIGN, 2, 0, zoomRatio);
        if (ret < 0) {
            CLOGE("ERR(%s):getCropRectAlign(%d, %d, %d, %d) fail",
                    __FUNCTION__, *newHwPictureW, *newHwPictureH, *newPictureW, *newPictureH);
            return BAD_VALUE;
        }
        *newHwPictureW = newW;
        *newHwPictureH = newH;

#ifdef FIXED_SENSOR_SIZE
        /*
         * sensor crop size:
         * sensor crop is only used at 16:9 aspect ratio in picture size.
         */
        if (getSamsungCamera() == true) {
            if (((float)*newPictureW / (float)*newPictureH) == ((float)16 / (float)9)) {
                CLOGD("(%s): Use sensor crop (ratio: %f)",
                        __FUNCTION__, ((float)*newPictureW / (float)*newPictureH));
                m_setHwSensorSize(newW, newH);
            }
        }
#endif
    }

    return NO_ERROR;
}

bool ExynosCameraParameters::m_isSupportedPictureSize(const int width,
                                                     const int height)
{
    int maxWidth, maxHeight = 0;
    int (*sizeList)[SIZE_OF_RESOLUTION];

    getMaxPictureSize(&maxWidth, &maxHeight);

    if (maxWidth < width || maxHeight < height) {
        CLOGE("ERR(%s):invalid picture Size(maxSize(%d/%d) size(%d/%d)",
            __FUNCTION__, maxWidth, maxHeight, width, height);
        return false;
    }

    if (getCameraId() == CAMERA_ID_BACK) {
        sizeList = m_staticInfo->rearPictureList;
        for (int i = 0; i < m_staticInfo->rearPictureListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.pictureSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    } else {
        sizeList = m_staticInfo->frontPictureList;
        for (int i = 0; i < m_staticInfo->frontPictureListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.pictureSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    }

    if (getCameraId() == CAMERA_ID_BACK) {
        sizeList = m_staticInfo->hiddenRearPictureList;
        for (int i = 0; i < m_staticInfo->hiddenRearPictureListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.pictureSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    } else {
        sizeList = m_staticInfo->hiddenFrontPictureList;
        for (int i = 0; i < m_staticInfo->hiddenFrontPictureListMax; i++) {
            if (sizeList[i][0] > maxWidth || sizeList[i][1] > maxHeight)
                continue;
            if (sizeList[i][0] == width && sizeList[i][1] == height) {
                m_cameraInfo.pictureSizeRatioId = sizeList[i][2];
                return true;
            }
        }
    }

    CLOGE("ERR(%s):Invalid picture size(%dx%d)", __FUNCTION__, width, height);

    return false;
}

void ExynosCameraParameters::m_setPictureSize(int w, int h)
{
    m_cameraInfo.pictureW = w;
    m_cameraInfo.pictureH = h;
}

void ExynosCameraParameters::getPictureSize(int *w, int *h)
{
    *w = m_cameraInfo.pictureW;
    *h = m_cameraInfo.pictureH;
}

void ExynosCameraParameters::getMaxPictureSize(int *w, int *h)
{
    *w = m_staticInfo->maxPictureW;
    *h = m_staticInfo->maxPictureH;
}

void ExynosCameraParameters::m_setHwPictureSize(int w, int h)
{
    m_cameraInfo.hwPictureW = w;
    m_cameraInfo.hwPictureH = h;
}

void ExynosCameraParameters::getHwPictureSize(int *w, int *h)
{
    *w = m_cameraInfo.hwPictureW;
    *h = m_cameraInfo.hwPictureH;
}

void ExynosCameraParameters::m_setHwBayerCropRegion(int w, int h, int x, int y)
{
    Mutex::Autolock lock(m_parameterLock);

    m_cameraInfo.hwBayerCropW = w;
    m_cameraInfo.hwBayerCropH = h;
    m_cameraInfo.hwBayerCropX = x;
    m_cameraInfo.hwBayerCropY = y;
}

void ExynosCameraParameters::getHwBayerCropRegion(int *w, int *h, int *x, int *y)
{
    Mutex::Autolock lock(m_parameterLock);

    *w = m_cameraInfo.hwBayerCropW;
    *h = m_cameraInfo.hwBayerCropH;
    *x = m_cameraInfo.hwBayerCropX;
    *y = m_cameraInfo.hwBayerCropY;
}

status_t ExynosCameraParameters::checkPictureFormat(const CameraParameters& params)
{
    int curPictureFormat = 0;
    int newPictureFormat = 0;
    const char *strNewPictureFormat = params.getPictureFormat();
    const char *strCurPictureFormat = m_params.getPictureFormat();
#ifdef SAMSUNG_DNG
    int shotMode = params.getInt("shot-mode");
    setDNGCaptureModeOn(false);
#endif

    if (strNewPictureFormat == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):newPictureFormat %s", "setParameters", strNewPictureFormat);

    if (!strcmp(strNewPictureFormat, CameraParameters::PIXEL_FORMAT_JPEG)) {
        newPictureFormat = SCC_OUTPUT_COLOR_FMT;
    }
#ifdef SAMSUNG_DNG
    else if (!strcmp(strNewPictureFormat, "raw+jpeg")) {
        newPictureFormat = SCC_OUTPUT_COLOR_FMT;
        if (shotMode == SHOT_MODE_PRO_MODE) {
            setDNGCaptureModeOn(true);
        } else {
            CLOGW("WARN(%s[%d]): Picture format(%s) is not supported!, shot mode(%d)",
                __FUNCTION__, __LINE__, strNewPictureFormat, shotMode);
        }
    }
#endif
    else {
        CLOGE("ERR(%s[%d]): Picture format(%s) is not supported!", __FUNCTION__, __LINE__, strNewPictureFormat);
        return BAD_VALUE;
    }

    curPictureFormat = getPictureFormat();

    if (newPictureFormat != curPictureFormat) {
        CLOGI("INFO(%s[%d]): Picture format changed, cur(%s) -> new(%s)", "Parameters", __LINE__, strCurPictureFormat, strNewPictureFormat);
        m_setPictureFormat(newPictureFormat);
        m_params.setPictureFormat(strNewPictureFormat);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setPictureFormat(int fmt)
{
    m_cameraInfo.pictureFormat = fmt;
}

int ExynosCameraParameters::getPictureFormat(void)
{
    return m_cameraInfo.pictureFormat;
}

status_t ExynosCameraParameters::checkJpegQuality(const CameraParameters& params)
{
    int newJpegQuality = params.getInt(CameraParameters::KEY_JPEG_QUALITY);
    int curJpegQuality = getJpegQuality();

    CLOGD("DEBUG(%s):newJpegQuality %d", "setParameters", newJpegQuality);

    if (newJpegQuality < 1 || newJpegQuality > 100) {
        CLOGE("ERR(%s): Invalid Jpeg Quality (Min: %d, Max: %d, Value: %d)", __FUNCTION__, 1, 100, newJpegQuality);
        return BAD_VALUE;
    }

    if (curJpegQuality != newJpegQuality) {
        m_setJpegQuality(newJpegQuality);
        m_params.set(CameraParameters::KEY_JPEG_QUALITY, newJpegQuality);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setJpegQuality(int quality)
{
    m_cameraInfo.jpegQuality = quality;
}

int ExynosCameraParameters::getJpegQuality(void)
{
    return m_cameraInfo.jpegQuality;
}

status_t ExynosCameraParameters::checkThumbnailSize(const CameraParameters& params)
{
    int curThumbnailW = 0;
    int curThumbnailH = 0;
    int maxThumbnailW = 0;
    int maxThumbnailH = 0;
    int newJpegThumbnailW = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    int newJpegThumbnailH = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);

    CLOGD("DEBUG(%s):newJpegThumbnailW X newJpegThumbnailH: %d X %d", "setParameters", newJpegThumbnailW, newJpegThumbnailH);

    getMaxThumbnailSize(&maxThumbnailW, &maxThumbnailH);

    if (newJpegThumbnailW < 0 || newJpegThumbnailH < 0 ||
        newJpegThumbnailW > maxThumbnailW || newJpegThumbnailH > maxThumbnailH) {
        CLOGE("ERR(%s): Invalid Thumbnail Size (maxSize(%d/%d) size(%d/%d)", __FUNCTION__, maxThumbnailW, maxThumbnailH, newJpegThumbnailW, newJpegThumbnailH);
        return BAD_VALUE;
    }

    getThumbnailSize(&curThumbnailW, &curThumbnailH);

    if (curThumbnailW != newJpegThumbnailW || curThumbnailH != newJpegThumbnailH) {
        m_setThumbnailSize(newJpegThumbnailW, newJpegThumbnailH);
        m_params.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH,  newJpegThumbnailW);
        m_params.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, newJpegThumbnailH);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setThumbnailSize(int w, int h)
{
    m_cameraInfo.thumbnailW = w;
    m_cameraInfo.thumbnailH = h;
}

void ExynosCameraParameters::getThumbnailSize(int *w, int *h)
{
    *w = m_cameraInfo.thumbnailW;
    *h = m_cameraInfo.thumbnailH;
}

void ExynosCameraParameters::getMaxThumbnailSize(int *w, int *h)
{
    *w = m_staticInfo->maxThumbnailW;
    *h = m_staticInfo->maxThumbnailH;
}

status_t ExynosCameraParameters::checkThumbnailQuality(const CameraParameters& params)
{
    int newJpegThumbnailQuality = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
    int curThumbnailQuality = getThumbnailQuality();

    CLOGD("DEBUG(%s):newJpegThumbnailQuality %d", "setParameters", newJpegThumbnailQuality);

    if (newJpegThumbnailQuality < 0 || newJpegThumbnailQuality > 100) {
        CLOGE("ERR(%s): Invalid Thumbnail Quality (Min: %d, Max: %d, Value: %d)", __FUNCTION__, 0, 100, newJpegThumbnailQuality);
        return BAD_VALUE;
    }

    if (curThumbnailQuality != newJpegThumbnailQuality) {
        m_setThumbnailQuality(newJpegThumbnailQuality);
        m_params.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, newJpegThumbnailQuality);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setThumbnailQuality(int quality)
{
    m_cameraInfo.thumbnailQuality = quality;
}

int ExynosCameraParameters::getThumbnailQuality(void)
{
    return m_cameraInfo.thumbnailQuality;
}

status_t ExynosCameraParameters::check3dnrMode(const CameraParameters& params)
{
    bool new3dnrMode = false;
    bool cur3dnrMode = false;

#if defined(USE_3DNR_ALWAYS)
    const char *str3dnrMode = (USE_3DNR_ALWAYS == true) ? "true" : params.get("3dnr");
#else
    const char *str3dnrMode = params.get("3dnr");
#endif

    if (str3dnrMode == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):new3dnrMode %s", "setParameters", str3dnrMode);

    if (!strcmp(str3dnrMode, "true"))
        new3dnrMode = true;

    if (m_flag3dnrMode != new3dnrMode) {
        m_flag3dnrMode = new3dnrMode;
        m_params.set("3dnr", str3dnrMode);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_set3dnrMode(bool toggle)
{
    m_cameraInfo.is3dnrMode = toggle;
}

bool ExynosCameraParameters::get3dnrMode(void)
{
    return m_cameraInfo.is3dnrMode;
}

bool ExynosCameraParameters::m_getFlag3dnrOffCase(void)
{
    /*
     * return true  : nevermind
     * return false : 3dnr off
     */
    bool flagOn = true;

    /* 120 fps recording, turn off 3DNR */
    uint32_t minFpsRange = 0;
    uint32_t maxFpsRange = 0;

    getPreviewFpsRange(&minFpsRange, &maxFpsRange);

#if defined(USE_3DNR_ALWAYS)
    if (USE_3DNR_ALWAYS == true) {
        if (60 < minFpsRange || 60 < maxFpsRange) {
#if defined(USE_3DNR_HIGHSPEED_RECORDING)
            flagOn = USE_3DNR_HIGHSPEED_RECORDING;
#else
            flagOn = false;
#endif /* USE_3DNR_HIGHSPEED_RECORDING */
            CLOGD("DEBUG(%s):Turn (%s) 3dnrMode by FpsRange(%d, %d)",
                "setParameters",
                (flagOn) ? "on":"off",
                minFpsRange,
                maxFpsRange);
        }
    }
#endif /* USE_3DNR_ALWAYS */

    /* dual case, turn off 3DNR */
    if (this->getDualMode() == true) {
        flagOn = false;

        CLOGD("DEBUG(%s):Turn (%s) 3dnrMode by dualMode",
            "setParameters",
            (flagOn) ? "on":"off");
    }

    /* UHD recording case, turn off 3DNR */
#if defined(USE_3DNR_ALWAYS)
    if (USE_3DNR_ALWAYS == true) {
        int hwPreviewW = 0, hwPreviewH = 0;
        getHwPreviewSize(&hwPreviewW, &hwPreviewH);

        if (3840 <= hwPreviewW && 2160 <= hwPreviewH) {
#if defined(USE_3DNR_UHDRECORDING)
            flagOn = USE_3DNR_UHDRECORDING;
#else
            flagOn = false;
#endif /* USE_3DNR_UHDRECORDING */
            CLOGD("DEBUG(%s):Turn (%s) 3dnrMode by UHD",
                "setParameters",
                (flagOn) ? "on":"off");
        }
    }
#endif /* USE_3DNR_ALWAYS */

#if defined(USE_3DNR_ALWAYS)
    if (USE_3DNR_ALWAYS == true) {
        if(getRecordingHint() == false) {
#if defined(USE_3DNR_PREVIEW)
            flagOn = USE_3DNR_PREVIEW;
#else
            flagOn = false;
#endif
            CLOGD("DEBUG(%s):Turn (%s) 3dnrMode by preview",
                "setParameters",
                (flagOn) ? "on":"off");
        }
    }
#endif /* USE_3DNR_ALWAYS */

#if defined(USE_3DNR_ALWAYS) && defined(USE_LIVE_BROADCAST)
    if (USE_3DNR_ALWAYS == true && getPLBMode() == true) {
        flagOn = false;
    }
#endif

    return flagOn;
}

status_t ExynosCameraParameters::checkDrcMode(const CameraParameters& params)
{
    bool newDrcMode = false;
    bool curDrcMode = false;
    const char *strDrcMode = params.get("drc");

    if (strDrcMode == NULL) {
#ifdef USE_FRONT_PREVIEW_DRC
        if (getCameraId() == CAMERA_ID_FRONT && m_staticInfo->drcSupport == true) {
            newDrcMode = !getRecordingHint();
            m_setDrcMode(newDrcMode);
            ALOGD("DEBUG(%s):Force DRC %s for front", "setParameters",
                    (newDrcMode == true)? "ON" : "OFF");
        }
#endif
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):newDrcMode %s", "setParameters", strDrcMode);

    if (!strcmp(strDrcMode, "true"))
        newDrcMode = true;

    curDrcMode = getDrcMode();

    if (curDrcMode != newDrcMode) {
        m_setDrcMode(newDrcMode);
        m_params.set("drc", strDrcMode);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setDrcMode(bool toggle)
{
    m_cameraInfo.isDrcMode = toggle;
    if (setDrcEnable(toggle) < 0) {
        CLOGE("ERR(%s[%d]): set DRC fail, toggle(%d)", __FUNCTION__, __LINE__, toggle);
    }
}

bool ExynosCameraParameters::getDrcMode(void)
{
    return m_cameraInfo.isDrcMode;
}

status_t ExynosCameraParameters::checkOdcMode(const CameraParameters& params)
{
    bool newOdcMode = false;
    bool curOdcMode = false;
    const char *strOdcMode = params.get("odc");

    if (strOdcMode == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):newOdcMode %s", "setParameters", strOdcMode);

    if (!strcmp(strOdcMode, "true"))
        newOdcMode = true;

    curOdcMode = getOdcMode();

    if (curOdcMode != newOdcMode) {
        m_setOdcMode(newOdcMode);
        m_params.set("odc", strOdcMode);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setOdcMode(bool toggle)
{
    m_cameraInfo.isOdcMode = toggle;
}

bool ExynosCameraParameters::getOdcMode(void)
{
    return m_cameraInfo.isOdcMode;
}

bool ExynosCameraParameters::getTpuEnabledMode(void)
{
    /*
     * This getTpuEnabledMode() func will remove.
     * but, current code use getTpuEnabledMode() many place.
     * so, just use until code stabilization.
     * and we will replace getTpuEnabledMode() to isIspTpuOtf()
     */
    bool ret = true;

    if (isIspTpuOtf() == true)
        ret = false;

    return ret;
}

status_t ExynosCameraParameters::checkZoomLevel(const CameraParameters& params)
{
    int newZoom = params.getInt(CameraParameters::KEY_ZOOM);
    int curZoom = 0;

    CLOGD("DEBUG(%s):newZoom %d", "setParameters", newZoom);

    /* cannot support DZoom -> set Zoom Level 0 */
    if (getZoomSupported() == false) {
        if (newZoom != ZOOM_LEVEL_0) {
            CLOGE("ERR(%s):Invalid value (Zoom Should be %d, Value: %d)", __FUNCTION__, ZOOM_LEVEL_0, newZoom);
            return BAD_VALUE;
        }

        if (setZoomLevel(ZOOM_LEVEL_0) != NO_ERROR)
            return BAD_VALUE;

        return NO_ERROR;
    } else {
        if (newZoom < ZOOM_LEVEL_0 || getMaxZoomLevel() <= newZoom) {
            CLOGE("ERR(%s):Invalid value (Min: %d, Max: %d, Value: %d)", __FUNCTION__, ZOOM_LEVEL_0, getMaxZoomLevel(), newZoom);
            return BAD_VALUE;
        }

        if (setZoomLevel(newZoom) != NO_ERROR) {
            return BAD_VALUE;
        }
        m_params.set(CameraParameters::KEY_ZOOM, newZoom);

        m_flagMeteringRegionChanged = true;

        return NO_ERROR;
    }
    return NO_ERROR;
}

status_t ExynosCameraParameters::setZoomLevel(int zoom)
{
    int srcW = 0;
    int srcH = 0;
    int dstW = 0;
    int dstH = 0;
#ifdef USE_FW_ZOOMRATIO
    float zoomRatio = 1.00f;
#endif

    m_cameraInfo.zoom = zoom;

    getHwSensorSize(&srcW, &srcH);
    getHwPreviewSize(&dstW, &dstH);

#if 0
    if (m_setParamCropRegion(zoom, srcW, srcH, dstW, dstH) != NO_ERROR) {
        return BAD_VALUE;
    }
#else
    ExynosRect srcRect, dstRect;
    getPreviewBayerCropSize(&srcRect, &dstRect);
#endif
#ifdef USE_FW_ZOOMRATIO
    zoomRatio = getZoomRatio(zoom) / 1000;
    setMetaCtlZoom(&m_metadata, zoomRatio);
#endif

    return NO_ERROR;
}

status_t ExynosCameraParameters::setCropRegion(int x, int y, int w, int h)
{
    status_t ret = NO_ERROR;

    ret = setMetaCtlCropRegion(&m_metadata, x, y, w, h);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to setMetaCtlCropRegion(%d, %d, %d, %d)",
                __FUNCTION__, __LINE__, x, y, w, h);
    }

    return ret;
}

void ExynosCameraParameters::m_getCropRegion(int *x, int *y, int *w, int *h)
{
    getMetaCtlCropRegion(&m_metadata, x, y, w, h);
}

status_t ExynosCameraParameters::m_setParamCropRegion(
        int zoom,
        int srcW, int srcH,
        int dstW, int dstH)
{
    int newX = 0, newY = 0, newW = 0, newH = 0;
    float zoomRatio = getZoomRatio(zoom) / 1000;

    if (getCropRectAlign(srcW,  srcH,
                         dstW,  dstH,
                         &newX, &newY,
                         &newW, &newH,
                         CAMERA_MAGIC_ALIGN, 2,
                         zoom, zoomRatio) != NO_ERROR) {
        CLOGE("ERR(%s):getCropRectAlign(%d, %d, %d, %d) fail",
            __func__, srcW,  srcH, dstW,  dstH);
        return BAD_VALUE;
    }

    newX = ALIGN_UP(newX, 2);
    newY = ALIGN_UP(newY, 2);
    newW = srcW - (newX * 2);
    newH = srcH - (newY * 2);

    CLOGI("DEBUG(%s):size0(%d, %d, %d, %d)",
        __FUNCTION__, srcW, srcH, dstW, dstH);
    CLOGI("DEBUG(%s):size(%d, %d, %d, %d), level(%d)",
        __FUNCTION__, newX, newY, newW, newH, zoom);

    m_setHwBayerCropRegion(newW, newH, newX, newY);

    return NO_ERROR;
}

int ExynosCameraParameters::getZoomLevel(void)
{
    return m_cameraInfo.zoom;
}

status_t ExynosCameraParameters::checkRotation(const CameraParameters& params)
{
    int newRotation = params.getInt(CameraParameters::KEY_ROTATION);
    int curRotation = 0;

    if (newRotation < 0) {
        CLOGE("ERR(%s): Invalide Rotation value(%d)", __FUNCTION__, newRotation);
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):set orientation:%d", "setParameters", newRotation);

    curRotation = getRotation();

    if (curRotation != newRotation) {
        m_setRotation(newRotation);
        m_params.set(CameraParameters::KEY_ROTATION, newRotation);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setRotation(int rotation)
{
    m_cameraInfo.rotation = rotation;
}

int ExynosCameraParameters::getRotation(void)
{
    return m_cameraInfo.rotation;
}

status_t ExynosCameraParameters::checkAutoExposureLock(const CameraParameters& params)
{
    bool newAutoExposureLock = false;
    bool curAutoExposureLock = false;
    const char *strAutoExposureLock = params.get(CameraParameters::KEY_AUTO_EXPOSURE_LOCK);
    if (strAutoExposureLock == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):newAutoExposureLock %s", "setParameters", strAutoExposureLock);

    if (!strcmp(strAutoExposureLock, "true"))
        newAutoExposureLock = true;

    curAutoExposureLock = getAutoExposureLock();

    if (curAutoExposureLock != newAutoExposureLock) {
        ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
        m_flashMgr->setAeLock(newAutoExposureLock);
        m_setAutoExposureLock(newAutoExposureLock);
        m_params.set(CameraParameters::KEY_AUTO_EXPOSURE_LOCK, strAutoExposureLock);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setAutoExposureLock(bool lock)
{
    if (getHalVersion() != IS_HAL_VER_3_2) {
        m_cameraInfo.autoExposureLock = lock;
        setMetaCtlAeLock(&m_metadata, lock);
    }
}

bool ExynosCameraParameters::getAutoExposureLock(void)
{
    return m_cameraInfo.autoExposureLock;
}

void ExynosCameraParameters::m_adjustAeMode(enum aa_aemode curAeMode, enum aa_aemode *newAeMode)
{
    if (getHalVersion() != IS_HAL_VER_3_2) {
        int curMeteringMode = getMeteringMode();
        if (curAeMode == AA_AEMODE_OFF) {
            switch(curMeteringMode) {
            case METERING_MODE_AVERAGE:
                *newAeMode = AA_AEMODE_AVERAGE;
                break;
            case METERING_MODE_CENTER:
                *newAeMode = AA_AEMODE_CENTER;
                break;
            case METERING_MODE_MATRIX:
                *newAeMode = AA_AEMODE_MATRIX;
                break;
            case METERING_MODE_SPOT:
                *newAeMode = AA_AEMODE_SPOT;
                break;
            default:
                *newAeMode = curAeMode;
                break;
            }
        }
    }
}

status_t ExynosCameraParameters::checkExposureCompensation(const CameraParameters& params)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return NO_ERROR;
    }

    int minExposureCompensation = params.getInt(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION);
    int maxExposureCompensation = params.getInt(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION);
    int newExposureCompensation = params.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
    int curExposureCompensation = getExposureCompensation();

    CLOGD("DEBUG(%s):newExposureCompensation %d", "setParameters", newExposureCompensation);

    if ((newExposureCompensation < minExposureCompensation) ||
        (newExposureCompensation > maxExposureCompensation)) {
        CLOGE("ERR(%s): Invalide Exposurecompensation (Min: %d, Max: %d, Value: %d)", __FUNCTION__,
            minExposureCompensation, maxExposureCompensation, newExposureCompensation);
        return BAD_VALUE;
    }

    if (curExposureCompensation != newExposureCompensation) {
        m_setExposureCompensation(newExposureCompensation);
        m_params.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, newExposureCompensation);
    }

    return NO_ERROR;
}

/* TODO: Who explane this offset value? */
#define FW_CUSTOM_OFFSET (1)
/* F/W's middle value is 5, and step is -4, -3, -2, -1, 0, 1, 2, 3, 4 */
void ExynosCameraParameters::m_setExposureCompensation(int32_t value)
{
#if defined(USE_SUBDIVIDED_EV)
    setMetaCtlExposureCompensation(&m_metadata, value);
    setMetaCtlExposureCompensationStep(&m_metadata, m_staticInfo->exposureCompensationStep);
#else
    setMetaCtlExposureCompensation(&m_metadata, value + IS_EXPOSURE_DEFAULT + FW_CUSTOM_OFFSET);
#endif
}

int32_t ExynosCameraParameters::getExposureCompensation(void)
{
    int32_t expCompensation;
    getMetaCtlExposureCompensation(&m_metadata, &expCompensation);
#if defined(USE_SUBDIVIDED_EV)
    return expCompensation;
#else
    return expCompensation - IS_EXPOSURE_DEFAULT - FW_CUSTOM_OFFSET;
#endif
}

status_t ExynosCameraParameters::checkExposureTime(const CameraParameters& params)
{
    int ret = NO_ERROR;
    int newExposureTime = params.getInt("exposure-time");
    int curExposureTime = m_params.getInt("exposure-time");
    const char *strNewSceneMode = params.get(CameraParameters::KEY_SCENE_MODE);

    if (strNewSceneMode == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):newExposureTime : %d", "setParameters", newExposureTime);

    if (newExposureTime == curExposureTime)
        return NO_ERROR;

    if (newExposureTime < 0)
        return NO_ERROR;

    if (newExposureTime != 0 &&
        (newExposureTime > getMaxExposureTime() || newExposureTime < getMinExposureTime())) {
        CLOGE("ERR(%s): Invalid Exposure Time(%d). minExposureTime(%d), maxExposureTime(%d)",
                "setParameters", newExposureTime, getMaxExposureTime(), getMinExposureTime());
        return BAD_VALUE;
    }

    if (strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_AUTO) && newExposureTime > 0) {
        CLOGD("DEBUG(%s):ExposureTime can set only auto scene mode", "setParameters");
        return NO_ERROR;
    }

    m_setExposureTime((uint64_t) newExposureTime);
    m_params.set("exposure-time", newExposureTime);

    return NO_ERROR;
}

void ExynosCameraParameters::m_setExposureTime(uint64_t exposureTime)
{
    m_exposureTimeCapture = exposureTime;

    /* make capture exposuretime micro second */
    setMetaCtlCaptureExposureTime(&m_metadata, (uint32_t)exposureTime);

    /* For showing faster Preview */
    if (exposureTime > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT)
        exposureTime = CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT;

    /* make exposuretime nano second */
    setMetaCtlExposureTime(&m_metadata, exposureTime * 1000);
}

void ExynosCameraParameters::setExposureTime(void)
{
    /* make exposuretime nano second */
    setMetaCtlExposureTime(&m_metadata, getLongExposureTime() * 1000);
}

void ExynosCameraParameters::setPreviewExposureTime(void)
{
    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();

    m_setExposureTime(m_exposureTimeCapture);
    m_flashMgr->setManualExposureTime(0);
}

uint64_t ExynosCameraParameters::getPreviewExposureTime(void)
{
    uint64_t exposureTime = 0;

    getMetaCtlExposureTime(&m_metadata, &exposureTime);
    /* make exposuretime milli second */
    exposureTime = exposureTime / 1000;

    return exposureTime;
}

uint64_t ExynosCameraParameters::getCaptureExposureTime(void)
{
    return m_exposureTimeCapture;
}

uint64_t ExynosCameraParameters::getLongExposureTime(void)
{
#ifndef CAMERA_ADD_BAYER_ENABLE
    if (m_exposureTimeCapture >= CAMERA_EXPOSURE_TIME_MAX) {
        return CAMERA_EXPOSURE_TIME_MAX;
    } else
#endif
    {
        return m_exposureTimeCapture / (getLongExposureShotCount() + 1);
    }
}

int32_t ExynosCameraParameters::getLongExposureShotCount(void)
{
    bool getResult;
    int32_t count = 0;
#ifdef CAMERA_ADD_BAYER_ENABLE
    if (m_exposureTimeCapture <= CAMERA_EXPOSURE_TIME_MAX)
#endif
        return 0;

    if (m_exposureTimeCapture % CAMERA_EXPOSURE_TIME_MAX) {
        count = 2;
        getResult = false;
        while (!getResult) {
            if (m_exposureTimeCapture % count) {
                count++;
                continue;
            }
            if (CAMERA_EXPOSURE_TIME_MAX < (m_exposureTimeCapture / count)) {
                count++;
                continue;
            }
            getResult = true;
        }
        return count - 1;
    } else {
        return m_exposureTimeCapture / CAMERA_EXPOSURE_TIME_MAX - 1;
    }
}

status_t ExynosCameraParameters::checkMeteringAreas(const CameraParameters& params)
{
    int ret = NO_ERROR;
    const char *newMeteringAreas = params.get(CameraParameters::KEY_METERING_AREAS);
    const char *curMeteringAreas = m_params.get(CameraParameters::KEY_METERING_AREAS);
    const char meteringAreas[20] = "(0,0,0,0,0)";
    bool nullCheckflag = false;

    int newMeteringAreasSize = 0;
    bool isMeteringAreasSame = false;
    uint32_t maxNumMeteringAreas = getMaxNumMeteringAreas();

    if (m_exposureTimeCapture > 0) {
        ALOGD("DEBUG(%s): manual exposure mode. MeteringAreas will not set.", "setParameters");
        return NO_ERROR;
    }

    if (newMeteringAreas == NULL || (newMeteringAreas != NULL && !strcmp(newMeteringAreas, "(0,0,0,0,0)"))) {
        if(getMeteringMode() == METERING_MODE_SPOT) {
            newMeteringAreas = meteringAreas;
            nullCheckflag = true;
        } else {
            setMetaCtlAeRegion(&m_metadata, 0, 0, 0, 0, 0);
            return NO_ERROR;
        }
    }

    if(getSamsungCamera()) {
        maxNumMeteringAreas = 1;
    }

    if (maxNumMeteringAreas <= 0) {
        CLOGD("DEBUG(%s): meterin area is not supported", "Parameters");
        return NO_ERROR;
    }

    ALOGD("DEBUG(%s):newMeteringAreas: %s ,maxNumMeteringAreas(%d)", "setParameters", newMeteringAreas, maxNumMeteringAreas);

    newMeteringAreasSize = strlen(newMeteringAreas);
    if (curMeteringAreas != NULL) {
        isMeteringAreasSame = !strncmp(newMeteringAreas, curMeteringAreas, newMeteringAreasSize);
    }

    if (curMeteringAreas == NULL || isMeteringAreasSame == false || m_flagMeteringRegionChanged == true) {
        /* ex : (-10,-10,0,0,300),(0,0,10,10,700) */
        ExynosRect2 *rect2s  = new ExynosRect2[maxNumMeteringAreas];
        int         *weights = new int[maxNumMeteringAreas];
        uint32_t validMeteringAreas = bracketsStr2Ints((char *)newMeteringAreas, maxNumMeteringAreas, rect2s, weights, 1);

        if (0 < validMeteringAreas && validMeteringAreas <= maxNumMeteringAreas) {
            m_setMeteringAreas((uint32_t)validMeteringAreas, rect2s, weights);
            if(!nullCheckflag) {
                m_params.set(CameraParameters::KEY_METERING_AREAS, newMeteringAreas);
            }
        } else {
            CLOGE("ERR(%s):MeteringAreas value is invalid", __FUNCTION__);
            ret = UNKNOWN_ERROR;
        }

        m_flagMeteringRegionChanged = false;
        delete [] rect2s;
        delete [] weights;
    }

    return ret;
}

void ExynosCameraParameters::m_setMeteringAreas(uint32_t num, ExynosRect  *rects, int *weights)
{
    ExynosRect2 *rect2s = new ExynosRect2[num];

    for (uint32_t i = 0; i < num; i++)
        convertingRectToRect2(&rects[i], &rect2s[i]);

    m_setMeteringAreas(num, rect2s, weights);

    delete [] rect2s;
}

void ExynosCameraParameters::m_setMeteringAreas(uint32_t num, ExynosRect2 *rect2s, int *weights)
{
    uint32_t maxNumMeteringAreas = getMaxNumMeteringAreas();

    if(getSamsungCamera()) {
        maxNumMeteringAreas = 1;
    }

    if (maxNumMeteringAreas == 0) {
        CLOGV("DEBUG(%s):maxNumMeteringAreas is 0. so, ignored", __FUNCTION__);
        return;
    }

    if (maxNumMeteringAreas < num)
        num = maxNumMeteringAreas;

    if (getAutoExposureLock() == true) {
        CLOGD("DEBUG(%s):autoExposure is Locked", __FUNCTION__);
        return;
    }

    if (num == 1) {
#ifdef CAMERA_GED_FEATURE
        int meteringMode = getMeteringMode();

        if (isRectNull(&rect2s[0]) == true) {
            switch (meteringMode) {
                case METERING_MODE_SPOT:
                    /*
                     * Even if SPOT metering mode, area must set valid values,
                     * but areas was invalid values, we change mode to CENTER.
                     */
                    m_setMeteringMode(METERING_MODE_CENTER);
                    m_cameraInfo.isTouchMetering = false;
                    break;
                case METERING_MODE_AVERAGE:
                case METERING_MODE_CENTER:
                case METERING_MODE_MATRIX:
                default:
                    /* adjust metering setting */
                    break;
            }
        } else {
            switch (meteringMode) {
                case METERING_MODE_CENTER:
                    /*
                     * SPOT metering mode in GED camera App was not set METERING_MODE_SPOT,
                     * but set metering areas only.
                     */
                    m_setMeteringMode(METERING_MODE_SPOT);
                    m_cameraInfo.isTouchMetering = true;
                    break;
                case METERING_MODE_AVERAGE:
                case METERING_MODE_MATRIX:
                case METERING_MODE_SPOT:
                default:
                    /* adjust metering setting */
                    break;
            }
        }
#endif
    } else {
        if (num > 1 && isRectEqual(&rect2s[0], &rect2s[1]) == false) {
            /* if MATRIX mode support, mode set METERING_MODE_MATRIX */
            m_setMeteringMode(METERING_MODE_AVERAGE);
            m_cameraInfo.isTouchMetering = false;
        } else {
            m_setMeteringMode(METERING_MODE_AVERAGE);
            m_cameraInfo.isTouchMetering = false;
        }
    }

    ExynosRect cropRegionRect;
    ExynosRect2 newRect2;

    getHwBayerCropRegion(&cropRegionRect.w, &cropRegionRect.h, &cropRegionRect.x, &cropRegionRect.y);

    for (uint32_t i = 0; i < num; i++) {
        bool isChangeMeteringArea = false;
#ifdef CAMERA_GED_FEATURE
        if (isRectNull(&rect2s[i]) == false)
            isChangeMeteringArea = true;
        else
            isChangeMeteringArea = false;
#else
        if ((isRectNull(&rect2s[i]) == false) ||((isRectNull(&rect2s[i]) == true) && (getMeteringMode() == METERING_MODE_SPOT)))
            isChangeMeteringArea = true;
#ifdef TOUCH_AE
        else if((getMeteringMode() == METERING_MODE_SPOT_TOUCH) || (getMeteringMode() == METERING_MODE_MATRIX_TOUCH)
            || (getMeteringMode() == METERING_MODE_CENTER_TOUCH) || (getMeteringMode() == METERING_MODE_AVERAGE_TOUCH))
            isChangeMeteringArea = true;
#endif
        else
            isChangeMeteringArea = false;
#endif
        if (isChangeMeteringArea == true) {
            CLOGD("DEBUG(%s) (%d %d %d %d) %d", __FUNCTION__, rect2s->x1, rect2s->y1, rect2s->x2, rect2s->y2, getMeteringMode());
            newRect2 = convertingAndroidArea2HWAreaBcropOut(&rect2s[i], &cropRegionRect);
            setMetaCtlAeRegion(&m_metadata, newRect2.x1, newRect2.y1,
                                newRect2.x2, newRect2.y2, weights[i]);
        }
    }
}

void ExynosCameraParameters::getMeteringAreas(__unused ExynosRect *rects)
{
    /* TODO */
}

void ExynosCameraParameters::getMeteringAreas(__unused ExynosRect2 *rect2s)
{
    /* TODO */
}

status_t ExynosCameraParameters::checkMeteringMode(const CameraParameters& params)
{
    const char *strNewMeteringMode = params.get("metering");
    int newMeteringMode = -1;
    int curMeteringMode = -1;
#ifdef SAMSUNG_COMPANION
    const char *strNewRTHdr = params.get("rt-hdr");
#endif
    if (strNewMeteringMode == NULL) {
        return NO_ERROR;
    }

    ALOGD("DEBUG(%s):strNewMeteringMode %s", "setParameters", strNewMeteringMode);

#ifdef SAMSUNG_COMPANION
    if ((strNewRTHdr != NULL && !strcmp(strNewRTHdr, "on"))
#ifdef TOUCH_AE
        && strcmp(strNewMeteringMode, "weighted-matrix")
#endif
        ) {
        newMeteringMode = METERING_MODE_MATRIX;
        ALOGD("DEBUG(%s):newMeteringMode %d in RT-HDR", "setParameters", newMeteringMode);
    } else
#endif
    {
        if (!strcmp(strNewMeteringMode, "average"))
            newMeteringMode = METERING_MODE_AVERAGE;
        else if (!strcmp(strNewMeteringMode, "center"))
            newMeteringMode = METERING_MODE_CENTER;
        else if (!strcmp(strNewMeteringMode, "matrix"))
            newMeteringMode = METERING_MODE_MATRIX;
        else if (!strcmp(strNewMeteringMode, "spot"))
            newMeteringMode = METERING_MODE_SPOT;
#ifdef TOUCH_AE
        else if (!strcmp(strNewMeteringMode, "weighted-center"))
            newMeteringMode = METERING_MODE_CENTER_TOUCH;
        else if (!strcmp(strNewMeteringMode, "weighted-matrix"))
            newMeteringMode = METERING_MODE_MATRIX_TOUCH;
        else if (!strcmp(strNewMeteringMode, "weighted-spot"))
            newMeteringMode = METERING_MODE_SPOT_TOUCH;
        else if (!strcmp(strNewMeteringMode, "weighted-average"))
            newMeteringMode = METERING_MODE_AVERAGE_TOUCH;
#endif
        else {
            ALOGE("ERR(%s):Invalid metering newMetering(%s)", __FUNCTION__, strNewMeteringMode);
            return UNKNOWN_ERROR;
        }
    }

    curMeteringMode = getMeteringMode();

    m_setMeteringMode(newMeteringMode);
#ifdef SAMSUNG_COMPANION
    if ((strNewRTHdr != NULL && strcmp(strNewRTHdr, "on"))
#ifdef TOUCH_AE
        || !strcmp(strNewMeteringMode, "weighted-matrix")
#endif
        ) {
        m_params.set("metering", strNewMeteringMode);
    }
#else
    m_params.set("metering", strNewMeteringMode);
#endif

    if (curMeteringMode != newMeteringMode) {
        ALOGI("INFO(%s): Metering Area is changed (%d -> %d)", __FUNCTION__, curMeteringMode, newMeteringMode);
        m_flagMeteringRegionChanged = true;
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setMeteringMode(int meteringMode)
{
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t w = 0;
    uint32_t h = 0;
    uint32_t weight = 0;
    int hwSensorW = 0;
    int hwSensorH = 0;
    enum aa_aemode aeMode;

    if (getAutoExposureLock() == true) {
        CLOGD("DEBUG(%s):autoExposure is Locked", __FUNCTION__);
        return;
    }

    m_cameraInfo.meteringMode = meteringMode;

    getHwSensorSize(&hwSensorW, &hwSensorH);

    switch (meteringMode) {
    case METERING_MODE_AVERAGE:
        aeMode = AA_AEMODE_AVERAGE;
        x = 0;
        y = 0;
        w = hwSensorW;
        h = hwSensorH;
        weight = 1000;
        break;
    case METERING_MODE_MATRIX:
        aeMode = AA_AEMODE_MATRIX;
        x = 0;
        y = 0;
        w = hwSensorW;
        h = hwSensorH;
        weight = 1000;
        break;
    case METERING_MODE_SPOT:
        /* In spot mode, default region setting is 100x100 rectangle on center */
        aeMode = AA_AEMODE_SPOT;
        x = hwSensorW / 2 - 50;
        y = hwSensorH / 2 - 50;
        w = hwSensorW / 2 + 50;
        h = hwSensorH / 2 + 50;
        weight = 50;
        break;
#ifdef TOUCH_AE
    case METERING_MODE_MATRIX_TOUCH:
        aeMode = AA_AEMODE_MATRIX_TOUCH;
        break;
    case METERING_MODE_SPOT_TOUCH:
        aeMode = AA_AEMODE_SPOT_TOUCH;
        break;
    case METERING_MODE_CENTER_TOUCH:
        aeMode = AA_AEMODE_CENTER_TOUCH;
        break;
    case METERING_MODE_AVERAGE_TOUCH:
        aeMode = AA_AEMODE_AVERAGE_TOUCH;
        break;
#endif
    case METERING_MODE_CENTER:
    default:
        aeMode = AA_AEMODE_CENTER;
        x = 0;
        y = 0;
        w = 0;
        h = 0;
        weight = 1000;
        break;
    }

    setMetaCtlAeMode(&m_metadata, aeMode);

    if (getCaptureExposureTime() > 0) {
        aeMode = AA_AEMODE_OFF;
    }

    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
    m_flashMgr->setFlashExposure(aeMode);
}

int ExynosCameraParameters::getMeteringMode(void)
{
    return m_cameraInfo.meteringMode;
}

int ExynosCameraParameters::getSupportedMeteringMode(void)
{
    return m_staticInfo->meteringList;
}

status_t ExynosCameraParameters::checkAntibanding(const CameraParameters& params)
{
    int newAntibanding = -1;
    int curAntibanding = -1;

    const char *strKeyAntibanding = params.get(CameraParameters::KEY_ANTIBANDING);
#ifdef USE_CSC_FEATURE
    CLOGD("DEBUG(%s):m_antiBanding %s", "setParameters", m_antiBanding);
    const char *strNewAntibanding = m_adjustAntibanding(m_antiBanding);
#else
    const char *strNewAntibanding = m_adjustAntibanding(strKeyAntibanding);
#endif

    if (strNewAntibanding == NULL) {
        return NO_ERROR;
    }
    CLOGD("DEBUG(%s):strNewAntibanding %s", "setParameters", strNewAntibanding);

    if (!strcmp(strNewAntibanding, CameraParameters::ANTIBANDING_AUTO))
        newAntibanding = AA_AE_ANTIBANDING_AUTO;
    else if (!strcmp(strNewAntibanding, CameraParameters::ANTIBANDING_50HZ))
        newAntibanding = AA_AE_ANTIBANDING_AUTO_50HZ;
    else if (!strcmp(strNewAntibanding, CameraParameters::ANTIBANDING_60HZ))
        newAntibanding = AA_AE_ANTIBANDING_AUTO_60HZ;
    else if (!strcmp(strNewAntibanding, CameraParameters::ANTIBANDING_OFF))
        newAntibanding = AA_AE_ANTIBANDING_OFF;
    else {
        CLOGE("ERR(%s):Invalid antibanding value(%s)", __FUNCTION__, strNewAntibanding);
        return BAD_VALUE;
    }

    curAntibanding = getAntibanding();

    if (curAntibanding != newAntibanding) {
        m_setAntibanding(newAntibanding);
    }

    if (strKeyAntibanding != NULL) {
        m_params.set(CameraParameters::KEY_ANTIBANDING, strKeyAntibanding); // HAL test (Ext_SecCameraParametersTest_testSetAntibanding_P01)
    }

    return NO_ERROR;
}

const char *ExynosCameraParameters::m_adjustAntibanding(const char *strAntibanding)
{
    const char *strAdjustedAntibanding = NULL;

    strAdjustedAntibanding = strAntibanding;

#if 0 /* fixed the flicker issue when highspeed recording(60fps or 120fps) */
    /* when high speed recording mode, off thre antibanding */
    if (getHighSpeedRecording())
        strAdjustedAntibanding = CameraParameters::ANTIBANDING_OFF;
#endif
    return strAdjustedAntibanding;
}


void ExynosCameraParameters::m_setAntibanding(int value)
{
    setMetaCtlAntibandingMode(&m_metadata, (enum aa_ae_antibanding_mode)value);
}

int ExynosCameraParameters::getAntibanding(void)
{
    enum aa_ae_antibanding_mode antibanding;
    getMetaCtlAntibandingMode(&m_metadata, &antibanding);
    return (int)antibanding;
}

int ExynosCameraParameters::getSupportedAntibanding(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return 0;
    } else {
        return m_staticInfo->antiBandingList;
    }
}

#ifdef USE_CSC_FEATURE
void ExynosCameraParameters::m_getAntiBandingFromLatinMCC(char *temp_str)
{
    char value[PROPERTY_VALUE_MAX];
    char country_value[10];

    memset(value, 0x00, sizeof(value));
    memset(country_value, 0x00, sizeof(country_value));
    if (!property_get("gsm.operator.numeric", value,"")) {
        strcpy(temp_str, CameraParameters::ANTIBANDING_60HZ);
        return ;
    }
    memcpy(country_value, value, 3);

    /** MCC Info. Jamaica : 338 / Argentina : 722 / Chile : 730 / Paraguay : 744 / Uruguay : 748  **/
    if (strstr(country_value,"338") || strstr(country_value,"722") || strstr(country_value,"730") || strstr(country_value,"744") || strstr(country_value,"748"))
        strcpy(temp_str, CameraParameters::ANTIBANDING_50HZ);
    else
        strcpy(temp_str, CameraParameters::ANTIBANDING_60HZ);
}

int ExynosCameraParameters::m_IsLatinOpenCSC()
{
    char sales_code[PROPERTY_VALUE_MAX] = {0};
    property_get("ro.csc.sales_code", sales_code, "");
    if (strstr(sales_code,"TFG") || strstr(sales_code,"TPA") || strstr(sales_code,"TTT") || strstr(sales_code,"JDI") || strstr(sales_code,"PCI") )
        return 1;
    else
        return 0;
}

void ExynosCameraParameters::m_chooseAntiBandingFrequency()
{
    status_t ret = NO_ERROR;
    int LatinOpenCSClength = 5;
    char *LatinOpenCSCstr = NULL;
    char *CSCstr = NULL;
    const char *defaultStr = "50hz";

    if (m_IsLatinOpenCSC()) {
        LatinOpenCSCstr = (char *)malloc(LatinOpenCSClength);
        if (LatinOpenCSCstr == NULL) {
            CLOGE("LatinOpenCSCstr is NULL");
            CSCstr = (char *)defaultStr;
            memset(m_antiBanding, 0, sizeof(m_antiBanding));
            strcpy(m_antiBanding, CSCstr);
            return;
        }
        memset(LatinOpenCSCstr, 0, LatinOpenCSClength);

        m_getAntiBandingFromLatinMCC(LatinOpenCSCstr);
        CSCstr = LatinOpenCSCstr;
    } else {
        CSCstr = (char *)SecNativeFeature::getInstance()->getString("CscFeature_Camera_CameraFlicker");
    }

    if (CSCstr == NULL || strlen(CSCstr) == 0) {
        CSCstr = (char *)defaultStr;
    }

    memset(m_antiBanding, 0, sizeof(m_antiBanding));
    strcpy(m_antiBanding, CSCstr);
    CLOGD("m_antiBanding = %s",m_antiBanding);

    if (LatinOpenCSCstr != NULL) {
        free(LatinOpenCSCstr);
        LatinOpenCSCstr = NULL;
    }
}
#endif

status_t ExynosCameraParameters::checkSceneMode(const CameraParameters& params)
{
    int  newSceneMode = -1;
    int  curSceneMode = -1;
    const char *strNewSceneMode = params.get(CameraParameters::KEY_SCENE_MODE);

    if (strNewSceneMode == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):strNewSceneMode %s", "setParameters", strNewSceneMode);

    if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_AUTO)) {
        newSceneMode = SCENE_MODE_AUTO;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_ACTION)) {
        newSceneMode = SCENE_MODE_ACTION;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_PORTRAIT)) {
        newSceneMode = SCENE_MODE_PORTRAIT;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_LANDSCAPE)) {
        newSceneMode = SCENE_MODE_LANDSCAPE;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_NIGHT)) {
        newSceneMode = SCENE_MODE_NIGHT;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_NIGHT_PORTRAIT)) {
        newSceneMode = SCENE_MODE_NIGHT_PORTRAIT;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_THEATRE)) {
        newSceneMode = SCENE_MODE_THEATRE;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_BEACH)) {
        newSceneMode = SCENE_MODE_BEACH;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_SNOW)) {
        newSceneMode = SCENE_MODE_SNOW;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_SUNSET)) {
        newSceneMode = SCENE_MODE_SUNSET;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_STEADYPHOTO)) {
        newSceneMode = SCENE_MODE_STEADYPHOTO;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_FIREWORKS)) {
        newSceneMode = SCENE_MODE_FIREWORKS;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_SPORTS)) {
        newSceneMode = SCENE_MODE_SPORTS;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_PARTY)) {
        newSceneMode = SCENE_MODE_PARTY;
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_CANDLELIGHT)) {
        newSceneMode = SCENE_MODE_CANDLELIGHT;
#ifdef SAMSUNG_COMPANION
    } else if (!strcmp(strNewSceneMode, CameraParameters::SCENE_MODE_HDR)) {
        newSceneMode = SCENE_MODE_HDR;
#endif
#ifdef SAMSUNG_FOOD_MODE
    } else if (!strcmp(strNewSceneMode, "food")) {
        enum aa_aemode aeMode = AA_AEMODE_MATRIX;
        newSceneMode = SCENE_MODE_FOOD;
#if defined(USE_SUBDIVIDED_EV)
        m_setExposureCompensation(5);
#else
        m_setExposureCompensation(2);
#endif
        setMetaCtlAeMode(&m_metadata, aeMode);
#endif
    } else if (!strcmp(strNewSceneMode, "aqua_scn")) {
        newSceneMode = SCENE_MODE_AQUA;
    } else {
        CLOGE("ERR(%s):unmatched scene_mode(%s)", "Parameters", strNewSceneMode);
        return BAD_VALUE;
    }

    curSceneMode = getSceneMode();

    if (curSceneMode != newSceneMode) {
        m_setSceneMode(newSceneMode);
        m_params.set(CameraParameters::KEY_SCENE_MODE, strNewSceneMode);
#ifdef SAMSUNG_COMPANION
        if(getUseCompanion() == true) {
            if (newSceneMode == SCENE_MODE_HDR)
                checkSceneRTHdr(true);
        }

        if ((newSceneMode == SCENE_MODE_HDR) || (curSceneMode == SCENE_MODE_HDR))
            m_setRestartPreview(true);
#endif
        updatePreviewFpsRange();
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setSceneMode(int value)
{
    enum aa_mode mode = AA_CONTROL_AUTO;
    enum aa_scene_mode sceneMode = AA_SCENE_MODE_FACE_PRIORITY;

    switch (value) {
    case SCENE_MODE_PORTRAIT:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_PORTRAIT;
        break;
    case SCENE_MODE_LANDSCAPE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_LANDSCAPE;
        break;
    case SCENE_MODE_NIGHT:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_NIGHT;
        break;
    case SCENE_MODE_BEACH:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_BEACH;
        break;
    case SCENE_MODE_SNOW:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_SNOW;
        break;
    case SCENE_MODE_SUNSET:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_SUNSET;
        break;
    case SCENE_MODE_FIREWORKS:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_FIREWORKS;
        break;
    case SCENE_MODE_SPORTS:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_SPORTS;
        break;
    case SCENE_MODE_PARTY:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_PARTY;
        break;
    case SCENE_MODE_CANDLELIGHT:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_CANDLELIGHT;
        break;
    case SCENE_MODE_STEADYPHOTO:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_STEADYPHOTO;
        break;
    case SCENE_MODE_ACTION:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_ACTION;
        break;
    case SCENE_MODE_NIGHT_PORTRAIT:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_NIGHT_PORTRAIT;
        break;
    case SCENE_MODE_THEATRE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_THEATRE;
        break;
#ifdef SAMSUNG_FOOD_MODE
    case SCENE_MODE_FOOD:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_FOOD;
        break;
#endif
    case SCENE_MODE_AQUA:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_AQUA;
        break;
    case SCENE_MODE_AUTO:
    default:
        mode = AA_CONTROL_AUTO;
        sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
        break;
    }

    m_cameraInfo.sceneMode = value;
    setMetaCtlSceneMode(&m_metadata, mode, sceneMode);
    m_cameraInfo.whiteBalanceMode = m_convertMetaCtlAwbMode(&m_metadata);
}

int ExynosCameraParameters::getSceneMode(void)
{
    return m_cameraInfo.sceneMode;
}

int ExynosCameraParameters::getSupportedSceneModes(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return 0;
    } else {
        return m_staticInfo->sceneModeList;
    }
}

status_t ExynosCameraParameters::checkFocusMode(const CameraParameters& params)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return NO_ERROR;
    } else {
        int  newFocusMode = -1;
        int  curFocusMode = -1;
        const char *strFocusMode = params.get(CameraParameters::KEY_FOCUS_MODE);
        const char *strNewFocusMode = m_adjustFocusMode(strFocusMode);

        if (strNewFocusMode == NULL) {
            return NO_ERROR;
        }

        CLOGD("DEBUG(%s):strNewFocusMode %s", "setParameters", strNewFocusMode);

        if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_AUTO)) {
            newFocusMode = FOCUS_MODE_AUTO;
            m_params.set(CameraParameters::KEY_FOCUS_DISTANCES,
                    BACK_CAMERA_AUTO_FOCUS_DISTANCES_STR);
        } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_INFINITY)) {
            newFocusMode = FOCUS_MODE_INFINITY;
            m_params.set(CameraParameters::KEY_FOCUS_DISTANCES,
                    BACK_CAMERA_INFINITY_FOCUS_DISTANCES_STR);
        } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_MACRO)) {
            newFocusMode = FOCUS_MODE_MACRO;
            m_params.set(CameraParameters::KEY_FOCUS_DISTANCES,
                    BACK_CAMERA_MACRO_FOCUS_DISTANCES_STR);
        } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_FIXED)) {
            newFocusMode = FOCUS_MODE_FIXED;
        } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_EDOF)) {
            newFocusMode = FOCUS_MODE_EDOF;
        } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO)) {
            newFocusMode = FOCUS_MODE_CONTINUOUS_VIDEO;
        } else if (!strcmp(strNewFocusMode, CameraParameters::FOCUS_MODE_CONTINUOUS_PICTURE)) {
            newFocusMode = FOCUS_MODE_CONTINUOUS_PICTURE;
        } else if (!strcmp(strNewFocusMode, "face-priority")) {
            newFocusMode = FOCUS_MODE_CONTINUOUS_PICTURE;
        } else if (!strcmp(strNewFocusMode, "continuous-picture-macro")) {
            newFocusMode = FOCUS_MODE_CONTINUOUS_PICTURE_MACRO;
#ifdef SAMSUNG_OT
        } else if (!strcmp(strNewFocusMode, "object-tracking-picture")) {
            newFocusMode = FOCUS_MODE_OBJECT_TRACKING_PICTURE;
        } else if (!strcmp(strNewFocusMode, "object-tracking-video")) {
            newFocusMode = FOCUS_MODE_OBJECT_TRACKING_VIDEO;
#endif
#ifdef SAMSUNG_MANUAL_FOCUS
        } else if (!strcmp(strNewFocusMode, "manual")) {
            newFocusMode = FOCUS_MODE_MANUAL;
#endif
        } else {
            CLOGE("ERR(%s):unmatched focus_mode(%s)", __FUNCTION__, strNewFocusMode);
            return BAD_VALUE;
        }

        if (!(newFocusMode & getSupportedFocusModes())){
            CLOGE("ERR(%s[%d]): Focus mode(%s) is not supported!", __FUNCTION__, __LINE__, strNewFocusMode);
            return BAD_VALUE;
        }

#ifdef SAMSUNG_MANUAL_FOCUS
        /* Set focus distance to -1 if focus mode is changed from MANUAL to others */
        if ((FOCUS_MODE_MANUAL == getFocusMode()) && (FOCUS_MODE_MANUAL != newFocusMode)) {
            m_setFocusDistance(-1);
        }
#endif

        m_setFocusMode(newFocusMode);
        m_params.set(CameraParameters::KEY_FOCUS_MODE, strNewFocusMode);

        return NO_ERROR;
    }
}

const char *ExynosCameraParameters::m_adjustFocusMode(const char *focusMode)
{
    int sceneMode = getSceneMode();
    const char *newFocusMode = NULL;

    /* TODO: vendor specific adjust */

    newFocusMode = focusMode;

    return newFocusMode;
}

void ExynosCameraParameters::m_setFocusMode(int focusMode)
{
    m_cameraInfo.focusMode = focusMode;

    if(getZoomActiveOn()) {
        ALOGD("DEBUG(%s):zoom moving..", "setParameters");
        return;
    }

    /* TODO: Notify auto focus activity */
    if(getPreviewRunning() == true) {
        ALOGD("set Focus Mode(%s[%d]) !!!!", __FUNCTION__, __LINE__);
        m_activityControl->setAutoFocusMode(focusMode);
    } else {
        m_setFocusmodeSetting = true;
    }

#ifdef SAMSUNG_OT
    if(m_cameraInfo.focusMode == FOCUS_MODE_OBJECT_TRACKING_VIDEO
       || m_cameraInfo.focusMode == FOCUS_MODE_OBJECT_TRACKING_PICTURE) {
        m_startObjectTracking = true;
    }
    else
        m_startObjectTracking = false;
#endif
}

void ExynosCameraParameters::setFocusModeLock(bool enable) {
    int curFocusMode = getFocusMode();

    ALOGD("DEBUG(%s):FocusModeLock (%s)", __FUNCTION__, enable? "true" : "false");

    if(enable) {
        m_activityControl->stopAutoFocus();
    } else {
        m_setFocusMode(curFocusMode);
    }
}

void ExynosCameraParameters::setFocusModeSetting(bool enable)
{
    m_setFocusmodeSetting = enable;
}

int ExynosCameraParameters::getFocusModeSetting(void)
{
    return m_setFocusmodeSetting;
}

int ExynosCameraParameters::getFocusMode(void)
{
    return m_cameraInfo.focusMode;
}

int ExynosCameraParameters::getSupportedFocusModes(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return 0;
    } else {
        return m_staticInfo->focusModeList;
    }
}

#ifdef SAMSUNG_MANUAL_FOCUS
status_t ExynosCameraParameters::checkFocusDistance(const CameraParameters& params)
{
    int newFocusDistance = params.getInt("focus-distance");
    int curFocusDistance = -1;

    if (newFocusDistance < 0) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):newFocusDistance %d", "setParameters", newFocusDistance);

    curFocusDistance = getFocusDistance();

    if (curFocusDistance != newFocusDistance) {
        if (FOCUS_MODE_MANUAL != getFocusMode()) {
            newFocusDistance = -1;
        }

        m_setFocusDistance(newFocusDistance);
        m_params.set("focus-distance", newFocusDistance);
    }

    return NO_ERROR;
}

/**
 * m_setFocusDistance: set focus distance.
 *
 * distance should be -1, 0 or value greater than 0.
 */
void ExynosCameraParameters::m_setFocusDistance(int32_t distance)
{
    float metaDistance;

    if (distance < -1) {
        CLOGE("ERR(%s):Invalid new focus distance (%d)", __FUNCTION__, distance);
        return;
    }

    if (!distance) {
        metaDistance = 0;
    } else if (-1 == distance) {
        metaDistance = -1;
    } else {
        /* distance variable is in milimeters.
         * And if distance variable is converted into meter unit(called as A),
         * meta distance value is as followings:
         *     metaDistance = 1 / A     */
        metaDistance = 1000 / (float)distance;
    }

    setMetaCtlFocusDistance(&m_metadata, metaDistance);
}

int32_t ExynosCameraParameters::getFocusDistance(void)
{
    float metaDistance = 0.0f;

    getMetaCtlFocusDistance(&m_metadata, &metaDistance);

    /* Focus distance 0 means infinite */
    if (0.0f == metaDistance) {
        return -2;
    } else if (-1.0f == metaDistance) {
        return -1;
    }

    return (int32_t)(1000 / metaDistance);
}
#endif /* SAMSUNG_MANUAL_FOCUS */

status_t ExynosCameraParameters::checkFlashMode(const CameraParameters& params)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return NO_ERROR;
    } else {
        int  newFlashMode = -1;
        int  curFlashMode = -1;
        const char *strFlashMode = params.get(CameraParameters::KEY_FLASH_MODE);
        const char *strNewFlashMode = m_adjustFlashMode(strFlashMode);

        if (strNewFlashMode == NULL) {
            return NO_ERROR;
        }

        CLOGD("DEBUG(%s):strNewFlashMode %s", "setParameters", strNewFlashMode);

        if (!strcmp(strNewFlashMode, CameraParameters::FLASH_MODE_OFF))
            newFlashMode = FLASH_MODE_OFF;
        else if (!strcmp(strNewFlashMode, CameraParameters::FLASH_MODE_AUTO))
            newFlashMode = FLASH_MODE_AUTO;
        else if (!strcmp(strNewFlashMode, CameraParameters::FLASH_MODE_ON))
            newFlashMode = FLASH_MODE_ON;
        else if (!strcmp(strNewFlashMode, CameraParameters::FLASH_MODE_RED_EYE))
            newFlashMode = FLASH_MODE_RED_EYE;
        else if (!strcmp(strNewFlashMode, CameraParameters::FLASH_MODE_TORCH))
            newFlashMode = FLASH_MODE_TORCH;
        else {
            CLOGE("ERR(%s):unmatched flash_mode(%s), turn off flash", __FUNCTION__, strNewFlashMode);
            newFlashMode = FLASH_MODE_OFF;
            strNewFlashMode = CameraParameters::FLASH_MODE_OFF;
            return BAD_VALUE;
        }

#ifndef UNSUPPORT_FLASH
        if (!(newFlashMode & getSupportedFlashModes())) {
            CLOGE("ERR(%s[%d]): Flash mode(%s) is not supported!", __FUNCTION__, __LINE__, strNewFlashMode);
            return BAD_VALUE;
        }
#endif

        curFlashMode = getFlashMode();

        if (curFlashMode != newFlashMode) {
            m_setFlashMode(newFlashMode);
            m_params.set(CameraParameters::KEY_FLASH_MODE, strNewFlashMode);
        }

        return NO_ERROR;
    }
}

const char *ExynosCameraParameters::m_adjustFlashMode(const char *flashMode)
{
    int sceneMode = getSceneMode();
    const char *newFlashMode = NULL;

    /* TODO: vendor specific adjust */

    newFlashMode = flashMode;

    return newFlashMode;
}

void ExynosCameraParameters::m_setFlashMode(int flashMode)
{
    m_cameraInfo.flashMode = flashMode;

    /* TODO: Notity flash activity */
    m_activityControl->setFlashMode(flashMode);
}

int ExynosCameraParameters::getFlashMode(void)
{
    return m_cameraInfo.flashMode;
}

int ExynosCameraParameters::getSupportedFlashModes(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return 0;
    } else {
        return m_staticInfo->flashModeList;
    }
}

status_t ExynosCameraParameters::checkWhiteBalanceMode(const CameraParameters& params)
{
    if (getHalVersion() != IS_HAL_VER_3_2) {
        int newWhiteBalance = -1;
        int curWhiteBalance = -1;
        const char *strWhiteBalance = params.get(CameraParameters::KEY_WHITE_BALANCE);
        const char *strNewWhiteBalance = m_adjustWhiteBalanceMode(strWhiteBalance);

        if (strNewWhiteBalance == NULL) {
            return NO_ERROR;
        }

        CLOGD("DEBUG(%s):newWhiteBalance %s", "setParameters", strNewWhiteBalance);

        if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_AUTO))
            newWhiteBalance = WHITE_BALANCE_AUTO;
        else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_INCANDESCENT))
            newWhiteBalance = WHITE_BALANCE_INCANDESCENT;
        else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_FLUORESCENT))
            newWhiteBalance = WHITE_BALANCE_FLUORESCENT;
        else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_WARM_FLUORESCENT))
            newWhiteBalance = WHITE_BALANCE_WARM_FLUORESCENT;
        else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_DAYLIGHT))
            newWhiteBalance = WHITE_BALANCE_DAYLIGHT;
        else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT))
            newWhiteBalance = WHITE_BALANCE_CLOUDY_DAYLIGHT;
        else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_TWILIGHT))
            newWhiteBalance = WHITE_BALANCE_TWILIGHT;
        else if (!strcmp(strNewWhiteBalance, CameraParameters::WHITE_BALANCE_SHADE))
            newWhiteBalance = WHITE_BALANCE_SHADE;
        else if (!strcmp(strNewWhiteBalance, "temperature"))
            newWhiteBalance = WHITE_BALANCE_CUSTOM_K;
        else {
            CLOGE("ERR(%s):Invalid white balance(%s)", __FUNCTION__, strNewWhiteBalance);
            return BAD_VALUE;
        }

        if (!(newWhiteBalance & getSupportedWhiteBalance())) {
            CLOGE("ERR(%s[%d]): white balance mode(%s) is not supported", __FUNCTION__, __LINE__, strNewWhiteBalance);
            return BAD_VALUE;
        }

        curWhiteBalance = getWhiteBalanceMode();

        if (getSceneMode() == SCENE_MODE_AUTO) {
            enum aa_awbmode cur_awbMode;
            getMetaCtlAwbMode(&m_metadata, &cur_awbMode);

            if (m_setWhiteBalanceMode(newWhiteBalance) != NO_ERROR)
                return BAD_VALUE;

            m_params.set(CameraParameters::KEY_WHITE_BALANCE, strNewWhiteBalance);
        }
    }
    return NO_ERROR;
}

const char *ExynosCameraParameters::m_adjustWhiteBalanceMode(const char *whiteBalance)
{
    int sceneMode = getSceneMode();
    const char *newWhiteBalance = NULL;

    /* TODO: vendor specific adjust */

    /* TN' feautre can change whiteBalance even if Non SCENE_MODE_AUTO */

    newWhiteBalance = whiteBalance;

    return newWhiteBalance;
}

status_t ExynosCameraParameters::m_setWhiteBalanceMode(int whiteBalance)
{
    enum aa_awbmode awbMode;

    switch (whiteBalance) {
    case WHITE_BALANCE_AUTO:
        awbMode = AA_AWBMODE_WB_AUTO;
        break;
    case WHITE_BALANCE_INCANDESCENT:
        awbMode = AA_AWBMODE_WB_INCANDESCENT;
        break;
    case WHITE_BALANCE_FLUORESCENT:
        awbMode = AA_AWBMODE_WB_FLUORESCENT;
        break;
    case WHITE_BALANCE_DAYLIGHT:
        awbMode = AA_AWBMODE_WB_DAYLIGHT;
        break;
    case WHITE_BALANCE_CLOUDY_DAYLIGHT:
        awbMode = AA_AWBMODE_WB_CLOUDY_DAYLIGHT;
        break;
    case WHITE_BALANCE_WARM_FLUORESCENT:
        awbMode = AA_AWBMODE_WB_WARM_FLUORESCENT;
        break;
    case WHITE_BALANCE_TWILIGHT:
        awbMode = AA_AWBMODE_WB_TWILIGHT;
        break;
    case WHITE_BALANCE_SHADE:
        awbMode = AA_AWBMODE_WB_SHADE;
        break;
    case WHITE_BALANCE_CUSTOM_K:
        awbMode = AA_AWBMODE_WB_CUSTOM_K;
        break;
    default:
        CLOGE("ERR(%s):Unsupported value(%d)", __FUNCTION__, whiteBalance);
        return BAD_VALUE;
    }

    m_cameraInfo.whiteBalanceMode = whiteBalance;
    setMetaCtlAwbMode(&m_metadata, awbMode);

    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
    m_flashMgr->setFlashWhiteBalance(awbMode);

    return NO_ERROR;
}

int ExynosCameraParameters::m_convertMetaCtlAwbMode(struct camera2_shot_ext *shot_ext)
{
    int awbMode = WHITE_BALANCE_AUTO;

    switch (shot_ext->shot.ctl.aa.awbMode) {
        case AA_AWBMODE_WB_AUTO:
            awbMode = WHITE_BALANCE_AUTO;
            break;
        case AA_AWBMODE_WB_INCANDESCENT:
            awbMode = WHITE_BALANCE_INCANDESCENT;
            break;
        case AA_AWBMODE_WB_FLUORESCENT:
            awbMode = WHITE_BALANCE_FLUORESCENT;
            break;
        case AA_AWBMODE_WB_DAYLIGHT:
            awbMode = WHITE_BALANCE_DAYLIGHT;
            break;
        case AA_AWBMODE_WB_CLOUDY_DAYLIGHT:
            awbMode = WHITE_BALANCE_CLOUDY_DAYLIGHT;
            break;
        case AA_AWBMODE_WB_WARM_FLUORESCENT:
            awbMode = WHITE_BALANCE_WARM_FLUORESCENT;
            break;
        case AA_AWBMODE_WB_TWILIGHT:
            awbMode = WHITE_BALANCE_TWILIGHT;
            break;
        case AA_AWBMODE_WB_SHADE:
            awbMode = WHITE_BALANCE_SHADE;
            break;
        default:
            ALOGE("ERR(%s):Unsupported awbMode(%d)", __FUNCTION__, shot_ext->shot.ctl.aa.awbMode);
            return BAD_VALUE;
    }

    return awbMode;
}

int ExynosCameraParameters::getWhiteBalanceMode(void)
{
    return m_cameraInfo.whiteBalanceMode;
}

int ExynosCameraParameters::getSupportedWhiteBalance(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return 0;
    } else {
        return m_staticInfo->whiteBalanceList;
    }
}

int ExynosCameraParameters::getSupportedISO(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return 0;
    } else {
        return m_staticInfo->isoValues;
    }
}

status_t ExynosCameraParameters::checkAutoWhiteBalanceLock(const CameraParameters& params)
{
    if (getHalVersion() != IS_HAL_VER_3_2) {
        bool newAutoWhiteBalanceLock = false;
        bool curAutoWhiteBalanceLock = false;
        const char *strNewAutoWhiteBalanceLock = params.get(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK);

        if (strNewAutoWhiteBalanceLock == NULL) {
            return NO_ERROR;
        }

        CLOGD("DEBUG(%s):strNewAutoWhiteBalanceLock %s", "setParameters", strNewAutoWhiteBalanceLock);

        if (!strcmp(strNewAutoWhiteBalanceLock, "true"))
            newAutoWhiteBalanceLock = true;

        curAutoWhiteBalanceLock = getAutoWhiteBalanceLock();

        if (curAutoWhiteBalanceLock != newAutoWhiteBalanceLock) {
            ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
            m_flashMgr->setAwbLock(newAutoWhiteBalanceLock);
            m_setAutoWhiteBalanceLock(newAutoWhiteBalanceLock);
            m_params.set(CameraParameters::KEY_AUTO_WHITEBALANCE_LOCK, strNewAutoWhiteBalanceLock);
        }
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setAutoWhiteBalanceLock(bool value)
{
    if (getHalVersion() != IS_HAL_VER_3_2) {
        m_cameraInfo.autoWhiteBalanceLock = value;
        setMetaCtlAwbLock(&m_metadata, value);
    }
}

bool ExynosCameraParameters::getAutoWhiteBalanceLock(void)
{
    return m_cameraInfo.autoWhiteBalanceLock;
}

#ifdef SAMSUNG_FOOD_MODE
status_t ExynosCameraParameters::checkWbLevel(const CameraParameters& params)
{
    int32_t newWbLevel = params.getInt("wb-level");
    int32_t curWbLevel = getWbLevel();
    const char *strNewSceneMode = params.get(CameraParameters::KEY_SCENE_MODE);
    int minWbLevel = -4, maxWbLevel = 4;

    if (strNewSceneMode == NULL) {
        CLOGE("ERR(%s):strNewSceneMode is NULL.", __FUNCTION__);
        return NO_ERROR;
    }

    if (!strcmp(strNewSceneMode, "food")) {
        if ((newWbLevel < minWbLevel) || (newWbLevel > maxWbLevel)) {
            ALOGE("ERR(%s): Invalid WbLevel (Min: %d, Max: %d, Value: %d)", __FUNCTION__,
                minWbLevel, maxWbLevel, newWbLevel);
            return BAD_VALUE;
        }

        ALOGD("DEBUG(%s):newWbLevel %d", "setParameters", newWbLevel);

        if (curWbLevel != newWbLevel) {
            m_setWbLevel(newWbLevel);
            m_params.set("wb-level", newWbLevel);
        }
    }

    return NO_ERROR;
}

#define IS_WBLEVEL_DEFAULT (4)

void ExynosCameraParameters::m_setWbLevel(int32_t value)
{
    setMetaCtlWbLevel(&m_metadata, value + IS_WBLEVEL_DEFAULT + FW_CUSTOM_OFFSET);
}

int32_t ExynosCameraParameters::getWbLevel(void)
{
    int32_t wbLevel;

    getMetaCtlWbLevel(&m_metadata, &wbLevel);

    return wbLevel - IS_WBLEVEL_DEFAULT - FW_CUSTOM_OFFSET;
}

#endif

status_t ExynosCameraParameters::checkWbKelvin(const CameraParameters& params)
{
    int32_t newWbKelvin = params.getInt("wb-k");
    int32_t curWbKelvin = getWbKelvin();
    const char *strNewSceneMode = params.get(CameraParameters::KEY_SCENE_MODE);
    const char *strWhiteBalance = params.get(CameraParameters::KEY_WHITE_BALANCE);
    const char *strNewWhiteBalance = m_adjustWhiteBalanceMode(strWhiteBalance);

    if (strNewSceneMode == NULL || strNewWhiteBalance == NULL) {
        CLOGE("ERR(%s):strNewSceneMode or strNewWhiteBalance is NULL.", __FUNCTION__);
        return NO_ERROR;
    }

    if (strcmp(strNewSceneMode, "food") && !strcmp(strNewWhiteBalance, "temperature")) {
        if ((newWbKelvin < getMinWBK()) || (newWbKelvin > getMaxWBK())) {
            ALOGE("ERR(%s): Invalid WbKelvin (Min: %d, Max: %d, Value: %d)", __FUNCTION__,
                    getMinWBK(), getMaxWBK(), newWbKelvin);
            return BAD_VALUE;
        }

        ALOGD("DEBUG(%s):newWbKelvin %d", "setParameters", newWbKelvin);

        if (curWbKelvin != newWbKelvin) {
            m_setWbKelvin(newWbKelvin);
            m_params.set("wb-k", newWbKelvin);
        }
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setWbKelvin(int32_t value)
{
    setMetaCtlWbLevel(&m_metadata, value);
}

int32_t ExynosCameraParameters::getWbKelvin(void)
{
    int32_t wbKelvin = 0;

    getMetaCtlWbLevel(&m_metadata, &wbKelvin);

    return wbKelvin;
}

status_t ExynosCameraParameters::checkFocusAreas(const CameraParameters& params)
{
    int ret = NO_ERROR;
    const char *newFocusAreas = params.get(CameraParameters::KEY_FOCUS_AREAS);
    int newNumFocusAreas = params.getInt(CameraParameters::KEY_MAX_NUM_FOCUS_AREAS);
    int curFocusMode = getFocusMode();
    uint32_t maxNumFocusAreas = getMaxNumFocusAreas();
    int FocusAreasCount = 0;

    if (newFocusAreas == NULL) {
        ExynosRect2 nullRect2[1];
        nullRect2[0].x1 = 0;
        nullRect2[0].y1 = 0;
        nullRect2[0].x2 = 0;
        nullRect2[0].y2 = 0;

        if (m_cameraInfo.numValidFocusArea != 0)
            m_setFocusAreas(0, nullRect2, NULL);

        return NO_ERROR;
    }

    if (newNumFocusAreas <= 0) {
        FocusAreasCount = maxNumFocusAreas;
    } else {
        FocusAreasCount = newNumFocusAreas;
    }
    CLOGD("DEBUG(%s):newFocusAreas %s, numFocusAreas(%d)", "setParameters", newFocusAreas, FocusAreasCount);

    /* In CameraParameters.h */
    /*
     * Focus area only has effect if the cur focus mode is FOCUS_MODE_AUTO,
     * FOCUS_MODE_MACRO, FOCUS_MODE_CONTINUOUS_VIDEO, or
     * FOCUS_MODE_CONTINUOUS_PICTURE.
     */
    if (curFocusMode & FOCUS_MODE_AUTO
         || curFocusMode & FOCUS_MODE_MACRO
         || curFocusMode & FOCUS_MODE_CONTINUOUS_VIDEO
         || curFocusMode & FOCUS_MODE_CONTINUOUS_PICTURE
         || curFocusMode & FOCUS_MODE_CONTINUOUS_PICTURE_MACRO
#ifdef SAMSUNG_OT
         || curFocusMode & FOCUS_MODE_OBJECT_TRACKING_VIDEO
         || curFocusMode & FOCUS_MODE_OBJECT_TRACKING_PICTURE
#endif
    ) {

        /* ex : (-10,-10,0,0,300),(0,0,10,10,700) */
        ExynosRect2 *rect2s = new ExynosRect2[FocusAreasCount];
        int         *weights = new int[FocusAreasCount];

        uint32_t validFocusedAreas = bracketsStr2Ints((char *)newFocusAreas, FocusAreasCount, rect2s, weights, 1);

        /* Check duplicate area */
        if (validFocusedAreas > 1) {
            for (uint32_t k = 0; k < validFocusedAreas; k++) {
                if (rect2s[k].x1 == rect2s[k+1].x1 &&
                        rect2s[k].y1 == rect2s[k+1].y1 &&
                        rect2s[k].x2 == rect2s[k+1].x2 &&
                        rect2s[k].y2 == rect2s[k+1].y2)
                    validFocusedAreas = 0;
            }
        }

        if (0 < validFocusedAreas) {
            /* CameraParameters.h */
            /*
              * A special case of single focus area (0,0,0,0,0) means driver to decide
              * the focus area. For example, the driver may use more signals to decide
              * focus areas and change them dynamically. Apps can set (0,0,0,0,0) if they
              * want the driver to decide focus areas.
              */
            m_setFocusAreas(validFocusedAreas, rect2s, weights);
            m_params.set(CameraParameters::KEY_FOCUS_AREAS, newFocusAreas);
        } else {
            CLOGE("ERR(%s):FocusAreas value is invalid", __FUNCTION__);
            ret = UNKNOWN_ERROR;
        }

        delete [] rect2s;
        delete [] weights;
    }

    return ret;
}

void ExynosCameraParameters::m_setFocusAreas(uint32_t numValid, ExynosRect *rects, int *weights)
{
    ExynosRect2 *rect2s = new ExynosRect2[numValid];

    for (uint32_t i = 0; i < numValid; i++)
        convertingRectToRect2(&rects[i], &rect2s[i]);

    m_setFocusAreas(numValid, rect2s, weights);

    delete [] rect2s;
}

void ExynosCameraParameters::m_setFocusAreas(uint32_t numValid, ExynosRect2 *rect2s, int *weights)
{
#ifdef SAMSUNG_OT
    int curFocusMode = getFocusMode();
#endif
    uint32_t maxNumFocusAreas = getMaxNumFocusAreas();
    if (maxNumFocusAreas < numValid)
        numValid = maxNumFocusAreas;

    if ((numValid == 1 || numValid == 0) && (isRectNull(&rect2s[0]) == true)) {
        /* m_setFocusMode(FOCUS_MODE_AUTO); */
        ExynosRect2 newRect2(0,0,0,0);
        m_activityControl->setAutoFcousArea(newRect2, 1000);

        m_activityControl->touchAFMode = false;
        m_activityControl->touchAFModeForFlash = false;
    } else {
#ifdef SAMSUNG_OT
        if(curFocusMode & FOCUS_MODE_OBJECT_TRACKING_VIDEO
            || curFocusMode & FOCUS_MODE_OBJECT_TRACKING_PICTURE
            || m_objectTrackingGet == true) {
            for (uint32_t i = 0; i < numValid; i++) {
                m_objectTrackingArea[i] = rect2s[i];
                m_objectTrackingWeight[i] = weights[i];
                m_objectTrackingAreaChanged = true;

                m_activityControl->touchAFMode = false;
                m_activityControl->touchAFModeForFlash = false;
            }

            m_cameraInfo.numValidFocusArea = numValid;

            return;
        }
#endif
        ExynosRect cropRegionRect;
        ExynosRect2 newRect2;

        getHwBayerCropRegion(&cropRegionRect.w, &cropRegionRect.h, &cropRegionRect.x, &cropRegionRect.y);

        for (uint32_t i = 0; i < numValid; i++) {
            newRect2 = convertingAndroidArea2HWAreaBcropOut(&rect2s[i], &cropRegionRect);
            /*setMetaCtlAfRegion(&m_metadata, rect2s[i].x1, rect2s[i].y1,
                                rect2s[i].x2, rect2s[i].y2, weights[i]);*/
            m_activityControl->setAutoFcousArea(newRect2, weights[i]);
        }
        m_activityControl->touchAFMode = true;
        m_activityControl->touchAFModeForFlash = true;
    }

    m_cameraInfo.numValidFocusArea = numValid;
}

void ExynosCameraParameters::getFocusAreas(int *validFocusArea, ExynosRect2 *rect2s, int *weights)
{
    *validFocusArea = m_cameraInfo.numValidFocusArea;

    if (*validFocusArea != 0) {
        /* Currently only supported 1 region */
        getMetaCtlAfRegion(&m_metadata, &rect2s->x1, &rect2s->y1,
                            &rect2s->x2, &rect2s->y2, weights);
    }
}

status_t ExynosCameraParameters::checkColorEffectMode(const CameraParameters& params)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return NO_ERROR;
    } else {
        int newEffectMode = EFFECT_NONE;
        int curEffectMode = EFFECT_NONE;
        const char *strNewEffectMode = params.get(CameraParameters::KEY_EFFECT);

#ifdef FORCE_CAL_RELOAD
        if (m_calValid == false) {
            CLOGE("ERR(%s):Cal data has error. We set aqua effect", "checkColorEffectmode");
            newEffectMode = EFFECT_AQUA;
            m_setColorEffectMode(newEffectMode);
            m_params.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_AQUA);

            m_frameSkipCounter.setCount(EFFECT_SKIP_FRAME);
            return NO_ERROR;
        }
#endif

        if (strNewEffectMode == NULL) {
            return NO_ERROR;
        }

        CLOGD("DEBUG(%s):strNewEffectMode %s", "setParameters", strNewEffectMode);

        if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_NONE)) {
            newEffectMode = EFFECT_NONE;
        } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_MONO)) {
            newEffectMode = EFFECT_MONO;
        } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_NEGATIVE)) {
            newEffectMode = EFFECT_NEGATIVE;
        } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_SOLARIZE)) {
            newEffectMode = EFFECT_SOLARIZE;
        } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_SEPIA)) {
            newEffectMode = EFFECT_SEPIA;
        } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_POSTERIZE)) {
            newEffectMode = EFFECT_POSTERIZE;
        } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_WHITEBOARD)) {
            newEffectMode = EFFECT_WHITEBOARD;
        } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_BLACKBOARD)) {
            newEffectMode = EFFECT_BLACKBOARD;
        } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_AQUA)) {
            newEffectMode = EFFECT_AQUA;
        } else if (!strcmp(strNewEffectMode, CameraParameters::EFFECT_POINT_BLUE)) {
            newEffectMode = EFFECT_BLUE;
        } else if (!strcmp(strNewEffectMode, "point-red-yellow")) {
            newEffectMode = EFFECT_RED_YELLOW;
        } else if (!strcmp(strNewEffectMode, "vintage-cold")) {
            newEffectMode = EFFECT_COLD_VINTAGE;
        } else if (!strcmp(strNewEffectMode, "beauty" )) {
            newEffectMode = EFFECT_BEAUTY_FACE;
        } else {
            CLOGE("ERR(%s):Invalid effect(%s)", __FUNCTION__, strNewEffectMode);
            return BAD_VALUE;
        }

        if (!isSupportedColorEffects(newEffectMode)) {
            CLOGE("ERR(%s[%d]): Effect mode(%s) is not supported!", __FUNCTION__, __LINE__, strNewEffectMode);
            return BAD_VALUE;
        }

        curEffectMode = getColorEffectMode();

        if (curEffectMode != newEffectMode) {
            m_setColorEffectMode(newEffectMode);
            m_params.set(CameraParameters::KEY_EFFECT, strNewEffectMode);

            m_frameSkipCounter.setCount(EFFECT_SKIP_FRAME);
        }
        return NO_ERROR;
    }
}

void ExynosCameraParameters::m_setColorEffectMode(int effect)
{
    aa_effect_mode_t newEffect;

    switch(effect) {
    case EFFECT_NONE:
        newEffect = AA_EFFECT_OFF;
        break;
    case EFFECT_MONO:
        newEffect = AA_EFFECT_MONO;
        break;
    case EFFECT_NEGATIVE:
        newEffect = AA_EFFECT_NEGATIVE;
        break;
    case EFFECT_SOLARIZE:
        newEffect = AA_EFFECT_SOLARIZE;
        break;
    case EFFECT_SEPIA:
        newEffect = AA_EFFECT_SEPIA;
        break;
    case EFFECT_POSTERIZE:
        newEffect = AA_EFFECT_POSTERIZE;
        break;
    case EFFECT_WHITEBOARD:
        newEffect = AA_EFFECT_WHITEBOARD;
        break;
    case EFFECT_BLACKBOARD:
        newEffect = AA_EFFECT_BLACKBOARD;
        break;
    case EFFECT_AQUA:
        newEffect = AA_EFFECT_AQUA;
        break;
    case EFFECT_RED_YELLOW:
        newEffect = AA_EFFECT_RED_YELLOW_POINT;
        break;
    case EFFECT_BLUE:
        newEffect = AA_EFFECT_BLUE_POINT;
        break;
    case EFFECT_WARM_VINTAGE:
        newEffect = AA_EFFECT_WARM_VINTAGE;
        break;
    case EFFECT_COLD_VINTAGE:
        newEffect = AA_EFFECT_COLD_VINTAGE;
        break;
    case EFFECT_BEAUTY_FACE:
        newEffect = AA_EFFECT_BEAUTY_FACE;
        break;
    default:
        newEffect = AA_EFFECT_OFF;
        CLOGE("ERR(%s[%d]):Color Effect mode(%d) is not supported", __FUNCTION__, __LINE__, effect);
        break;
    }
    setMetaCtlAaEffect(&m_metadata, newEffect);
}

int ExynosCameraParameters::getColorEffectMode(void)
{
    aa_effect_mode_t curEffect;
    int effect;

    getMetaCtlAaEffect(&m_metadata, &curEffect);

    switch(curEffect) {
    case AA_EFFECT_OFF:
        effect = EFFECT_NONE;
        break;
    case AA_EFFECT_MONO:
        effect = EFFECT_MONO;
        break;
    case AA_EFFECT_NEGATIVE:
        effect = EFFECT_NEGATIVE;
        break;
    case AA_EFFECT_SOLARIZE:
        effect = EFFECT_SOLARIZE;
        break;
    case AA_EFFECT_SEPIA:
        effect = EFFECT_SEPIA;
        break;
    case AA_EFFECT_POSTERIZE:
        effect = EFFECT_POSTERIZE;
        break;
    case AA_EFFECT_WHITEBOARD:
        effect = EFFECT_WHITEBOARD;
        break;
    case AA_EFFECT_BLACKBOARD:
        effect = EFFECT_BLACKBOARD;
        break;
    case AA_EFFECT_AQUA:
        effect = EFFECT_AQUA;
        break;
    case AA_EFFECT_RED_YELLOW_POINT:
        effect = EFFECT_RED_YELLOW;
        break;
    case AA_EFFECT_BLUE_POINT:
        effect = EFFECT_BLUE;
        break;
    case AA_EFFECT_WARM_VINTAGE:
        effect = EFFECT_WARM_VINTAGE;
        break;
    case AA_EFFECT_COLD_VINTAGE:
        effect = EFFECT_COLD_VINTAGE;
        break;
    case AA_EFFECT_BEAUTY_FACE:
        effect = EFFECT_BEAUTY_FACE;
        break;
    default:
        effect = 0;
        CLOGE("ERR(%s[%d]):Color Effect mode(%d) is invalid value", __FUNCTION__, __LINE__, curEffect);
        break;
    }

    return effect;
}

int ExynosCameraParameters::getSupportedColorEffects(void)
{
    return m_staticInfo->effectList;
}

bool ExynosCameraParameters::isSupportedColorEffects(int effectMode)
{
    int ret = false;

    if (effectMode & getSupportedColorEffects()) {
        return true;
    }

    if (effectMode & m_staticInfo->hiddenEffectList) {
        return true;
    }

    return ret;
}

status_t ExynosCameraParameters::checkGpsAltitude(const CameraParameters& params)
{
    double newAltitude = 0;
    double curAltitude = 0;
    const char *strNewGpsAltitude = params.get(CameraParameters::KEY_GPS_ALTITUDE);

    if (strNewGpsAltitude == NULL) {
        m_params.remove(CameraParameters::KEY_GPS_ALTITUDE);
        m_setGpsAltitude(0);
        return NO_ERROR;
    }

    CLOGV("DEBUG(%s):strNewGpsAltitude %s", "setParameters", strNewGpsAltitude);

    newAltitude = atof(strNewGpsAltitude);
    curAltitude = getGpsAltitude();

    if (curAltitude != newAltitude) {
        m_setGpsAltitude(newAltitude);
        m_params.set(CameraParameters::KEY_GPS_ALTITUDE, strNewGpsAltitude);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setGpsAltitude(double altitude)
{
    m_cameraInfo.gpsAltitude = altitude;
}

double ExynosCameraParameters::getGpsAltitude(void)
{
    return m_cameraInfo.gpsAltitude;
}

status_t ExynosCameraParameters::checkGpsLatitude(const CameraParameters& params)
{
    double newLatitude = 0;
    double curLatitude = 0;
    const char *strNewGpsLatitude = params.get(CameraParameters::KEY_GPS_LATITUDE);

    if (strNewGpsLatitude == NULL) {
        m_params.remove(CameraParameters::KEY_GPS_LATITUDE);
        m_setGpsLatitude(0);
        return NO_ERROR;
    }

    CLOGV("DEBUG(%s):strNewGpsLatitude %s", "setParameters", strNewGpsLatitude);

    newLatitude = atof(strNewGpsLatitude);
    curLatitude = getGpsLatitude();

    if (curLatitude != newLatitude) {
        m_setGpsLatitude(newLatitude);
        m_params.set(CameraParameters::KEY_GPS_LATITUDE, strNewGpsLatitude);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setGpsLatitude(double latitude)
{
    m_cameraInfo.gpsLatitude = latitude;
}

double ExynosCameraParameters::getGpsLatitude(void)
{
    return m_cameraInfo.gpsLatitude;
}

status_t ExynosCameraParameters::checkGpsLongitude(const CameraParameters& params)
{
    double newLongitude = 0;
    double curLongitude = 0;
    const char *strNewGpsLongitude = params.get(CameraParameters::KEY_GPS_LONGITUDE);

    if (strNewGpsLongitude == NULL) {
        m_params.remove(CameraParameters::KEY_GPS_LONGITUDE);
        m_setGpsLongitude(0);
        return NO_ERROR;
    }

    CLOGV("DEBUG(%s):strNewGpsLongitude %s", "setParameters", strNewGpsLongitude);

    newLongitude = atof(strNewGpsLongitude);
    curLongitude = getGpsLongitude();

    if (curLongitude != newLongitude) {
        m_setGpsLongitude(newLongitude);
        m_params.set(CameraParameters::KEY_GPS_LONGITUDE, strNewGpsLongitude);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setGpsLongitude(double longitude)
{
    m_cameraInfo.gpsLongitude = longitude;
}

double ExynosCameraParameters::getGpsLongitude(void)
{
    return m_cameraInfo.gpsLongitude;
}

status_t ExynosCameraParameters::checkGpsProcessingMethod(const CameraParameters& params)
{
    // gps processing method
    const char *strNewGpsProcessingMethod = params.get(CameraParameters::KEY_GPS_PROCESSING_METHOD);
    const char *strCurGpsProcessingMethod = NULL;
    bool changeMethod = false;

    if (strNewGpsProcessingMethod == NULL) {
        m_params.remove(CameraParameters::KEY_GPS_PROCESSING_METHOD);
        m_setGpsProcessingMethod(NULL);
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):strNewGpsProcessingMethod %s", "setParameters", strNewGpsProcessingMethod);

    strCurGpsProcessingMethod = getGpsProcessingMethod();

    if (strCurGpsProcessingMethod != NULL) {
        int newLen = strlen(strNewGpsProcessingMethod);
        int curLen = strlen(strCurGpsProcessingMethod);

        if (newLen != curLen)
            changeMethod = true;
        else
            changeMethod = strncmp(strNewGpsProcessingMethod, strCurGpsProcessingMethod, newLen);
    }

    if (changeMethod == true) {
        m_setGpsProcessingMethod(strNewGpsProcessingMethod);
        m_params.set(CameraParameters::KEY_GPS_PROCESSING_METHOD, strNewGpsProcessingMethod);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setGpsProcessingMethod(const char *gpsProcessingMethod)
{
    memset(m_exifInfo.gps_processing_method, 0, sizeof(m_exifInfo.gps_processing_method));
    if (gpsProcessingMethod == NULL)
        return;

    size_t len = strlen(gpsProcessingMethod);

    if (len > sizeof(m_exifInfo.gps_processing_method)) {
        len = sizeof(m_exifInfo.gps_processing_method);
    }
    memcpy(m_exifInfo.gps_processing_method, gpsProcessingMethod, len);
}

const char *ExynosCameraParameters::getGpsProcessingMethod(void)
{
    return (const char *)m_exifInfo.gps_processing_method;
}

void ExynosCameraParameters::m_setExifFixedAttribute(void)
{
    char property[PROPERTY_VALUE_MAX];

    memset(&m_exifInfo, 0, sizeof(m_exifInfo));

    /* 2 0th IFD TIFF Tags */
    /* 3 Maker */
    strncpy((char *)m_exifInfo.maker, EXIF_DEF_MAKER,
                sizeof(m_exifInfo.maker) - 1);
    m_exifInfo.maker[sizeof(EXIF_DEF_MAKER) - 1] = '\0';

    /* 3 Model */
    property_get("ro.product.model", property, EXIF_DEF_MODEL);
    strncpy((char *)m_exifInfo.model, property,
                sizeof(m_exifInfo.model) - 1);
    m_exifInfo.model[sizeof(m_exifInfo.model) - 1] = '\0';
    /* 3 Software */
    property_get("ro.build.PDA", property, EXIF_DEF_SOFTWARE);
    strncpy((char *)m_exifInfo.software, property,
                sizeof(m_exifInfo.software) - 1);
    m_exifInfo.software[sizeof(m_exifInfo.software) - 1] = '\0';

    /* 3 YCbCr Positioning */
    m_exifInfo.ycbcr_positioning = EXIF_DEF_YCBCR_POSITIONING;

    /*2 0th IFD Exif Private Tags */
    /* 3 Exif Version */
    memcpy(m_exifInfo.exif_version, EXIF_DEF_EXIF_VERSION, sizeof(m_exifInfo.exif_version));

    if (getHalVersion() == IS_HAL_VER_3_2) {
        /* 3 Aperture */
        m_exifInfo.aperture.num = (int) m_staticInfo->aperture * COMMON_DENOMINATOR;
        m_exifInfo.aperture.den = COMMON_DENOMINATOR;
        /* 3 F Number */
        m_exifInfo.fnumber.num = m_staticInfo->fNumber * COMMON_DENOMINATOR;
        m_exifInfo.fnumber.den = COMMON_DENOMINATOR;
        /* 3 Maximum lens aperture */
        m_exifInfo.max_aperture.num = m_staticInfo->aperture * COMMON_DENOMINATOR;
        m_exifInfo.max_aperture.den = COMMON_DENOMINATOR;
        /* 3 Lens Focal Length */
        m_exifInfo.focal_length.num = m_staticInfo->focalLength * COMMON_DENOMINATOR;
        m_exifInfo.focal_length.den = COMMON_DENOMINATOR;
    } else {
        m_exifInfo.aperture.num = m_staticInfo->apertureNum;
        m_exifInfo.aperture.den = m_staticInfo->apertureDen;
        /* 3 F Number */
        m_exifInfo.fnumber.num = m_staticInfo->fNumberNum;
        m_exifInfo.fnumber.den = m_staticInfo->fNumberDen;
        /* 3 Maximum lens aperture */
        m_exifInfo.max_aperture.num = m_staticInfo->apertureNum;
        m_exifInfo.max_aperture.den = m_staticInfo->apertureDen;
        /* 3 Lens Focal Length */
        m_exifInfo.focal_length.num = m_staticInfo->focalLengthNum;
        m_exifInfo.focal_length.den = m_staticInfo->focalLengthDen;
    }

    /* 3 Maker note */
    if (m_exifInfo.maker_note)
        delete m_exifInfo.maker_note;

    m_exifInfo.maker_note_size = 98;
    m_exifInfo.maker_note = new unsigned char[m_exifInfo.maker_note_size];
    memset((void *)m_exifInfo.maker_note, 0, m_exifInfo.maker_note_size);
    /* 3 User Comments */
    if (m_exifInfo.user_comment)
        delete m_exifInfo.user_comment;

    m_exifInfo.user_comment_size = sizeof("user comment");
    m_exifInfo.user_comment = new unsigned char[m_exifInfo.user_comment_size + 8];
    memset((void *)m_exifInfo.user_comment, 0, m_exifInfo.user_comment_size + 8);

    /* 3 Color Space information */
    m_exifInfo.color_space = EXIF_DEF_COLOR_SPACE;
    /* 3 interoperability */
    m_exifInfo.interoperability_index = EXIF_DEF_INTEROPERABILITY;
    /* 3 Exposure Mode */
    m_exifInfo.exposure_mode = EXIF_DEF_EXPOSURE_MODE;

    /* 2 0th IFD GPS Info Tags */
    unsigned char gps_version[4] = { 0x02, 0x02, 0x00, 0x00 };
    memcpy(m_exifInfo.gps_version_id, gps_version, sizeof(gps_version));

    /* 2 1th IFD TIFF Tags */
    m_exifInfo.compression_scheme = EXIF_DEF_COMPRESSION;
    m_exifInfo.x_resolution.num = EXIF_DEF_RESOLUTION_NUM;
    m_exifInfo.x_resolution.den = EXIF_DEF_RESOLUTION_DEN;
    m_exifInfo.y_resolution.num = EXIF_DEF_RESOLUTION_NUM;
    m_exifInfo.y_resolution.den = EXIF_DEF_RESOLUTION_DEN;
    m_exifInfo.resolution_unit = EXIF_DEF_RESOLUTION_UNIT;
}

void ExynosCameraParameters::setExifChangedAttribute(exif_attribute_t   *exifInfo,
                                                     ExynosRect         *pictureRect,
                                                     ExynosRect         *thumbnailRect,
                                                     camera2_shot_t     *shot)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        m_setExifChangedAttribute(exifInfo, pictureRect, thumbnailRect, shot);
    } else {
        m_setExifChangedAttribute(exifInfo, pictureRect, thumbnailRect, &(shot->dm), &(shot->udm));
    }
}

void ExynosCameraParameters::m_setExifChangedAttribute(exif_attribute_t *exifInfo,
                                                       ExynosRect       *pictureRect,
                                                       ExynosRect       *thumbnailRect,
                                                       __unused camera2_dm       *dm,
                                                       camera2_udm      *udm)
{
    /* 2 0th IFD TIFF Tags */
    /* 3 Width */
    exifInfo->width = pictureRect->w;
    /* 3 Height */
    exifInfo->height = pictureRect->h;

    /* 3 Orientation */
    switch (m_cameraInfo.rotation) {
    case 90:
        exifInfo->orientation = EXIF_ORIENTATION_90;
        break;
    case 180:
        exifInfo->orientation = EXIF_ORIENTATION_180;
        break;
    case 270:
        exifInfo->orientation = EXIF_ORIENTATION_270;
        break;
    case 0:
    default:
        exifInfo->orientation = EXIF_ORIENTATION_UP;
        break;
    }

    /* 3 Maker note */
    /* back-up udm info for exif's maker note */
    memcpy((void *)mDebugInfo.debugData[4], (void *)udm, sizeof(struct camera2_udm));
    unsigned int offset = sizeof(struct camera2_udm);

#ifdef SAMSUNG_OIS
    if (getCameraId() == CAMERA_ID_BACK) {
        getOisEXIFFromFile(m_staticInfo, (int)m_cameraInfo.oisMode);
        /* Copy ois data to debugData*/
        memcpy((void *)(mDebugInfo.debugData[4] + offset),
                (void *)&m_staticInfo->ois_exif_info, sizeof(m_staticInfo->ois_exif_info));

        offset += sizeof(m_staticInfo->ois_exif_info);
    }
#endif

#ifdef SAMSUNG_BD
    /* Copy bd data to debugData */
    if(getSeriesShotMode() == SERIES_SHOT_MODE_BURST) {
        memcpy((void *)(mDebugInfo.debugData[4] + offset),
                (void *)&m_staticInfo->bd_exif_info, sizeof(m_staticInfo->bd_exif_info));
        offset += sizeof(m_staticInfo->bd_exif_info);
    }
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    int llsMode = getLDCaptureMode();
    if(llsMode == MULTI_SHOT_MODE_MULTI1 || llsMode == MULTI_SHOT_MODE_MULTI2
        || llsMode == MULTI_SHOT_MODE_MULTI3) {
        memcpy((void *)(mDebugInfo.debugData[4] + offset),
                (void *)&m_staticInfo->lls_exif_info, sizeof(m_staticInfo->lls_exif_info));
    }
#endif

    /* TODO */
#if 0
    if (getSeriesShotCount() && getShotMode() != SHOT_MODE_BEST_PHOTO) {
        unsigned char l_makernote[98] = { 0x07, 0x00, 0x01, 0x00, 0x07, 0x00, 0x04, 0x00, 0x00, 0x00,
                                          0x30, 0x31, 0x30, 0x30, 0x02, 0x00, 0x04, 0x00, 0x01, 0x00,
                                          0x00, 0x00, 0x00, 0x20, 0x01, 0x00, 0x40, 0x00, 0x04, 0x00,
                                          0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00,
                                          0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                          0x10, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x5A, 0x00,
                                          0x00, 0x00, 0x50, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
                                          0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x01, 0x00,
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        long long int mCityId = getCityId();
        l_makernote[46] = getWeatherId();
        memcpy(l_makernote + 90, &mCityId, 8);
        exifInfo->maker_note_size = 98;
        memcpy(exifInfo->maker_note, l_makernote, sizeof(l_makernote));
    } else {
        exifInfo->maker_note_size = 0;
    }
#else
    exifInfo->maker_note_size = 0;
#endif

    /* 3 Date time */
    struct timeval rawtime;
    struct tm timeinfo;
    gettimeofday(&rawtime, NULL);
    localtime_r((time_t *)&rawtime.tv_sec, &timeinfo);
    strftime((char *)exifInfo->date_time, 20, "%Y:%m:%d %H:%M:%S", &timeinfo);
    snprintf((char *)exifInfo->sec_time, 5, "%04d", (int)(rawtime.tv_usec/1000));

#ifdef SAMSUNG_UTC_TS
    gmtime_r((time_t *)&rawtime.tv_sec, &timeinfo);
    struct utc_ts m_utc;
    strftime((char *)&m_utc.utc_ts_data[4], 20, "%Y:%m:%d %H:%M:%S", &timeinfo);
    /* UTC Time Info Tag : 0x1001 */
    m_utc.utc_ts_data[0] = 0x10;
    m_utc.utc_ts_data[1] = 0x01;
    m_utc.utc_ts_data[2] = 0x00;
    m_utc.utc_ts_data[3] = 0x16;
    memcpy(mDebugInfo.debugData[5], m_utc.utc_ts_data, sizeof(struct utc_ts));
#endif


    /* 2 0th IFD Exif Private Tags */
    bool flagSLSIAlgorithm = true;
    /*
     * vendorSpecific2[100]      : exposure
     * vendorSpecific2[101]      : iso(gain)
     * vendorSpecific2[102] /256 : Bv
     * vendorSpecific2[103]      : Tv
     */

    /* 3 ISO Speed Rating */
    exifInfo->iso_speed_rating = udm->internal.vendorSpecific2[101];

    /* 3 Exposure Program */
    if (m_exposureTimeCapture == 0) {
        m_exifInfo.exposure_program = EXIF_DEF_EXPOSURE_PROGRAM;
    } else {
        m_exifInfo.exposure_program = EXIF_DEF_EXPOSURE_MANUAL;
    }

    /* 3 Exposure Time */
    if (m_exposureTimeCapture == 0) {
        exifInfo->exposure_time.num = 1;

        if (udm->ae.vendorSpecific[0] == 0xAEAEAEAE)
            exifInfo->exposure_time.den = (uint32_t)udm->ae.vendorSpecific[64];
        else
            exifInfo->exposure_time.den = (uint32_t)udm->internal.vendorSpecific2[100];
    } else if (m_exposureTimeCapture > 0 && m_exposureTimeCapture <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        exifInfo->exposure_time.num = 1;
        if ((1000000 % m_exposureTimeCapture) != 0) {
            uint32_t dig;
            if (m_exposureTimeCapture < 100) {
                dig = 1000;
            } else if (m_exposureTimeCapture < 1000) {
                dig = 100;
            } else if (m_exposureTimeCapture < 10000) {
                dig = 10;
            } else {
                dig = 5;
            }
            exifInfo->exposure_time.den = ROUND_OFF_DIGIT((int)(1000000 / m_exposureTimeCapture), dig);
        } else {
            exifInfo->exposure_time.den = (int)(1000000 / m_exposureTimeCapture);
        }
    } else {
        exifInfo->exposure_time.num = m_exposureTimeCapture / 1000;
        exifInfo->exposure_time.den = 1000;
    }

    /* 3 Shutter Speed */
    if (m_exposureTimeCapture <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        if (udm->ae.vendorSpecific[0] == 0xAEAEAEAE)
            exifInfo->shutter_speed.num = (uint32_t)(ROUND_OFF_HALF(((double)(udm->ae.vendorSpecific[69] / 256.f) * EXIF_DEF_APEX_DEN), 0));
        else
            exifInfo->shutter_speed.num = (uint32_t)(ROUND_OFF_HALF(((double)(udm->internal.vendorSpecific2[103] / 256.f) * EXIF_DEF_APEX_DEN), 0));

        exifInfo->shutter_speed.den = EXIF_DEF_APEX_DEN;
    } else {
        exifInfo->shutter_speed.num = (int32_t)(log2((double)m_exposureTimeCapture / 1000000) * -100);
        exifInfo->shutter_speed.den = 100;
    }

    if (getHalVersion() == IS_HAL_VER_3_2) {
        /* 3 Aperture */
        exifInfo->aperture.num = APEX_FNUM_TO_APERTURE((double)(exifInfo->fnumber.num) / (double)(exifInfo->fnumber.den)) * COMMON_DENOMINATOR;
        exifInfo->aperture.den = COMMON_DENOMINATOR;

        /* 3 Max Aperture */
        exifInfo->max_aperture.num = APEX_FNUM_TO_APERTURE((double)(exifInfo->fnumber.num) / (double)(exifInfo->fnumber.den)) * COMMON_DENOMINATOR;
        exifInfo->max_aperture.den = COMMON_DENOMINATOR;
    } else {
        /* 3 Aperture */
        exifInfo->aperture.num = APEX_FNUM_TO_APERTURE((double)(exifInfo->fnumber.num) / (double)(exifInfo->fnumber.den)) * m_staticInfo->apertureDen;
        exifInfo->aperture.den = m_staticInfo->apertureDen;

        /* 3 Max Aperture */
        exifInfo->max_aperture.num = APEX_FNUM_TO_APERTURE((double)(exifInfo->fnumber.num) / (double)(exifInfo->fnumber.den)) * m_staticInfo->apertureDen;
        exifInfo->max_aperture.den = m_staticInfo->apertureDen;
    }

    /* 3 Brightness */
    int temp = udm->internal.vendorSpecific2[102];
    if ((int)udm->ae.vendorSpecific[102] < 0)
        temp = -temp;

    exifInfo->brightness.num = (int32_t)(ROUND_OFF_HALF((double)((temp * EXIF_DEF_APEX_DEN) / 256.f), 0));
    if ((int)udm->ae.vendorSpecific[102] < 0)
        exifInfo->brightness.num = -exifInfo->brightness.num;

    exifInfo->brightness.den = EXIF_DEF_APEX_DEN;

    CLOGD("DEBUG(%s):udm->internal.vendorSpecific2[100](%d)", __FUNCTION__, udm->internal.vendorSpecific2[100]);
    CLOGD("DEBUG(%s):udm->internal.vendorSpecific2[101](%d)", __FUNCTION__, udm->internal.vendorSpecific2[101]);
    CLOGD("DEBUG(%s):udm->internal.vendorSpecific2[102](%d)", __FUNCTION__, udm->internal.vendorSpecific2[102]);
    CLOGD("DEBUG(%s):udm->internal.vendorSpecific2[103](%d)", __FUNCTION__, udm->internal.vendorSpecific2[103]);

    CLOGD("DEBUG(%s):iso_speed_rating(%d)", __FUNCTION__, exifInfo->iso_speed_rating);
    CLOGD("DEBUG(%s):exposure_time(%d/%d)", __FUNCTION__, exifInfo->exposure_time.num, exifInfo->exposure_time.den);
    CLOGD("DEBUG(%s):shutter_speed(%d/%d)", __FUNCTION__, exifInfo->shutter_speed.num, exifInfo->shutter_speed.den);
    CLOGD("DEBUG(%s):aperture     (%d/%d)", __FUNCTION__, exifInfo->aperture.num, exifInfo->aperture.den);
    CLOGD("DEBUG(%s):brightness   (%d/%d)", __FUNCTION__, exifInfo->brightness.num, exifInfo->brightness.den);

    /* 3 Exposure Bias */
    if (m_exposureTimeCapture == 0) {
        exifInfo->exposure_bias.num = (int32_t)getExposureCompensation() * (m_staticInfo->exposureCompensationStep * 10);
        exifInfo->exposure_bias.den = 10;
    } else {
        exifInfo->exposure_bias.num = 0;
        exifInfo->exposure_bias.den = 1;
    }

    /* 3 Metering Mode */
#ifdef SAMSUNG_COMPANION
    enum companion_wdr_mode wdr_mode;
    wdr_mode = getRTHdr();
    if (wdr_mode == COMPANION_WDR_ON) {
        exifInfo->metering_mode = EXIF_METERING_PATTERN;
    } else
#endif
    {
        switch (m_cameraInfo.meteringMode) {
        case METERING_MODE_CENTER:
            exifInfo->metering_mode = EXIF_METERING_CENTER;
            break;
        case METERING_MODE_MATRIX:
            exifInfo->metering_mode = EXIF_METERING_AVERAGE;
            break;
        case METERING_MODE_SPOT:
#ifdef TOUCH_AE
        case METERING_MODE_CENTER_TOUCH:
        case METERING_MODE_SPOT_TOUCH:
        case METERING_MODE_AVERAGE_TOUCH:
        case METERING_MODE_MATRIX_TOUCH:
#endif
            exifInfo->metering_mode = EXIF_METERING_SPOT;
            break;
        case METERING_MODE_AVERAGE:
        default:
            exifInfo->metering_mode = EXIF_METERING_AVERAGE;
            break;
        }

#ifdef SAMSUNG_FOOD_MODE
        if(getSceneMode() == SCENE_MODE_FOOD) {
            exifInfo->metering_mode = EXIF_METERING_AVERAGE;
        }
#endif
    }

    /* 3 Flash */
    if (m_cameraInfo.flashMode == FLASH_MODE_OFF) {
        exifInfo->flash = 0;
    } else if (m_cameraInfo.flashMode == FLASH_MODE_TORCH) {
        exifInfo->flash = 1;
    } else {
        exifInfo->flash = getMarkingOfExifFlash();
    }

    /* 3 White Balance */
    if (m_cameraInfo.whiteBalanceMode == WHITE_BALANCE_AUTO)
        exifInfo->white_balance = EXIF_WB_AUTO;
    else
        exifInfo->white_balance = EXIF_WB_MANUAL;

    /* 3 Focal Length in 35mm length */
    exifInfo->focal_length_in_35mm_length = m_staticInfo->focalLengthIn35mmLength;

    /* 3 Scene Capture Type */
    switch (m_cameraInfo.sceneMode) {
    case SCENE_MODE_PORTRAIT:
        exifInfo->scene_capture_type = EXIF_SCENE_PORTRAIT;
        break;
    case SCENE_MODE_LANDSCAPE:
        exifInfo->scene_capture_type = EXIF_SCENE_LANDSCAPE;
        break;
    case SCENE_MODE_NIGHT:
        exifInfo->scene_capture_type = EXIF_SCENE_NIGHT;
        break;
    default:
        exifInfo->scene_capture_type = EXIF_SCENE_STANDARD;
        break;
    }

    switch (this->getShotMode()) {
    case SHOT_MODE_BEAUTY_FACE:
    case SHOT_MODE_BEST_FACE:
        exifInfo->scene_capture_type = EXIF_SCENE_PORTRAIT;
        break;
    default:
        break;
    }

    /* 3 Image Unique ID */
#if defined(SAMSUNG_TN_FEATURE) && defined(SENSOR_FW_GET_FROM_FILE)
    char *front_fw = NULL;
    char *savePtr;

    if (getCameraId() == CAMERA_ID_BACK){
        memset(exifInfo->unique_id, 0, sizeof(exifInfo->unique_id));
        strncpy((char *)exifInfo->unique_id,
		getSensorFWFromFile(m_staticInfo, m_cameraId), sizeof(exifInfo->unique_id) - 1);
    } else if (getCameraId() == CAMERA_ID_FRONT) {
        front_fw = strtok_r((char *)getSensorFWFromFile(m_staticInfo, m_cameraId), " ", &savePtr);
        strcpy((char *)exifInfo->unique_id, front_fw);
    }
#endif

    /* 2 0th IFD GPS Info Tags */
    if (m_cameraInfo.gpsLatitude != 0 && m_cameraInfo.gpsLongitude != 0) {
        if (m_cameraInfo.gpsLatitude > 0)
            strncpy((char *)exifInfo->gps_latitude_ref, "N", 2);
        else
            strncpy((char *)exifInfo->gps_latitude_ref, "S", 2);

        if (m_cameraInfo.gpsLongitude > 0)
            strncpy((char *)exifInfo->gps_longitude_ref, "E", 2);
        else
            strncpy((char *)exifInfo->gps_longitude_ref, "W", 2);

        if (m_cameraInfo.gpsAltitude > 0)
            exifInfo->gps_altitude_ref = 0;
        else
            exifInfo->gps_altitude_ref = 1;

        double latitude = fabs(m_cameraInfo.gpsLatitude);
        double longitude = fabs(m_cameraInfo.gpsLongitude);
        double altitude = fabs(m_cameraInfo.gpsAltitude);

        exifInfo->gps_latitude[0].num = (uint32_t)latitude;
        exifInfo->gps_latitude[0].den = 1;
        exifInfo->gps_latitude[1].num = (uint32_t)((latitude - exifInfo->gps_latitude[0].num) * 60);
        exifInfo->gps_latitude[1].den = 1;
        exifInfo->gps_latitude[2].num = (uint32_t)(round((((latitude - exifInfo->gps_latitude[0].num) * 60)
                                        - exifInfo->gps_latitude[1].num) * 60));
        exifInfo->gps_latitude[2].den = 1;

        exifInfo->gps_longitude[0].num = (uint32_t)longitude;
        exifInfo->gps_longitude[0].den = 1;
        exifInfo->gps_longitude[1].num = (uint32_t)((longitude - exifInfo->gps_longitude[0].num) * 60);
        exifInfo->gps_longitude[1].den = 1;
        exifInfo->gps_longitude[2].num = (uint32_t)(round((((longitude - exifInfo->gps_longitude[0].num) * 60)
                                        - exifInfo->gps_longitude[1].num) * 60));
        exifInfo->gps_longitude[2].den = 1;

        exifInfo->gps_altitude.num = (uint32_t)altitude;
        exifInfo->gps_altitude.den = 1;

        struct tm tm_data;
        gmtime_r(&m_cameraInfo.gpsTimeStamp, &tm_data);
        exifInfo->gps_timestamp[0].num = tm_data.tm_hour;
        exifInfo->gps_timestamp[0].den = 1;
        exifInfo->gps_timestamp[1].num = tm_data.tm_min;
        exifInfo->gps_timestamp[1].den = 1;
        exifInfo->gps_timestamp[2].num = tm_data.tm_sec;
        exifInfo->gps_timestamp[2].den = 1;
        snprintf((char*)exifInfo->gps_datestamp, sizeof(exifInfo->gps_datestamp),
                "%04d:%02d:%02d", tm_data.tm_year + 1900, tm_data.tm_mon + 1, tm_data.tm_mday);

        exifInfo->enableGps = true;
    } else {
        exifInfo->enableGps = false;
    }

    /* 2 1th IFD TIFF Tags */
    exifInfo->widthThumb = thumbnailRect->w;
    exifInfo->heightThumb = thumbnailRect->h;

    setMarkingOfExifFlash(0);
}

/* for CAMERA2_API_SUPPORT */
void ExynosCameraParameters::m_setExifChangedAttribute(exif_attribute_t *exifInfo,
                                                       ExynosRect       *pictureRect,
                                                       ExynosRect       *thumbnailRect,
                                                       camera2_shot_t   *shot)
{
    /* JPEG Picture Size */
    exifInfo->width = pictureRect->w;
    exifInfo->height = pictureRect->h;

    /* Orientation */
    switch (shot->ctl.jpeg.orientation) {
    case 90:
        exifInfo->orientation = EXIF_ORIENTATION_90;
        break;
    case 180:
        exifInfo->orientation = EXIF_ORIENTATION_180;
        break;
    case 270:
        exifInfo->orientation = EXIF_ORIENTATION_270;
        break;
    case 0:
    default:
        exifInfo->orientation = EXIF_ORIENTATION_UP;
        break;
    }

    /* Maker Note Size */
    /* back-up udm info for exif's maker note */
#ifdef SAMSUNG_OIS
    if (getCameraId() == CAMERA_ID_BACK) {
        memcpy((void *)mDebugInfo.debugData[4], (void *)&shot->udm, sizeof(struct camera2_udm));
        getOisEXIFFromFile(m_staticInfo, (int)m_cameraInfo.oisMode);

        /* Copy ois data to debugData*/
        memcpy((void *)(mDebugInfo.debugData[4] + sizeof(struct camera2_udm)),
                (void *)&m_staticInfo->ois_exif_info, sizeof(m_staticInfo->ois_exif_info));
    } else {
        memcpy((void *)mDebugInfo.debugData[4], (void *)&shot->udm, mDebugInfo.debugSize[4]);
    }
#else
    memcpy((void *)mDebugInfo.debugData[4], (void *)&shot->udm, mDebugInfo.debugSize[4]);
#endif

    /* TODO */
#if 0
    if (getSeriesShotCount() && getShotMode() != SHOT_MODE_BEST_PHOTO) {
        unsigned char l_makernote[98] = { 0x07, 0x00, 0x01, 0x00, 0x07, 0x00, 0x04, 0x00, 0x00, 0x00,
            0x30, 0x31, 0x30, 0x30, 0x02, 0x00, 0x04, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x20, 0x01, 0x00, 0x40, 0x00, 0x04, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00,
            0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x10, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x5A, 0x00,
            0x00, 0x00, 0x50, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x00, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        long long int mCityId = getCityId();
        l_makernote[46] = getWeatherId();
        memcpy(l_makernote + 90, &mCityId, 8);
        exifInfo->maker_note_size = 98;
        memcpy(exifInfo->maker_note, l_makernote, sizeof(l_makernote));
    } else {
        exifInfo->maker_note_size = 0;
    }
#else
    exifInfo->maker_note_size = 0;
#endif

    /* Date Time */
    struct timeval rawtime;
    struct tm timeinfo;
    gettimeofday(&rawtime, NULL);
    localtime_r((time_t *)&rawtime.tv_sec, &timeinfo);
    strftime((char *)exifInfo->date_time, 20, "%Y:%m:%d %H:%M:%S", &timeinfo);
    snprintf((char *)exifInfo->sec_time, 5, "%04d", (int)(rawtime.tv_usec/1000));

    /* Exif Private Tags */
    bool flagSLSIAlgorithm = true;
    /*
     * vendorSpecific2[100]      : exposure
     * vendorSpecific2[101]      : iso(gain)
     * vendorSpecific2[102] /256 : Bv
     * vendorSpecific2[103]      : Tv
     */

    /* ISO Speed Rating */
#if 0 /* TODO: Must be same with the sensitivity in Result Metadata */
    exifInfo->iso_speed_rating = shot->udm.internal.vendorSpecific2[101];
#else
    exifInfo->iso_speed_rating = shot->dm.sensor.sensitivity;
#endif

    /* Exposure Program */
    if (m_exposureTimeCapture == 0) {
        m_exifInfo.exposure_program = EXIF_DEF_EXPOSURE_PROGRAM;
    } else {
        m_exifInfo.exposure_program = EXIF_DEF_EXPOSURE_MANUAL;
    }

    /* Exposure Time */
    exifInfo->exposure_time.num = 1;
#if 0 /* TODO: Must be same with the exposure time in Result Metadata */
    if (shot->udm.ae.vendorSpecific[0] == 0xAEAEAEAE) {
        exifInfo->exposure_time.den = (uint32_t) shot->udm.ae.vendorSpecific[64];
    } else
#endif
    {
        /* HACK : Sometimes, F/W does NOT send the exposureTime */
        if (shot->dm.sensor.exposureTime != 0)
            exifInfo->exposure_time.den = (uint32_t) 1e9 / shot->dm.sensor.exposureTime;
        else
            exifInfo->exposure_time.num = 0;
    }

    /* Shutter Speed */
    exifInfo->shutter_speed.num = (uint32_t) (ROUND_OFF_HALF(((double) (shot->udm.internal.vendorSpecific2[103] / 256.f) * EXIF_DEF_APEX_DEN), 0));
    exifInfo->shutter_speed.den = EXIF_DEF_APEX_DEN;

    /* Aperture */
    exifInfo->aperture.num = APEX_FNUM_TO_APERTURE((double) (exifInfo->fnumber.num) / (double) (exifInfo->fnumber.den)) * COMMON_DENOMINATOR;
    exifInfo->aperture.den = COMMON_DENOMINATOR;

    /* Max Aperture */
    exifInfo->max_aperture.num = APEX_FNUM_TO_APERTURE((double) (exifInfo->fnumber.num) / (double) (exifInfo->fnumber.den)) * COMMON_DENOMINATOR;
    exifInfo->max_aperture.den = COMMON_DENOMINATOR;

    /* Brightness */
    int temp = shot->udm.internal.vendorSpecific2[102];
    if ((int) shot->udm.ae.vendorSpecific[102] < 0)
        temp = -temp;
    exifInfo->brightness.num = (int32_t) (ROUND_OFF_HALF((double)((temp * EXIF_DEF_APEX_DEN)/256.f), 0));
    if ((int) shot->udm.ae.vendorSpecific[102] < 0)
        exifInfo->brightness.num = -exifInfo->brightness.num;
    exifInfo->brightness.den = EXIF_DEF_APEX_DEN;

    ALOGD("DEBUG(%s):udm->internal.vendorSpecific2[100](%d)", __FUNCTION__, shot->udm.internal.vendorSpecific2[100]);
    ALOGD("DEBUG(%s):udm->internal.vendorSpecific2[101](%d)", __FUNCTION__, shot->udm.internal.vendorSpecific2[101]);
    ALOGD("DEBUG(%s):udm->internal.vendorSpecific2[102](%d)", __FUNCTION__, shot->udm.internal.vendorSpecific2[102]);
    ALOGD("DEBUG(%s):udm->internal.vendorSpecific2[103](%d)", __FUNCTION__, shot->udm.internal.vendorSpecific2[103]);

    ALOGD("DEBUG(%s):iso_speed_rating(%d)", __FUNCTION__, exifInfo->iso_speed_rating);
    ALOGD("DEBUG(%s):exposure_time(%d/%d)", __FUNCTION__, exifInfo->exposure_time.num, exifInfo->exposure_time.den);
    ALOGD("DEBUG(%s):shutter_speed(%d/%d)", __FUNCTION__, exifInfo->shutter_speed.num, exifInfo->shutter_speed.den);
    ALOGD("DEBUG(%s):aperture     (%d/%d)", __FUNCTION__, exifInfo->aperture.num, exifInfo->aperture.den);
    ALOGD("DEBUG(%s):brightness   (%d/%d)", __FUNCTION__, exifInfo->brightness.num, exifInfo->brightness.den);

    /* Exposure Bias */
#if defined(USE_SUBDIVIDED_EV)
    exifInfo->exposure_bias.num = shot->ctl.aa.aeExpCompensation * (m_staticInfo->exposureCompensationStep * 10);
#else
    exifInfo->exposure_bias.num =
        (shot->ctl.aa.aeExpCompensation) * (m_staticInfo->exposureCompensationStep * 10);
#endif
    exifInfo->exposure_bias.den = 10;

    /* Metering Mode */
#ifdef SAMSUNG_COMPANION
    enum companion_wdr_mode wdr_mode;
    wdr_mode = shot->uctl.companionUd.wdr_mode;
    if (wdr_mode == COMPANION_WDR_ON) {
        exifInfo->metering_mode = EXIF_METERING_PATTERN;
    } else
#endif
    {
        switch (shot->ctl.aa.aeMode) {
        case AA_AEMODE_CENTER:
            exifInfo->metering_mode = EXIF_METERING_CENTER;
            break;
        case AA_AEMODE_MATRIX:
            exifInfo->metering_mode = EXIF_METERING_AVERAGE;
            break;
        case AA_AEMODE_SPOT:
            exifInfo->metering_mode = EXIF_METERING_SPOT;
            break;
        default:
            exifInfo->metering_mode = EXIF_METERING_AVERAGE;
            break;
        }

#ifdef SAMSUNG_FOOD_MODE
        if(getSceneMode() == SCENE_MODE_FOOD) {
            exifInfo->metering_mode = EXIF_METERING_AVERAGE;
        }
#endif
    }

    /* Flash Mode */
    if (shot->ctl.flash.flashMode == CAM2_FLASH_MODE_TORCH) {
        exifInfo->flash = 1;
    } else {
        exifInfo->flash = getMarkingOfExifFlash();
    }

    /* White Balance */
    if (shot->ctl.aa.awbMode == AA_AWBMODE_WB_AUTO)
        exifInfo->white_balance = EXIF_WB_AUTO;
    else
        exifInfo->white_balance = EXIF_WB_MANUAL;

    /* Focal Length in 35mm length */
    exifInfo->focal_length_in_35mm_length = getFocalLengthIn35mmFilm();

    /* Scene Capture Type */
    switch (shot->ctl.aa.sceneMode) {
    case AA_SCENE_MODE_PORTRAIT:
    case AA_SCENE_MODE_FACE_PRIORITY:
        exifInfo->scene_capture_type = EXIF_SCENE_PORTRAIT;
        break;
    case AA_SCENE_MODE_LANDSCAPE:
        exifInfo->scene_capture_type = EXIF_SCENE_LANDSCAPE;
        break;
    case AA_SCENE_MODE_NIGHT:
        exifInfo->scene_capture_type = EXIF_SCENE_NIGHT;
        break;
    default:
        exifInfo->scene_capture_type = EXIF_SCENE_STANDARD;
        break;
    }

    switch (this->getShotMode()) {
    case SHOT_MODE_BEAUTY_FACE:
    case SHOT_MODE_BEST_FACE:
        exifInfo->scene_capture_type = EXIF_SCENE_PORTRAIT;
        break;
    default:
        break;
    }

    /* Image Unique ID */
#if defined(SAMSUNG_TN_FEATURE) && defined(SENSOR_FW_GET_FROM_FILE)
    char *front_fw = NULL;
    char *savePtr;

    if (getCameraId() == CAMERA_ID_BACK){
        memset(exifInfo->unique_id, 0, sizeof(exifInfo->unique_id));
        strncpy((char *)exifInfo->unique_id,
		getSensorFWFromFile(m_staticInfo, m_cameraId), sizeof(exifInfo->unique_id) - 1);
    } else if (getCameraId() == CAMERA_ID_FRONT) {
        front_fw = strtok_r((char *)getSensorFWFromFile(m_staticInfo, m_cameraId), " ", &savePtr);
        strcpy((char *)exifInfo->unique_id, front_fw);
    }
#endif

    /* GPS Coordinates */
    double gpsLatitude = shot->ctl.jpeg.gpsCoordinates[0];
    double gpsLongitude = shot->ctl.jpeg.gpsCoordinates[1];
    double gpsAltitude = shot->ctl.jpeg.gpsCoordinates[2];
    if (gpsLatitude != 0 && gpsLongitude != 0) {
        if (gpsLatitude > 0)
            strncpy((char *) exifInfo->gps_latitude_ref, "N", 2);
        else
            strncpy((char *) exifInfo->gps_latitude_ref, "S", 2);

        if (gpsLongitude > 0)
            strncpy((char *) exifInfo->gps_longitude_ref, "E", 2);
        else
            strncpy((char *) exifInfo->gps_longitude_ref, "W", 2);

        if (gpsAltitude > 0)
            exifInfo->gps_altitude_ref = 0;
        else
            exifInfo->gps_altitude_ref = 1;

        gpsLatitude = fabs(gpsLatitude);
        gpsLongitude = fabs(gpsLongitude);
        gpsAltitude = fabs(gpsAltitude);

        exifInfo->gps_latitude[0].num = (uint32_t) gpsLatitude;
        exifInfo->gps_latitude[0].den = 1;
        exifInfo->gps_latitude[1].num = (uint32_t)((gpsLatitude - exifInfo->gps_latitude[0].num) * 60);
        exifInfo->gps_latitude[1].den = 1;
        exifInfo->gps_latitude[2].num = (uint32_t)(round((((gpsLatitude - exifInfo->gps_latitude[0].num) * 60)
                        - exifInfo->gps_latitude[1].num) * 60));
        exifInfo->gps_latitude[2].den = 1;

        exifInfo->gps_longitude[0].num = (uint32_t)gpsLongitude;
        exifInfo->gps_longitude[0].den = 1;
        exifInfo->gps_longitude[1].num = (uint32_t)((gpsLongitude - exifInfo->gps_longitude[0].num) * 60);
        exifInfo->gps_longitude[1].den = 1;
        exifInfo->gps_longitude[2].num = (uint32_t)(round((((gpsLongitude - exifInfo->gps_longitude[0].num) * 60)
                        - exifInfo->gps_longitude[1].num) * 60));
        exifInfo->gps_longitude[2].den = 1;

        exifInfo->gps_altitude.num = (uint32_t)gpsAltitude;
        exifInfo->gps_altitude.den = 1;

        struct tm tm_data;
        long gpsTimestamp = (long) shot->ctl.jpeg.gpsTimestamp;
        gmtime_r(&gpsTimestamp, &tm_data);
        exifInfo->gps_timestamp[0].num = tm_data.tm_hour;
        exifInfo->gps_timestamp[0].den = 1;
        exifInfo->gps_timestamp[1].num = tm_data.tm_min;
        exifInfo->gps_timestamp[1].den = 1;
        exifInfo->gps_timestamp[2].num = tm_data.tm_sec;
        exifInfo->gps_timestamp[2].den = 1;
        snprintf((char*)exifInfo->gps_datestamp, sizeof(exifInfo->gps_datestamp),
                "%04d:%02d:%02d", tm_data.tm_year + 1900, tm_data.tm_mon + 1, tm_data.tm_mday);

        exifInfo->enableGps = true;
    } else {
        exifInfo->enableGps = false;
    }

    /* Thumbnail Size */
    exifInfo->widthThumb = thumbnailRect->w;
    exifInfo->heightThumb = thumbnailRect->h;

    setMarkingOfExifFlash(0);
}

debug_attribute_t *ExynosCameraParameters::getDebugAttribute(void)
{
    return &mDebugInfo;
}

status_t ExynosCameraParameters::getFixedExifInfo(exif_attribute_t *exifInfo)
{
    if (exifInfo == NULL) {
        CLOGE("ERR(%s[%d]): buffer is NULL", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    memcpy(exifInfo, &m_exifInfo, sizeof(exif_attribute_t));

    return NO_ERROR;
}

#ifdef SAMSUNG_DNG
status_t ExynosCameraParameters::checkDngFilePath(const CameraParameters& params)
{
    const char *dngFilePath = params.get("capture-raw-filepath");

    if (dngFilePath != NULL) {
        snprintf(m_dngFilePath, sizeof(m_dngFilePath), "%s", dngFilePath);
        CLOGD("DEBUG(%s): dngFilePath %s", "setParameters", dngFilePath);
    } else {
        CLOGD("DEBUG(%s): dngFilePath NULL", "setParameters");
        memset(m_dngFilePath, 0, CAMERA_FILE_PATH_SIZE);
    }

    return NO_ERROR;
}

char *ExynosCameraParameters::getDngFilePath(void)
{
    return m_dngFilePath;
}

int ExynosCameraParameters::getDngSaveLocation(void)
{
    int dngShotSaveLocation = m_dngSaveLocation;

    /* GED's series shot work as callback */
#ifdef CAMERA_GED_FEATURE
    dngShotSaveLocation = BURST_SAVE_CALLBACK;
#else
    if (m_dngSaveLocation == 0)
        dngShotSaveLocation = BURST_SAVE_PHONE;
    else
        dngShotSaveLocation = BURST_SAVE_SDCARD;
#endif

    return dngShotSaveLocation;
}

void ExynosCameraParameters::setDngSaveLocation(int saveLocation)
{
    m_dngSaveLocation = saveLocation;
}

void ExynosCameraParameters::m_setDngFixedAttribute(void)
{
    char property[PROPERTY_VALUE_MAX];

    memset(&m_dngInfo, 0, sizeof(m_dngInfo));

    /* IFD TIFF Tags */
    /* string Tags */
    strncpy((char *)m_dngInfo.make, TIFF_DEF_MAKE, sizeof(m_dngInfo.make) - 1);
    m_dngInfo.make[sizeof(TIFF_DEF_MAKE) - 1] = '\0';
    property_get("ro.product.model", property, TIFF_DEF_MODEL);
    strncpy((char *)m_dngInfo.model, property, sizeof(m_dngInfo.model) - 1);
    m_dngInfo.model[sizeof(m_dngInfo.model) - 1] = '\0';
    strncpy((char *)m_dngInfo.unique_camera_model, property, sizeof(m_dngInfo.unique_camera_model) - 1);
    m_dngInfo.unique_camera_model[sizeof(m_dngInfo.unique_camera_model) - 1] = '\0';
    property_get("ro.build.PDA", property, TIFF_DEF_SOFTWARE);
    strncpy((char *)m_dngInfo.software, property, sizeof(m_dngInfo.software) - 1);
    m_dngInfo.software[sizeof(m_dngInfo.software) - 1] = '\0';

    /* pre-defined Tasgs at the header */
    m_dngInfo.new_subfile_type = 0;
    m_dngInfo.bits_per_sample = TIFF_DEF_BITS_PER_SAMPLE;
    m_dngInfo.rows_per_strip = TIFF_DEF_ROWS_PER_STRIP;
    m_dngInfo.photometric_interpretation = TIFF_DEF_PHOTOMETRIC_INTERPRETATION;
    m_dngInfo.image_description[0] = '\0';
    m_dngInfo.planar_configuration = TIFF_DEF_PLANAR_CONFIGURATION;
    m_dngInfo.copyright = '\0';
    m_dngInfo.samples_per_pixel = TIFF_DEF_SAMPLES_PER_PIXEL;
    m_dngInfo.compression = TIFF_DEF_COMPRESSION;
    m_dngInfo.x_resolution.num = TIFF_DEF_RESOLUTION_NUM;
    m_dngInfo.x_resolution.den = TIFF_DEF_RESOLUTION_DEN;
    m_dngInfo.y_resolution.num = TIFF_DEF_RESOLUTION_NUM;
    m_dngInfo.y_resolution.den = TIFF_DEF_RESOLUTION_DEN;
    m_dngInfo.resolution_unit = TIFF_DEF_RESOLUTION_UNIT;
    m_dngInfo.cfa_layout = TIFF_DEF_CFA_LAYOUT;
    memcpy(m_dngInfo.tiff_ep_standard_id, TIFF_DEF_TIFF_EP_STANDARD_ID, sizeof(m_dngInfo.tiff_ep_standard_id));
    memcpy(m_dngInfo.cfa_repeat_pattern_dm, TIFF_DEF_CFA_REPEAT_PATTERN_DM, sizeof(m_dngInfo.cfa_repeat_pattern_dm));
    memcpy(m_dngInfo.cfa_pattern, TIFF_DEF_CFA_PATTERN, sizeof(m_dngInfo.cfa_pattern));
    memcpy(m_dngInfo.dng_version, TIFF_DEF_DNG_VERSION, sizeof(m_dngInfo.dng_version));
    memcpy(m_dngInfo.dng_backward_version, TIFF_DEF_DNG_BACKWARD_VERSION, sizeof(m_dngInfo.dng_backward_version));
    memcpy(m_dngInfo.cfa_plane_color, TIFF_DEF_CFA_PLANE_COLOR, sizeof(m_dngInfo.cfa_plane_color));
    memcpy(m_dngInfo.black_level_repeat_dim, TIFF_DEF_BLACK_LEVEL_REPEAT_DIM, sizeof(m_dngInfo.black_level_repeat_dim));
    memcpy(m_dngInfo.default_scale, TIFF_DEF_DEFAULT_SCALE, sizeof(m_dngInfo.default_scale));
    memcpy(m_dngInfo.default_crop_origin, TIFF_DEF_DEFAULT_CROP_ORIGIN, sizeof(m_dngInfo.default_crop_origin));
    memcpy(m_dngInfo.opcode_list2, &TIFF_DEF_OPCODE_LIST2, sizeof(m_dngInfo.opcode_list2));
    memcpy(m_dngInfo.exif_version, &TIFF_DEF_EXIF_VERSION, sizeof(m_dngInfo.exif_version));

    /* static Metadata */
    m_dngInfo.f_number.num = m_staticInfo->fNumberNum;
    m_dngInfo.f_number.den = m_staticInfo->fNumberDen;
    m_dngInfo.focal_length.num = m_staticInfo->focalLengthNum;
    m_dngInfo.focal_length.den = m_staticInfo->focalLengthDen;
    m_dngInfo.white_level = m_staticInfo->whiteLevel;
    m_dngInfo.calibration_illuminant1 = m_staticInfo->referenceIlluminant1;
    m_dngInfo.calibration_illuminant2 = m_staticInfo->referenceIlluminant2;
    memcpy(m_dngInfo.black_level_repeat, m_staticInfo->blackLevelPattern, sizeof(m_dngInfo.black_level_repeat));
    memcpy(m_dngInfo.color_matrix1, m_staticInfo->colorTransformMatrix1, sizeof(m_dngInfo.color_matrix1));
    memcpy(m_dngInfo.color_matrix2, m_staticInfo->colorTransformMatrix2, sizeof(m_dngInfo.color_matrix2));
    memcpy(m_dngInfo.forward_matrix1, m_staticInfo->forwardMatrix1, sizeof(m_dngInfo.forward_matrix1));
    memcpy(m_dngInfo.forward_matrix2, m_staticInfo->forwardMatrix2, sizeof(m_dngInfo.forward_matrix2));
    memcpy(m_dngInfo.camera_calibration1, m_staticInfo->calibration1, sizeof(m_dngInfo.camera_calibration1));
    memcpy(m_dngInfo.camera_calibration2, m_staticInfo->calibration2, sizeof(m_dngInfo.camera_calibration2));
}

void ExynosCameraParameters::setDngChangedAttribute(struct camera2_dm *dm, struct camera2_udm *udm)
{
    CLOGD("DEBUG(%s[%d]): set Dynamic Dng Info", __FUNCTION__, __LINE__);

    /* IFD TIFF Tags */
    getHwSensorSize((int *)&m_dngInfo.image_width, (int *)&m_dngInfo.image_length);
    m_dngInfo.default_crop_size[0] = m_dngInfo.image_width - m_dngInfo.default_crop_origin[0];
    m_dngInfo.default_crop_size[1] = m_dngInfo.image_length - m_dngInfo.default_crop_origin[1];

    /* Orientation */
    switch (m_cameraInfo.rotation) {
    case 90:
        m_dngInfo.orientation = EXIF_ORIENTATION_90;
        break;
    case 180:
        m_dngInfo.orientation = EXIF_ORIENTATION_180;
        break;
    case 270:
        m_dngInfo.orientation = EXIF_ORIENTATION_270;
        break;
    case 0:
    default:
        m_dngInfo.orientation = EXIF_ORIENTATION_UP;
        break;
    }

    /* Date time */
    struct timeval rawtime;
    struct tm timeinfo;
    gettimeofday(&rawtime, NULL);
    localtime_r((time_t *)&rawtime.tv_sec, &timeinfo);
    strftime((char *)m_dngInfo.date_time, 20, "%Y:%m:%d %H:%M:%S", &timeinfo);
    strftime((char *)m_dngInfo.date_time_original, 20, "%Y:%m:%d %H:%M:%S", &timeinfo);

    /* ISO Speed Rating */
    m_dngInfo.iso_speed_ratings = udm->internal.vendorSpecific2[101];

    /* Exposure Time */
    if (m_exposureTimeCapture == 0) {
        m_dngInfo.exposure_time.num = 1;

        if (udm->ae.vendorSpecific[0] == 0xAEAEAEAE)
            m_dngInfo.exposure_time.den = (uint32_t)udm->ae.vendorSpecific[64];
        else
            m_dngInfo.exposure_time.den = (uint32_t)udm->internal.vendorSpecific2[100];
    } else if (m_exposureTimeCapture > 0 && m_exposureTimeCapture <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        m_dngInfo.exposure_time.num = 1;
        if ((1000000 % m_exposureTimeCapture) != 0) {
            uint32_t dig;
            if (m_exposureTimeCapture < 100) {
                dig = 1000;
            } else if (m_exposureTimeCapture < 1000) {
                dig = 100;
            } else if (m_exposureTimeCapture < 10000) {
                dig = 10;
            } else {
                dig = 5;
            }
            m_dngInfo.exposure_time.den = ROUND_OFF_DIGIT((int)(1000000 / m_exposureTimeCapture), dig);
        } else {
            m_dngInfo.exposure_time.den = (int)(1000000 / m_exposureTimeCapture);
        }
    } else {
        m_dngInfo.exposure_time.num = m_exposureTimeCapture / 1000;
        m_dngInfo.exposure_time.den = 1000;
    }

    /* AsShotNeutral & NoiseProfile */
    memcpy(m_dngInfo.as_shot_neutral, dm->sensor.neutralColorPoint, sizeof(m_dngInfo.as_shot_neutral));
    memcpy(m_dngInfo.noise_profile, dm->sensor.noiseProfile, sizeof(m_dngInfo.noise_profile));

    /* 3 Image Unique ID */
#if defined(SAMSUNG_TN_FEATURE) && defined(SENSOR_FW_GET_FROM_FILE)
    char *front_fw = NULL;
    char *savePtr;

    if (getCameraId() == CAMERA_ID_BACK){
        memset(m_dngInfo.unique_camera_model, 0, sizeof(m_dngInfo.unique_camera_model));
        strncpy((char *)m_dngInfo.unique_camera_model, getSensorFWFromFile(m_staticInfo, m_cameraId),
            sizeof(m_dngInfo.unique_camera_model) - 1);
    } else if (getCameraId() == CAMERA_ID_FRONT) {
        front_fw = strtok_r((char *)getSensorFWFromFile(m_staticInfo, m_cameraId), " ", &savePtr);
        strcpy((char *)m_dngInfo.unique_camera_model, front_fw);
    }
#endif
}

dng_attribute_t* ExynosCameraParameters::getDngInfo()
{
    int retryCount = 12; /* 30ms * 12 */
    while(retryCount > 0) {
        if(getIsUsefulDngInfo() == false) {
            CLOGD("DEBUG(%s[%d]): Waiting for update DNG metadata failed, retryCount(%d)", __FUNCTION__, __LINE__, retryCount);
        } else {
            CLOGD("DEBUG(%s[%d]): Success DNG meta, retryCount(%d)", __FUNCTION__, __LINE__, retryCount);
            break;
        }
        retryCount--;
        usleep(DM_WAITING_TIME);
    }

    return &m_dngInfo;
}

void ExynosCameraParameters::setIsUsefulDngInfo(bool enable)
{
    m_isUsefulDngInfo = enable;
}

bool ExynosCameraParameters::getIsUsefulDngInfo()
{
    return m_isUsefulDngInfo;
}
#endif

status_t ExynosCameraParameters::checkGpsTimeStamp(const CameraParameters& params)
{
    long newGpsTimeStamp = -1;
    long curGpsTimeStamp = -1;
    const char *strNewGpsTimeStamp = params.get(CameraParameters::KEY_GPS_TIMESTAMP);

    if (strNewGpsTimeStamp == NULL) {
        m_params.remove(CameraParameters::KEY_GPS_TIMESTAMP);
        m_setGpsTimeStamp(0);
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):strNewGpsTimeStamp %s", "setParameters", strNewGpsTimeStamp);

    newGpsTimeStamp = atol(strNewGpsTimeStamp);

    curGpsTimeStamp = getGpsTimeStamp();

    if (curGpsTimeStamp != newGpsTimeStamp) {
        m_setGpsTimeStamp(newGpsTimeStamp);
        m_params.set(CameraParameters::KEY_GPS_TIMESTAMP, strNewGpsTimeStamp);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setGpsTimeStamp(long timeStamp)
{
    m_cameraInfo.gpsTimeStamp = timeStamp;
}

long ExynosCameraParameters::getGpsTimeStamp(void)
{
    return m_cameraInfo.gpsTimeStamp;
}

/* TODO: Do not used yet */
#if 0
status_t ExynosCameraParameters::checkCityId(const CameraParameters& params)
{
    long long int newCityId = params.getInt64(CameraParameters::KEY_CITYID);
    long long int curCityId = -1;

    if (newCityId < 0)
        newCityId = 0;

    curCityId = getCityId();

    if (curCityId != newCityId) {
        m_setCityId(newCityId);
        m_params.set(CameraParameters::KEY_CITYID, newCityId);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setCityId(long long int cityId)
{
    m_cameraInfo.cityId = cityId;
}

long long int ExynosCameraParameters::getCityId(void)
{
    return m_cameraInfo.cityId;
}

status_t ExynosCameraParameters::checkWeatherId(const CameraParameters& params)
{
    int newWeatherId = params.getInt(CameraParameters::KEY_WEATHER);
    int curWeatherId = -1;

    if (newWeatherId < 0 || newWeatherId > 5) {
        return BAD_VALUE;
    }

    curWeatherId = (int)getWeatherId();

    if (curWeatherId != newWeatherId) {
        m_setWeatherId((unsigned char)newWeatherId);
        m_params.set(CameraParameters::KEY_WEATHER, newWeatherId);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setWeatherId(unsigned char weatherId)
{
    m_cameraInfo.weatherId = weatherId;
}

unsigned char ExynosCameraParameters::getWeatherId(void)
{
    return m_cameraInfo.weatherId;
}
#endif

/*
 * Additional API.
 */

status_t ExynosCameraParameters::checkBrightness(const CameraParameters& params)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return NO_ERROR;
    } else {
        int maxBrightness = params.getInt("brightness-max");
        int minBrightness = params.getInt("brightness-min");
        int newBrightness = params.getInt("brightness");
        int curBrightness = -1;
        int curEffectMode = EFFECT_NONE;

        CLOGD("DEBUG(%s):newBrightness %d", "setParameters", newBrightness);

        if (newBrightness < minBrightness || newBrightness > maxBrightness) {
            CLOGE("ERR(%s): Invalid Brightness (Min: %d, Max: %d, Value: %d)", __FUNCTION__, minBrightness, maxBrightness, newBrightness);
            return BAD_VALUE;
        }

        curEffectMode = getColorEffectMode();
        if(curEffectMode == EFFECT_BEAUTY_FACE) {
            return NO_ERROR;
        }

        curBrightness = getBrightness();

        if (curBrightness != newBrightness) {
            m_setBrightness(newBrightness);
            m_params.set("brightness", newBrightness);
        }
        return NO_ERROR;
    }
}

/* F/W's middle value is 3, and step is -2, -1, 0, 1, 2 */
void ExynosCameraParameters::m_setBrightness(int brightness)
{
    setMetaCtlBrightness(&m_metadata, brightness + IS_BRIGHTNESS_DEFAULT + FW_CUSTOM_OFFSET);
}

int ExynosCameraParameters::getBrightness(void)
{
    int32_t brightness = 0;

    getMetaCtlBrightness(&m_metadata, &brightness);
    return brightness - IS_BRIGHTNESS_DEFAULT - FW_CUSTOM_OFFSET;
}

status_t ExynosCameraParameters::checkSaturation(const CameraParameters& params)
{
    int maxSaturation = params.getInt("saturation-max");
    int minSaturation = params.getInt("saturation-min");
    int newSaturation = params.getInt("saturation");
    int curSaturation = -1;

    CLOGD("DEBUG(%s):newSaturation %d", "setParameters", newSaturation);

    if (newSaturation < minSaturation || newSaturation > maxSaturation) {
        CLOGE("ERR(%s): Invalid Saturation (Min: %d, Max: %d, Value: %d)", __FUNCTION__, minSaturation, maxSaturation, newSaturation);
        return BAD_VALUE;
    }

    curSaturation = getSaturation();

    if (curSaturation != newSaturation) {
        m_setSaturation(newSaturation);
        m_params.set("saturation", newSaturation);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setSaturation(int saturation)
{
    setMetaCtlSaturation(&m_metadata, saturation + IS_SATURATION_DEFAULT + FW_CUSTOM_OFFSET);
}

int ExynosCameraParameters::getSaturation(void)
{
    int32_t saturation = 0;

    getMetaCtlSaturation(&m_metadata, &saturation);
    return saturation - IS_SATURATION_DEFAULT - FW_CUSTOM_OFFSET;
}

status_t ExynosCameraParameters::checkSharpness(const CameraParameters& params)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return NO_ERROR;
    } else {
        int maxSharpness = params.getInt("sharpness-max");
        int minSharpness = params.getInt("sharpness-min");
        int newSharpness = params.getInt("sharpness");
        int curSharpness = -1;
        int curEffectMode = EFFECT_NONE;

        CLOGD("DEBUG(%s):newSharpness %d", "setParameters", newSharpness);

        if (newSharpness < minSharpness || newSharpness > maxSharpness) {
            CLOGE("ERR(%s): Invalid Sharpness (Min: %d, Max: %d, Value: %d)", __FUNCTION__, minSharpness, maxSharpness, newSharpness);
            return BAD_VALUE;
        }

        curEffectMode = getColorEffectMode();
        if(curEffectMode == EFFECT_BEAUTY_FACE
#if 0
//#ifdef SAMSUNG_FOOD_MODE
            || (getSceneMode() == SCENE_MODE_FOOD)
#endif
            ) {
            return NO_ERROR;
        }

        curSharpness = getSharpness();

        if (curSharpness != newSharpness) {
            m_setSharpness(newSharpness);
            m_params.set("sharpness", newSharpness);
        }
        return NO_ERROR;
    }
}

void ExynosCameraParameters::m_setSharpness(int sharpness)
{
#ifdef USE_NEW_NOISE_REDUCTION_ALGORITHM
    enum processing_mode default_edge_mode = PROCESSING_MODE_FAST;
    enum processing_mode default_noise_mode = PROCESSING_MODE_FAST;
    int default_edge_strength = 5;
    int default_noise_strength = 5;
#else
    enum processing_mode default_edge_mode = PROCESSING_MODE_OFF;
    enum processing_mode default_noise_mode = PROCESSING_MODE_OFF;
    int default_edge_strength = 0;
    int default_noise_strength = 0;
#endif

    int newSharpness = sharpness + IS_SHARPNESS_DEFAULT;
    enum processing_mode edge_mode = default_edge_mode;
    enum processing_mode noise_mode = default_noise_mode;
    int edge_strength = default_edge_strength;
    int noise_strength = default_noise_strength;

    switch (newSharpness) {
    case IS_SHARPNESS_MINUS_2:
        edge_mode = default_edge_mode;
        noise_mode = default_noise_mode;
        edge_strength = default_edge_strength;
        noise_strength = 10;
        break;
    case IS_SHARPNESS_MINUS_1:
        edge_mode = default_edge_mode;
        noise_mode = default_noise_mode;
        edge_strength = default_edge_strength;
        noise_strength = (10 + default_noise_strength + 1) / 2;
        break;
    case IS_SHARPNESS_DEFAULT:
        edge_mode = default_edge_mode;
        noise_mode = default_noise_mode;
        edge_strength = default_edge_strength;
        noise_strength = default_noise_strength;
        break;
    case IS_SHARPNESS_PLUS_1:
        edge_mode = default_edge_mode;
        noise_mode = default_noise_mode;
        edge_strength = (10 + default_edge_strength + 1) / 2;
        noise_strength = default_noise_strength;
        break;
    case IS_SHARPNESS_PLUS_2:
        edge_mode = default_edge_mode;
        noise_mode = default_noise_mode;
        edge_strength = 10;
        noise_strength = default_noise_strength;
        break;
    default:
        break;
    }

    CLOGD("DEBUG(%s):newSharpness %d edge_mode(%d),st(%d), noise(%d),st(%d)",
         __FUNCTION__, newSharpness, edge_mode, edge_strength, noise_mode, noise_strength);

    setMetaCtlSharpness(&m_metadata, edge_mode, edge_strength, noise_mode, noise_strength);
}

int ExynosCameraParameters::getSharpness(void)
{
#ifdef USE_NEW_NOISE_REDUCTION_ALGORITHM
    enum processing_mode default_edge_mode = PROCESSING_MODE_FAST;
    enum processing_mode default_noise_mode = PROCESSING_MODE_FAST;
    int default_edge_sharpness = 5;
    int default_noise_sharpness = 5;
#else
    enum processing_mode default_edge_mode = PROCESSING_MODE_OFF;
    enum processing_mode default_noise_mode = PROCESSING_MODE_OFF;
    int default_edge_sharpness = 0;
    int default_noise_sharpness = 0;
#endif

    int32_t edge_sharpness = default_edge_sharpness;
    int32_t noise_sharpness = default_noise_sharpness;
    int32_t sharpness = 0;
    enum processing_mode edge_mode = default_edge_mode;
    enum processing_mode noise_mode = default_noise_mode;

    getMetaCtlSharpness(&m_metadata, &edge_mode, &edge_sharpness, &noise_mode, &noise_sharpness);

    if(noise_sharpness == 10 && edge_sharpness == default_edge_sharpness) {
        sharpness = IS_SHARPNESS_MINUS_2;
    } else if(noise_sharpness == (10 + default_noise_sharpness + 1) / 2
            && edge_sharpness == default_edge_sharpness) {
        sharpness = IS_SHARPNESS_MINUS_1;
    } else if(noise_sharpness == default_noise_sharpness
            && edge_sharpness == default_edge_sharpness) {
        sharpness = IS_SHARPNESS_DEFAULT;
    } else if(noise_sharpness == default_noise_sharpness
            && edge_sharpness == (10 + default_edge_sharpness + 1) / 2) {
        sharpness = IS_SHARPNESS_PLUS_1;
    } else if(noise_sharpness == default_noise_sharpness
            && edge_sharpness == 10) {
        sharpness = IS_SHARPNESS_PLUS_2;
    } else {
        sharpness = IS_SHARPNESS_DEFAULT;
    }

    return sharpness - IS_SHARPNESS_DEFAULT;
}

status_t ExynosCameraParameters::checkHue(const CameraParameters& params)
{
    int maxHue = params.getInt("hue-max");
    int minHue = params.getInt("hue-min");
    int newHue = params.getInt("hue");
    int curHue = -1;

    CLOGD("DEBUG(%s):newHue %d", "setParameters", newHue);

    if (newHue < minHue || newHue > maxHue) {
        CLOGE("ERR(%s): Invalid Hue (Min: %d, Max: %d, Value: %d)", __FUNCTION__, minHue, maxHue, newHue);
        return BAD_VALUE;
    }

    curHue = getHue();

    if (curHue != newHue) {
        m_setHue(newHue);
        m_params.set("hue", newHue);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setHue(int hue)
{
    setMetaCtlHue(&m_metadata, hue + IS_HUE_DEFAULT + FW_CUSTOM_OFFSET);
}

int ExynosCameraParameters::getHue(void)
{
    int32_t hue = 0;

    getMetaCtlHue(&m_metadata, &hue);
    return hue - IS_HUE_DEFAULT - FW_CUSTOM_OFFSET;
}

#ifdef SAMSUNG_QUICKSHOT
status_t ExynosCameraParameters::checkQuickShot(const CameraParameters& params)
{
    int newQshot = params.getInt("quick-shot");
    int curQshot= -1;

    CLOGD("DEBUG(%s):newQshot %d", "setParameters", newQshot);

    if (newQshot < 0) {
        CLOGE("ERR(%s): Invalid quick shot mode", __FUNCTION__, newQshot);
        return BAD_VALUE;
    }

    curQshot = getQuickShot();

    if (curQshot != newQshot) {
        m_params.set("quick-shot", newQshot);
    }

    return NO_ERROR;
}

int ExynosCameraParameters::getQuickShot(void)
{
    int32_t qshot = 0;

    qshot = m_params.getInt("quick-shot");
    return qshot;
}
#endif

#ifdef SAMSUNG_OIS
status_t ExynosCameraParameters::checkOIS(const CameraParameters& params)
{
    enum optical_stabilization_mode newOIS = OPTICAL_STABILIZATION_MODE_OFF;
    enum optical_stabilization_mode curOIS = OPTICAL_STABILIZATION_MODE_OFF;
    const char *strNewOIS = params.get("ois");
    int zoomLevel = getZoomLevel();
    int zoomRatio = (int)getZoomRatio(zoomLevel);

    if (strNewOIS == NULL) {
        return NO_ERROR;
    }

    ALOGD("DEBUG(%s):strNewOIS %s", "setParameters", strNewOIS);

    if (!strcmp(strNewOIS, "off")) {
        newOIS = OPTICAL_STABILIZATION_MODE_OFF;
    } else if (!strcmp(strNewOIS, "still")) {
        if (getRecordingHint() == true) {
            newOIS = OPTICAL_STABILIZATION_MODE_VIDEO;
        } else {
            if(zoomRatio >= 4000) {
                newOIS = OPTICAL_STABILIZATION_MODE_STILL_ZOOM;
            } else {
                newOIS = OPTICAL_STABILIZATION_MODE_STILL;
            }
        }
    } else if (!strcmp(strNewOIS, "still_zoom")) {
        newOIS = OPTICAL_STABILIZATION_MODE_STILL_ZOOM;
    } else if (!strcmp(strNewOIS, "video")) {
        newOIS = OPTICAL_STABILIZATION_MODE_VIDEO;
    } else if (!strcmp(strNewOIS, "sine_x")) {
        newOIS = OPTICAL_STABILIZATION_MODE_SINE_X;
    } else if (!strcmp(strNewOIS, "sine_y")) {
        newOIS = OPTICAL_STABILIZATION_MODE_SINE_Y;
    } else if (!strcmp(strNewOIS, "center")) {
        newOIS = OPTICAL_STABILIZATION_MODE_CENTERING;
    }
#ifdef SAMSUNG_OIS_VDIS
    else if (!strcmp(strNewOIS, "vdis")) {
        newOIS = OPTICAL_STABILIZATION_MODE_VDIS;
    }
#endif
    else {
        ALOGE("ERR(%s):Invalid ois command(%s)", __FUNCTION__, strNewOIS);
        return BAD_VALUE;
    }

    curOIS = getOIS();

    if (curOIS != newOIS) {
        ALOGD("%s set OIS, new OIS Mode = %d", __FUNCTION__, newOIS);
        m_setOIS(newOIS);
        m_params.set("ois", strNewOIS);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setOIS(enum  optical_stabilization_mode ois)
{
    m_cameraInfo.oisMode = ois;

    if(getZoomActiveOn()) {
        ALOGD("DEBUG(%s):zoom moving..", "setParameters");
        return;
    }

#if 0 // Host controlled OIS Factory Mode
    if(m_oisNode) {
        setOISMode();
    } else {
        ALOGD("Ois node is not prepared yet(%s[%d]) !!!!", __FUNCTION__, __LINE__);
        m_setOISmodeSetting = true;
    }
#else
    setOISMode();

#ifdef SAMSUNG_OIS_VDIS
    setOISCoef(0x00);
#endif
#endif
}

enum optical_stabilization_mode ExynosCameraParameters::getOIS(void)
{
    return m_cameraInfo.oisMode;
}

void ExynosCameraParameters::setOISNode(ExynosCameraNode *node)
{
    m_oisNode = node;
}

void ExynosCameraParameters::setOISModeSetting(bool enable)
{
    m_setOISmodeSetting = enable;
}

int ExynosCameraParameters::getOISModeSetting(void)
{
    return m_setOISmodeSetting;
}

void ExynosCameraParameters::setOISMode(void)
{
    int ret = 0;

    ALOGD("(%s[%d])set OIS Mode = %d", __FUNCTION__, __LINE__, m_cameraInfo.oisMode);

    setMetaCtlOIS(&m_metadata, m_cameraInfo.oisMode);

#if 0 // Host controlled OIS Factory Mode
    if (m_cameraInfo.oisMode == OPTICAL_STABILIZATION_MODE_SINE_X && m_oisNode != NULL) {
        ret = m_oisNode->setControl(V4L2_CID_CAMERA_OIS_SINE_MODE, OPTICAL_STABILIZATION_MODE_SINE_X);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
    } else if (m_cameraInfo.oisMode == OPTICAL_STABILIZATION_MODE_SINE_Y && m_oisNode != NULL) {
        ret = m_oisNode->setControl(V4L2_CID_CAMERA_OIS_SINE_MODE, OPTICAL_STABILIZATION_MODE_SINE_Y);
        if (ret < 0) {
            ALOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
    }
#endif
}

#endif

#ifdef SAMSUNG_COMPANION
status_t ExynosCameraParameters::checkRTDrc(const CameraParameters& params)
{
    enum companion_drc_mode newRTDrc = COMPANION_DRC_OFF;
    enum companion_drc_mode curRTDrc = COMPANION_DRC_OFF;
    const char *strNewRTDrc = params.get("dynamic-range-control");

    if (strNewRTDrc == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):strNewRTDrc %s", "setParameters", strNewRTDrc);

    if (!strcmp(strNewRTDrc, "off")) {
        newRTDrc = COMPANION_DRC_OFF;
    } else if (!strcmp(strNewRTDrc, "on")) {
        newRTDrc = COMPANION_DRC_ON;
    } else {
        CLOGE("ERR(%s):Invalid rt drc(%s)", __FUNCTION__, strNewRTDrc);
        return BAD_VALUE;
    }

    curRTDrc = getRTDrc();

    if (curRTDrc != newRTDrc) {
        m_setRTDrc(newRTDrc);
        m_params.set("dynamic-range-control", strNewRTDrc);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setRTDrc(enum  companion_drc_mode rtdrc)
{
    setMetaCtlRTDrc(&m_metadata, rtdrc);
}

enum companion_drc_mode ExynosCameraParameters::getRTDrc(void)
{
    enum companion_drc_mode mode;

    getMetaCtlRTDrc(&m_metadata, &mode);

    return mode;
}

status_t ExynosCameraParameters::checkPaf(const CameraParameters& params)
{
    enum companion_paf_mode newPaf = COMPANION_PAF_OFF;
    enum companion_paf_mode curPaf = COMPANION_PAF_OFF;
    const char *strNewPaf = params.get("phase-af");

    if (strNewPaf == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):strNewPaf %s", "setParameters", strNewPaf);

#ifdef RAWDUMP_CAPTURE
    newPaf = COMPANION_PAF_OFF;
#else
    if (!strcmp(strNewPaf, "off")) {
        newPaf = COMPANION_PAF_OFF;
    } else if (!strcmp(strNewPaf, "on")) {
        newPaf = COMPANION_PAF_ON;
    } else {
        CLOGE("ERR(%s):Invalid paf(%s)", __FUNCTION__, strNewPaf);
        return BAD_VALUE;
    }
#endif

    curPaf = getPaf();

    if (curPaf != newPaf) {
        m_setPaf(newPaf);
        m_params.set("phase-af", strNewPaf);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setPaf(enum companion_paf_mode paf)
{
    setMetaCtlPaf(&m_metadata, paf);
}

enum companion_paf_mode ExynosCameraParameters::getPaf(void)
{
    enum companion_paf_mode mode = COMPANION_PAF_OFF;

    getMetaCtlPaf(&m_metadata, &mode);

    return mode;
}

status_t ExynosCameraParameters::checkRTHdr(const CameraParameters& params)
{
    enum companion_wdr_mode newRTHdr = COMPANION_WDR_OFF;
    enum companion_wdr_mode curRTHdr = COMPANION_WDR_OFF;
    const char *strNewRTHdr = params.get("rt-hdr");

    if (strNewRTHdr == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):strNewRTWdr %s", "setParameters", strNewRTHdr);

    if (!strcmp(strNewRTHdr, "off")) {
        newRTHdr = COMPANION_WDR_OFF;
    } else if (!strcmp(strNewRTHdr, "on")) {
        newRTHdr = COMPANION_WDR_ON;
    } else if (!strcmp(strNewRTHdr, "auto")) {
        newRTHdr = COMPANION_WDR_AUTO;
    } else {
        CLOGE("ERR(%s):Invalid rt wdr(%s)", __FUNCTION__, strNewRTHdr);
        return BAD_VALUE;
    }

    curRTHdr = getRTHdr();

    if (curRTHdr != newRTHdr) {
        m_setRTHdr(newRTHdr);
        m_params.set("rt-hdr", strNewRTHdr);
        /* For Samsung SDK */
        if (getPreviewRunning() == true) {
            ALOGD("DEBUG(%s[%d]):setRestartPreviewChecked true", __FUNCTION__, __LINE__);
            m_setRestartPreviewChecked(true);
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraParameters::checkSceneRTHdr(bool onoff)
{
    if (onoff) {
        m_setRTHdr(COMPANION_WDR_ON);
        m_setRTDrc(COMPANION_DRC_ON);
    } else {
        m_setRTHdr(COMPANION_WDR_OFF);
        m_setRTDrc(COMPANION_DRC_OFF);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setRTHdr(enum companion_wdr_mode rthdr)
{
    setMetaCtlRTHdr(&m_metadata, rthdr);
}

enum companion_wdr_mode ExynosCameraParameters::getRTHdr(void)
{
    enum companion_wdr_mode mode = COMPANION_WDR_OFF;

    if (getUseCompanion() == true)
        getMetaCtlRTHdr(&m_metadata, &mode);

    return mode;
}
#endif

status_t ExynosCameraParameters::checkIso(const CameraParameters& params)
{
    uint32_t newISO = 0;
    uint32_t curISO = 0;
    const char *strNewISO = params.get("iso");

    if (strNewISO == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):strNewISO %s", "setParameters", strNewISO);

    if (!strcmp(strNewISO, "auto")) {
        newISO = 0;
    } else {
        newISO = (uint32_t)atoi(strNewISO);
        if (newISO == 0) {
            CLOGE("ERR(%s):Invalid iso value(%s)", __FUNCTION__, strNewISO);
            return BAD_VALUE;
        }
    }

    curISO = getIso();

    if (curISO != newISO) {
        m_setIso(newISO);
        m_params.set("iso", strNewISO);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setIso(uint32_t iso)
{
    enum aa_isomode mode = AA_ISOMODE_AUTO;

    if (iso == 0)
        mode = AA_ISOMODE_AUTO;
    else
        mode = AA_ISOMODE_MANUAL;

    setMetaCtlIso(&m_metadata, mode, iso);
}

uint32_t ExynosCameraParameters::getIso(void)
{
    enum aa_isomode mode = AA_ISOMODE_AUTO;
    uint32_t iso = 0;

    getMetaCtlIso(&m_metadata, &mode, &iso);

    return iso;
}

#ifdef SAMSUNG_TN_FEATURE
void ExynosCameraParameters::setParamIso(camera2_udm *udm)
{
    uint32_t iso = udm->internal.vendorSpecific2[101];

    ALOGI("DEBUG(%s):set exif_iso %u", __FUNCTION__, iso);

    m_params.set("exif_iso", iso);
}
#endif

status_t ExynosCameraParameters::checkContrast(const CameraParameters& params)
{
    uint32_t newContrast = 0;
    uint32_t curContrast = 0;
    const char *strNewContrast = params.get("contrast");

    if (strNewContrast == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):strNewContrast %s", "setParameters", strNewContrast);

    if (!strcmp(strNewContrast, "auto"))
        newContrast = IS_CONTRAST_DEFAULT;
    else if (!strcmp(strNewContrast, "-2"))
        newContrast = IS_CONTRAST_MINUS_2;
    else if (!strcmp(strNewContrast, "-1"))
        newContrast = IS_CONTRAST_MINUS_1;
    else if (!strcmp(strNewContrast, "0"))
        newContrast = IS_CONTRAST_DEFAULT;
    else if (!strcmp(strNewContrast, "1"))
        newContrast = IS_CONTRAST_PLUS_1;
    else if (!strcmp(strNewContrast, "2"))
        newContrast = IS_CONTRAST_PLUS_2;
    else {
        CLOGE("ERR(%s):Invalid contrast value(%s)", __FUNCTION__, strNewContrast);
        return BAD_VALUE;
    }

    curContrast = getContrast();

    if (curContrast != newContrast) {
        m_setContrast(newContrast);
        m_params.set("contrast", strNewContrast);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setContrast(uint32_t contrast)
{
    setMetaCtlContrast(&m_metadata, contrast);
}

uint32_t ExynosCameraParameters::getContrast(void)
{
    uint32_t contrast = 0;
    getMetaCtlContrast(&m_metadata, &contrast);
    return contrast;
}

status_t ExynosCameraParameters::checkHdrMode(const CameraParameters& params)
{
    int newHDR = params.getInt("hdr-mode");
    bool curHDR = -1;

    if (newHDR < 0) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):newHDR %d", "setParameters", newHDR);

    curHDR = getHdrMode();

    if (curHDR != (bool)newHDR) {
        m_setHdrMode((bool)newHDR);
        m_params.set("hdr-mode", newHDR);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setHdrMode(bool hdr)
{
    m_cameraInfo.hdrMode = hdr;

#ifdef CAMERA_GED_FEATURE
    if (hdr == true)
        m_setShotMode(SHOT_MODE_RICH_TONE);
    else
        m_setShotMode(SHOT_MODE_NORMAL);
#endif

    m_activityControl->setHdrMode(hdr);
}

bool ExynosCameraParameters::getHdrMode(void)
{
    return m_cameraInfo.hdrMode;
}

status_t ExynosCameraParameters::checkWdrMode(const CameraParameters& params)
{
    int newWDR = params.getInt("wdr");
    bool curWDR = -1;
    bool toggle = false;

    if (newWDR < 0) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):newWDR %d", "setParameters", newWDR);

    curWDR = getWdrMode();

    if (curWDR != (bool)newWDR) {
        m_setWdrMode((bool)newWDR);
        m_params.set("wdr", newWDR);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setWdrMode(bool wdr)
{
    m_cameraInfo.wdrMode = wdr;
}

bool ExynosCameraParameters::getWdrMode(void)
{
    return m_cameraInfo.wdrMode;
}

#ifdef SAMSUNG_LBP
int32_t ExynosCameraParameters::getHoldFrameCount(void)
{
    if(getCameraId() == CAMERA_ID_BACK)
        return MAX_LIVE_REAR_BEST_PICS;
    else
        return MAX_LIVE_FRONT_BEST_PICS;
}
#endif

#ifdef USE_BINNING_MODE
int ExynosCameraParameters::getBinningMode(void)
{
    int ret = 0;
    int vt_mode = getVtMode();
#ifdef USE_LIVE_BROADCAST
    int camera_id = getCameraId();
    bool plb_mode = getPLBMode();
#endif

    if (m_staticInfo->vtcallSizeLutMax == 0 || m_staticInfo->vtcallSizeLut == NULL) {
       ALOGV("(%s):vtCallSizeLut is NULL, can't support the binnig mode", __FUNCTION__);
       return ret;
    }

#ifdef SAMSUNG_COMPANION
    if ((getCameraId() == CAMERA_ID_FRONT) && getUseCompanion()) {
        ALOGV("(%s): Companion mode in front can't support the binning mode.(%d,%d)",
        __FUNCTION__, getCameraId(), getUseCompanion());
        return ret;
    }
#endif

    /* For VT Call with DualCamera Scenario */
    if (getDualMode()
#ifdef USE_LIVE_BROADCAST_DUAL
        && (plb_mode != true || (plb_mode == true && camera_id == CAMERA_ID_FRONT))
#endif
    ) {
        ALOGV("(%s):DualMode can't support the binnig mode.(%d,%d)", __FUNCTION__, getCameraId(), getDualMode());
        return ret;
    }

    if (vt_mode != 3 && vt_mode > 0 && vt_mode < 5) {
        ret = 1;
    }
#ifdef USE_LIVE_BROADCAST
    else if (plb_mode == true) {
        if (camera_id == CAMERA_ID_BACK) {
            ret = 1;
        }
    }
#endif
    else {
        if (m_binningProperty == true && getSamsungCamera() == false) {
            ret = 1;
        }
    }
    return ret;
}
#endif

status_t ExynosCameraParameters::checkShotMode(const CameraParameters& params)
{
    int newShotMode = params.getInt("shot-mode");
    int curShotMode = -1;

#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    if(getSamsungCamera() == false) {
        int vtMode = params.getInt("vtmode");

        newShotMode = checkPropertyForShotMode(newShotMode, vtMode);
    }
#endif
    if (newShotMode < 0) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):newShotMode %d", "setParameters", newShotMode);

    curShotMode = getShotMode();

    if ((getRecordingHint() == true)
#ifdef SAMSUNG_TN_FEATURE
        && (newShotMode != SHOT_MODE_SEQUENCE) && (newShotMode != SHOT_MODE_AQUA)
#endif
#ifdef USE_FASTMOTION_CROP
        && (newShotMode != SHOT_MODE_FASTMOTION)
#endif
        ) {
        m_setShotMode(SHOT_MODE_NORMAL);
        m_params.set("shot-mode", SHOT_MODE_NORMAL);
    } else if (curShotMode != newShotMode) {
        m_setShotMode(newShotMode);
        m_params.set("shot-mode", newShotMode);

        updatePreviewFpsRange();
    }
#ifdef LLS_CAPTURE
    else if (m_flagLLSOn && getSceneMode() == SCENE_MODE_AUTO) {
        m_setLLSShotMode();
    }
#endif

    return NO_ERROR;
}

void ExynosCameraParameters::m_setShotMode(int shotMode)
{
    enum aa_mode mode = AA_CONTROL_AUTO;
    enum aa_scene_mode sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
    bool changeSceneMode = true;

    switch (shotMode) {
    case SHOT_MODE_DRAMA:
#ifdef SAMSUNG_DOF
    case SHOT_MODE_LIGHT_TRACE:
#endif
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_DRAMA;
        break;
#ifdef SAMSUNG_MAGICSHOT
    case SHOT_MODE_MAGIC:
        if (getCameraId() == CAMERA_ID_BACK) {
            mode = AA_CONTROL_USE_SCENE_MODE;
            sceneMode = AA_SCENE_MODE_DRAMA;
        } else {
            mode = AA_CONTROL_AUTO;
            sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
        }
        break;
#endif
    case SHOT_MODE_3D_PANORAMA:
    case SHOT_MODE_PANORAMA:
    case SHOT_MODE_FRONT_PANORAMA:
    case SHOT_MODE_INTERACTIVE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_PANORAMA;
        break;
    case SHOT_MODE_NIGHT:
    case SHOT_MODE_NIGHT_SCENE:
#ifdef SAMSUNG_FOOD_MODE
        if (getSceneMode() == SCENE_MODE_FOOD) {
            mode = AA_CONTROL_USE_SCENE_MODE;
            sceneMode = AA_SCENE_MODE_FOOD;
        } else
#endif
        {
            mode = AA_CONTROL_USE_SCENE_MODE;
            sceneMode = AA_SCENE_MODE_LLS;
        }
        break;
    case SHOT_MODE_ANIMATED_SCENE:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_ANIMATED;
        break;
    case SHOT_MODE_SPORTS:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_SPORTS;
        break;
    case SHOT_MODE_GOLF:
#ifdef SAMSUNG_TN_FEATURE
    case SHOT_MODE_SEQUENCE:
#endif
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_GOLF;
        break;
    case SHOT_MODE_NORMAL:
        if (getDualMode() == true) {
            mode = AA_CONTROL_USE_SCENE_MODE;
            sceneMode = AA_SCENE_MODE_DUAL;
        } else {
            mode = AA_CONTROL_AUTO;
            sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
        }
        break;
    case SHOT_MODE_AUTO:
    case SHOT_MODE_BEAUTY_FACE:
    case SHOT_MODE_BEST_PHOTO:
    case SHOT_MODE_BEST_FACE:
    case SHOT_MODE_ERASER:
    case SHOT_MODE_RICH_TONE:
    case SHOT_MODE_STORY:
    case SHOT_MODE_SELFIE_ALARM:
#ifdef SAMSUNG_DOF
    case SHOT_MODE_3DTOUR:
    case SHOT_MODE_OUTFOCUS:
#endif
    case SHOT_MODE_FASTMOTION:
    case SHOT_MODE_PRO_MODE:
        mode = AA_CONTROL_AUTO;
        sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
        break;
    case SHOT_MODE_DUAL:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_DUAL;
        break;
    case SHOT_MODE_AQUA:
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_AQUA;
        break;
    case SHOT_MODE_AUTO_PORTRAIT:
    case SHOT_MODE_PET:
#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    case THIRD_PARTY_BLACKBOX_MODE:
    case THIRD_PARTY_VTCALL_MODE:
#endif
    default:
        changeSceneMode = false;
        break;
    }

#ifdef LLS_CAPTURE
    if(m_flagLLSOn) {
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_LLS;
    }
#endif

    m_cameraInfo.shotMode = shotMode;
    if (changeSceneMode == true)
        setMetaCtlSceneMode(&m_metadata, mode, sceneMode);
}

int ExynosCameraParameters::getPreviewBufferCount(void)
{
    CLOGV("DEBUG(%s):getPreviewBufferCount %d", "setParameters", m_previewBufferCount);

    return m_previewBufferCount;
}

void ExynosCameraParameters::setPreviewBufferCount(int previewBufferCount)
{
    m_previewBufferCount = previewBufferCount;

    CLOGV("DEBUG(%s):setPreviewBufferCount %d", "setParameters", m_previewBufferCount);

    return;
}

int ExynosCameraParameters::getShotMode(void)
{
    return m_cameraInfo.shotMode;
}

status_t ExynosCameraParameters::checkAntiShake(const CameraParameters& params)
{
    int newAntiShake = params.getInt("anti-shake");
    bool curAntiShake = false;
    bool toggle = false;
    int curShotMode = getShotMode();

    if (curShotMode != SHOT_MODE_AUTO)
        return NO_ERROR;

    if (newAntiShake < 0) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):newAntiShake %d", "setParameters", newAntiShake);

    if (newAntiShake == 1)
        toggle = true;

    curAntiShake = getAntiShake();

    if (curAntiShake != toggle) {
        m_setAntiShake(toggle);
        m_params.set("anti-shake", newAntiShake);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setAntiShake(bool toggle)
{
    enum aa_mode mode = AA_CONTROL_AUTO;
    enum aa_scene_mode sceneMode = AA_SCENE_MODE_FACE_PRIORITY;

    if (toggle == true) {
        mode = AA_CONTROL_USE_SCENE_MODE;
        sceneMode = AA_SCENE_MODE_ANTISHAKE;
    }

    setMetaCtlSceneMode(&m_metadata, mode, sceneMode);
    m_cameraInfo.antiShake = toggle;
}

bool ExynosCameraParameters::getAntiShake(void)
{
    return m_cameraInfo.antiShake;
}

status_t ExynosCameraParameters::checkVtMode(const CameraParameters& params)
{
    int newVTMode = params.getInt("vtmode");
    int curVTMode = -1;

    CLOGD("DEBUG(%s):newVTMode %d", "setParameters", newVTMode);

    /*
     * VT mode
     *   1: 3G vtmode (176x144, Fixed 7fps)
     *   2: LTE or WIFI vtmode (640x480, Fixed 15fps)
     *   3: Reserved : Smart Stay
     *   4: CHINA vtmode (1280x720, Fixed 30fps)
     */
    if (newVTMode == 3 || (newVTMode < 0 || newVTMode > 4)) {
        newVTMode = 0;
    }

    curVTMode = getVtMode();

    if (curVTMode != newVTMode) {
        m_setVtMode(newVTMode);
        m_params.set("vtmode", newVTMode);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setVtMode(int vtMode)
{
    m_cameraInfo.vtMode = vtMode;

    setMetaVtMode(&m_metadata, (enum camera_vt_mode)vtMode);
}

int ExynosCameraParameters::getVtMode(void)
{
    return m_cameraInfo.vtMode;
}

status_t ExynosCameraParameters::checkVRMode(const CameraParameters& params)
{
    int newVRMode =  params.getInt("vrmode");
    int curVRMode = getVRMode();

    CLOGD("DEBUG(%s):newVRMode %d", "setParameters", newVRMode);

    if (curVRMode != newVRMode) {
        m_setVRMode(newVRMode);
        m_params.set("vrmode", newVRMode);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setVRMode(int vrMode)
{
    m_cameraInfo.vrMode = vrMode;
}

int ExynosCameraParameters::getVRMode(void)
{
    return m_cameraInfo.vrMode;
}

#ifdef USE_LIVE_BROADCAST
void ExynosCameraParameters::setPLBMode(bool plbMode)
{
    m_cameraInfo.plbMode = plbMode;
}

bool ExynosCameraParameters::getPLBMode(void)
{
    return m_cameraInfo.plbMode;
}
#endif

status_t ExynosCameraParameters::checkGamma(const CameraParameters& params)
{
    bool newGamma = false;
    bool curGamma = false;
    const char *strNewGamma = params.get("video_recording_gamma");

    if (strNewGamma == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):strNewGamma %s", "setParameters", strNewGamma);

    if (!strcmp(strNewGamma, "off")) {
        newGamma = false;
    } else if (!strcmp(strNewGamma, "on")) {
        newGamma = true;
    } else {
        CLOGE("ERR(%s):unmatched gamma(%s)", __FUNCTION__, strNewGamma);
        return BAD_VALUE;
    }

    curGamma = getGamma();

    if (curGamma != newGamma) {
        m_setGamma(newGamma);
        m_params.set("video_recording_gamma", strNewGamma);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setGamma(bool gamma)
{
    m_cameraInfo.gamma = gamma;
}

bool ExynosCameraParameters::getGamma(void)
{
    return m_cameraInfo.gamma;
}

status_t ExynosCameraParameters::checkSlowAe(const CameraParameters& params)
{
    bool newSlowAe = false;
    bool curSlowAe = false;
    const char *strNewSlowAe = params.get("slow_ae");

    if (strNewSlowAe == NULL) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):strNewSlowAe %s", "setParameters", strNewSlowAe);

    if (!strcmp(strNewSlowAe, "off"))
        newSlowAe = false;
    else if (!strcmp(strNewSlowAe, "on"))
        newSlowAe = true;
    else {
        CLOGE("ERR(%s):unmatched slow_ae(%s)", __FUNCTION__, strNewSlowAe);
        return BAD_VALUE;
    }

    curSlowAe = getSlowAe();

    if (curSlowAe != newSlowAe) {
        m_setSlowAe(newSlowAe);
        m_params.set("slow_ae", strNewSlowAe);
    }

    return NO_ERROR;
}

void ExynosCameraParameters::m_setSlowAe(bool slowAe)
{
    m_cameraInfo.slowAe = slowAe;
}

bool ExynosCameraParameters::getSlowAe(void)
{
    return m_cameraInfo.slowAe;
}

status_t ExynosCameraParameters::checkScalableSensorMode(const CameraParameters& params)
{
    bool needScaleMode = false;
    bool curScaleMode = false;
    int newScaleMode = params.getInt("scale_mode");

    if (newScaleMode < 0) {
        return NO_ERROR;
    }

    CLOGD("DEBUG(%s):newScaleMode %d", "setParameters", newScaleMode);

    if (isScalableSensorSupported() == true) {
        needScaleMode = m_adjustScalableSensorMode(newScaleMode);
        curScaleMode = getScalableSensorMode();

        if (curScaleMode != needScaleMode) {
            setScalableSensorMode(needScaleMode);
            m_params.set("scale_mode", newScaleMode);
        }

//        updateHwSensorSize();
    }

    return NO_ERROR;
}

bool ExynosCameraParameters::isScalableSensorSupported(void)
{
    return m_staticInfo->scalableSensorSupport;
}

bool ExynosCameraParameters::m_adjustScalableSensorMode(const int scaleMode)
{
    bool adjustedScaleMode = false;
    int pictureW = 0;
    int pictureH = 0;
    float pictureRatio = 0;
    uint32_t minFps = 0;
    uint32_t maxFps = 0;

    /* If scale_mode is 1 or dual camera, scalable sensor turn on */
    if (scaleMode == 1)
        adjustedScaleMode = true;

    if (getDualMode() == true
#ifdef USE_LIVE_BROADCAST_DUAL
        && (plb_mode != true || (plb_mode == true && camera_id == CAMERA_ID_FRONT))
#endif
    ) {
        adjustedScaleMode = true;
    }

    /*
     * scalable sensor only support     24     fps for 4:3  - picture size
     * scalable sensor only support 15, 24, 30 fps for 16:9 - picture size
     */
    getPreviewFpsRange(&minFps, &maxFps);
    getPictureSize(&pictureW, &pictureH);

    pictureRatio = ROUND_OFF(((float)pictureW / (float)pictureH), 2);

    if (pictureRatio == 1.33f) { /* 4:3 */
        if (maxFps != 24)
            adjustedScaleMode = false;
    } else if (pictureRatio == 1.77f) { /* 16:9 */
        if ((maxFps != 15) && (maxFps != 24) && (maxFps != 30))
            adjustedScaleMode = false;
    } else {
        adjustedScaleMode = false;
    }

    if (scaleMode == 1 && adjustedScaleMode == false) {
        CLOGW("WARN(%s):pictureRatio(%f, %d, %d) fps(%d, %d) is not proper for scalable",
                __FUNCTION__, pictureRatio, pictureW, pictureH, minFps, maxFps);
    }

    return adjustedScaleMode;
}

void ExynosCameraParameters::setScalableSensorMode(bool scaleMode)
{
    m_cameraInfo.scalableSensorMode = scaleMode;
}

bool ExynosCameraParameters::getScalableSensorMode(void)
{
    return m_cameraInfo.scalableSensorMode;
}

void ExynosCameraParameters::m_getScalableSensorSize(int *newSensorW, int *newSensorH)
{
    int previewW = 0;
    int previewH = 0;

    *newSensorW = 1920;
    *newSensorH = 1080;

    /* default scalable sensor size is 1920x1080(16:9) */
    getPreviewSize(&previewW, &previewH);

    /* when preview size is 1440x1080(4:3), return sensor size(1920x1440) */
    /* if (previewW == 1440 && previewH == 1080) { */
    if ((previewW * 3 / 4) == previewH) {
        *newSensorW  = 1920;
        *newSensorH = 1440;
    }
}

status_t ExynosCameraParameters::checkImageUniqueId(__unused const CameraParameters& params)
{
    const char *strCurImageUniqueId = m_params.get("imageuniqueid-value");
    const char *strNewImageUniqueId = NULL;

#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
    if (m_romReadThreadDone != true) {
        ALOGD("DEBUG(%s): checkImageUniqueId : romReadThreadDone(%d)", "setParameters", m_romReadThreadDone);
        return NO_ERROR;
    }
#endif

    if(strCurImageUniqueId == NULL || strcmp(strCurImageUniqueId, "") == 0 || strcmp(strCurImageUniqueId, "0") == 0) {
        strNewImageUniqueId = getImageUniqueId();

        if (strNewImageUniqueId != NULL && strcmp(strNewImageUniqueId, "") != 0) {
            ALOGD("DEBUG(%s):newImageUniqueId %s ", "setParameters", strNewImageUniqueId );
            m_params.set("imageuniqueid-value", strNewImageUniqueId);
        }
    }
    return NO_ERROR;
}

status_t ExynosCameraParameters::m_setImageUniqueId(const char *uniqueId)
{
    int uniqueIdLength;

    if (uniqueId == NULL) {
        return BAD_VALUE;
    }

    memset(m_cameraInfo.imageUniqueId, 0, sizeof(m_cameraInfo.imageUniqueId));

    uniqueIdLength = strlen(uniqueId);
    memcpy(m_cameraInfo.imageUniqueId, uniqueId, uniqueIdLength);

    return NO_ERROR;
}

const char *ExynosCameraParameters::getImageUniqueId(void)
{
#if defined(SAMSUNG_TN_FEATURE) && defined(SENSOR_FW_GET_FROM_FILE)
    char *sensorfw = NULL;
    char *uniqueid = NULL;
    char *savePtr;
#ifdef FORCE_CAL_RELOAD
    char checkcal[50];

    memset(checkcal, 0, sizeof(checkcal));
#endif
    sensorfw = (char *)getSensorFWFromFile(m_staticInfo, m_cameraId);
#ifdef FORCE_CAL_RELOAD
    sprintf(checkcal, "%s", sensorfw);
    m_calValid = m_checkCalibrationDataValid(checkcal);
#endif

    if (getCameraId() == CAMERA_ID_BACK) {
        uniqueid = sensorfw;
    } else {
#ifdef SAMSUNG_EEPROM_FRONT
        if (SAMSUNG_EEPROM_FRONT) {
            uniqueid = sensorfw;
        } else
#endif
        {
            uniqueid = strtok_r(sensorfw, " ", &savePtr);
        }
    }
    setImageUniqueId(uniqueid);

    return (const char *)m_cameraInfo.imageUniqueId;
#else
    return m_cameraInfo.imageUniqueId;
#endif
}

#ifdef SAMSUNG_TN_FEATURE
void ExynosCameraParameters::setImageUniqueId(char *uniqueId)
{
    memcpy(m_cameraInfo.imageUniqueId, uniqueId, sizeof(m_cameraInfo.imageUniqueId));
}
#endif

#ifdef BURST_CAPTURE
status_t ExynosCameraParameters::checkSeriesShotFilePath(const CameraParameters& params)
{
    const char *seriesShotFilePath = params.get("capture-burst-filepath");

    if (seriesShotFilePath != NULL) {
        snprintf(m_seriesShotFilePath, sizeof(m_seriesShotFilePath), "%s", seriesShotFilePath);
        CLOGD("DEBUG(%s): seriesShotFilePath %s", "setParameters", seriesShotFilePath);
        m_params.set("capture-burst-filepath", seriesShotFilePath);
    } else {
        CLOGD("DEBUG(%s): seriesShotFilePath NULL", "setParameters");
        memset(m_seriesShotFilePath, 0, CAMERA_FILE_PATH_SIZE);
    }

    return NO_ERROR;
}
#endif

status_t ExynosCameraParameters::checkSeriesShotMode(const CameraParameters& params)
{
#ifndef SAMSUNG_TN_FEATURE
    int burstCount = params.getInt("burst-capture");
    int bestCount = params.getInt("best-capture");

    CLOGD("DEBUG(%s): burstCount(%d), bestCount(%d)", "setParameters", burstCount, bestCount);

    if (burstCount < 0 || bestCount < 0) {
        CLOGE("ERR(%s[%d]): Invalid burst-capture count(%d), best-capture count(%d)", __FUNCTION__, __LINE__, burstCount, bestCount);
        return BAD_VALUE;
    }

    /* TODO: select shot count */
    if (bestCount > burstCount) {
        m_setSeriesShotCount(bestCount);
        m_params.set("burst-capture", 0);
        m_params.set("best-capture", bestCount);
    } else {
        m_setSeriesShotCount(burstCount);
        m_params.set("burst-capture", burstCount);
        m_params.set("best-capture", 0);
    }
#endif
    return NO_ERROR;
}

#ifdef SAMSUNG_LLV
status_t ExynosCameraParameters::checkLLV(const CameraParameters& params)
{
    int mode = params.getInt("llv_mode");
    int prev_mode = m_params.getInt("llv_mode");

    if (mode !=  prev_mode) {
        CLOGW("[PARM_DBG] new LLV mode : %d ", mode);
        setLLV(mode);
        m_params.set("llv_mode", mode);
    } else {
        CLOGD("%s: No value change in LLV mode : %d", __FUNCTION__, mode);
    }

    return NO_ERROR;
}
#endif

#ifdef SAMSUNG_DOF
status_t ExynosCameraParameters::checkMoveLens(const CameraParameters& params)
{
    const char *newStrMoveLens = params.get("focus-bracketing-values");
    const char *curStrMoveLens = m_params.get("focus-bracketing-values");

    if(newStrMoveLens != NULL) {
        CLOGD("[DOF][%s][%d] string : %s", "checkMoveLens", __LINE__, newStrMoveLens);
    }
    else {
        memset(m_cameraInfo.lensPosTbl, 0, sizeof(m_cameraInfo.lensPosTbl));
        return NO_ERROR;
    }

    if(SHOT_MODE_OUTFOCUS != getShotMode())
        return NO_ERROR;

    CLOGD("[DOF][PARM_DBG] new Lens position : %s ", newStrMoveLens);
    char *start;
    char *end;
    char delim = ',';
    int N = 6; // max count is 5 (+ number of value)

    if (newStrMoveLens != NULL) {
        start = (char *)newStrMoveLens;
        for(int i = 1; i < N; i++) {
            m_cameraInfo.lensPosTbl[i] = (int) strtol(start, &end, 10);
            CLOGD("[DOF][%s][%d] lensPosTbl[%d] : %d",
                    "checkMoveLens", __LINE__, i, m_cameraInfo.lensPosTbl[i]);
            if(*end != delim) {
                m_cameraInfo.lensPosTbl[0] = i; // Total Count
                CLOGD("[DOF][%s][%d] lensPosTbl[0] : %d", "checkMoveLens", __LINE__, i);
                break;
            }
            start = end+1;
        }
    }

    m_params.set("focus-bracketing-values", newStrMoveLens);

    return NO_ERROR;
}

int ExynosCameraParameters::getMoveLensTotal(void)
{
    return m_cameraInfo.lensPosTbl[0];
}

void ExynosCameraParameters::setMoveLensTotal(int count)
{
    m_cameraInfo.lensPosTbl[0] = count;
}

void ExynosCameraParameters::setMoveLensCount(int count)
{
    m_curLensCount = count;
    CLOGD("[DOF][%s][%d] m_curLensCount : %d",
                "setMoveLensCount", __LINE__, m_curLensCount);
}

void ExynosCameraParameters::m_setLensPosition(int step)
{
    CLOGD("[DOF][%s][%d] step : %d",
                "m_setLensPosition", __LINE__, step);
    setMetaCtlLensPos(&m_metadata, m_cameraInfo.lensPosTbl[step]);
    m_curLensStep = m_cameraInfo.lensPosTbl[step];
}
#endif

#ifdef SAMSUNG_OIS_VDIS
void ExynosCameraParameters::setOISCoef(uint32_t coef)
{
    setMetaCtlOISCoef(&m_metadata, coef);
}
#endif

#ifdef BURST_CAPTURE
int ExynosCameraParameters::getSeriesShotSaveLocation(void)
{
    int seriesShotSaveLocation = m_seriesShotSaveLocation;
    int shotMode = getShotMode();

    /* GED's series shot work as callback */
#ifdef CAMERA_GED_FEATURE
    seriesShotSaveLocation = BURST_SAVE_CALLBACK;
#else
    if (shotMode == SHOT_MODE_BEST_PHOTO) {
        seriesShotSaveLocation = BURST_SAVE_CALLBACK;
#ifdef ONE_SECOND_BURST_CAPTURE
    } else if (m_cameraInfo.seriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        seriesShotSaveLocation = BURST_SAVE_CALLBACK;
#endif
    } else {
        if (m_seriesShotSaveLocation == 0)
            seriesShotSaveLocation = BURST_SAVE_PHONE;
        else
            seriesShotSaveLocation = BURST_SAVE_SDCARD;
    }
#endif

    return seriesShotSaveLocation;
}

void ExynosCameraParameters::setSeriesShotSaveLocation(int ssaveLocation)
{
    m_seriesShotSaveLocation = ssaveLocation;
}

char *ExynosCameraParameters::getSeriesShotFilePath(void)
{
    return m_seriesShotFilePath;
}
#endif

int ExynosCameraParameters::getSeriesShotDuration(void)
{
    switch (m_cameraInfo.seriesShotMode) {
    case SERIES_SHOT_MODE_BURST:
        return NORMAL_BURST_DURATION;
    case SERIES_SHOT_MODE_BEST_FACE:
        return BEST_FACE_DURATION;
    case SERIES_SHOT_MODE_BEST_PHOTO:
        return BEST_PHOTO_DURATION;
    case SERIES_SHOT_MODE_ERASER:
        return ERASER_DURATION;
#ifdef SAMSUNG_MAGICSHOT
    case SERIES_SHOT_MODE_MAGIC:
        return MAGIC_SHOT_DURATION;
#endif
    case SERIES_SHOT_MODE_SELFIE_ALARM:
        return SELFIE_ALARM_DURATION;
#ifdef ONE_SECOND_BURST_CAPTURE
    case SERIES_SHOT_MODE_ONE_SECOND_BURST:
        return ONE_SECOND_BURST_CAPTURE_DURATION;
#endif
    default:
        return 0;
    }
    return 0;
}

int ExynosCameraParameters::getSeriesShotMode(void)
{
    return m_cameraInfo.seriesShotMode;
}

void ExynosCameraParameters::setSeriesShotMode(int sshotMode, int count)
{
    int sshotCount = 0;
    int shotMode = getShotMode();
    if (sshotMode == SERIES_SHOT_MODE_BURST) {
        if (shotMode == SHOT_MODE_BEST_PHOTO) {
            sshotMode = SERIES_SHOT_MODE_BEST_PHOTO;
            sshotCount = 8;
        } else if (shotMode == SHOT_MODE_BEST_FACE) {
            sshotMode = SERIES_SHOT_MODE_BEST_FACE;
            sshotCount = 5;
        } else if (shotMode == SHOT_MODE_ERASER) {
            sshotMode = SERIES_SHOT_MODE_ERASER;
            sshotCount = 5;
        }
#ifdef SAMSUNG_MAGICSHOT
        else if (shotMode == SHOT_MODE_MAGIC) {
            sshotMode = SERIES_SHOT_MODE_MAGIC;
            if ( m_cameraId == CAMERA_ID_FRONT)
                sshotCount = 16;
            else
                sshotCount = 32;
        }
#endif
        else if (shotMode == SHOT_MODE_SELFIE_ALARM) {
            sshotMode = SERIES_SHOT_MODE_SELFIE_ALARM;
            sshotCount = 3;
        } else {
            sshotMode = SERIES_SHOT_MODE_BURST;
            sshotCount = MAX_SERIES_SHOT_COUNT;
        }
    } else if (sshotMode == SERIES_SHOT_MODE_LLS ||
            sshotMode == SERIES_SHOT_MODE_SIS) {
            if(count > 0) {
                sshotCount = count;
            } else {
                sshotCount = 5;
            }
#ifdef ONE_SECOND_BURST_CAPTURE
    } else if (sshotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        sshotCount = ONE_SECOND_BURST_CAPTURE_COUNT;
#endif
    }

    CLOGD("DEBUG(%s[%d]: set shotmode(%d), shotCount(%d)", __FUNCTION__, __LINE__, sshotMode, sshotCount);

    m_cameraInfo.seriesShotMode = sshotMode;
    m_setSeriesShotCount(sshotCount);
}

void ExynosCameraParameters::m_setSeriesShotCount(int seriesShotCount)
{
    m_cameraInfo.seriesShotCount = seriesShotCount;
}

int ExynosCameraParameters::getSeriesShotCount(void)
{
    return m_cameraInfo.seriesShotCount;
}

void ExynosCameraParameters::setSamsungCamera(bool value)
{
    String8 tempStr;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_activityControl->getAutoFocusMgr();

    m_cameraInfo.samsungCamera = value;
    autoFocusMgr->setSamsungCamera(value);

    /* zoom */
    if (getZoomSupported() == true) {
        int maxZoom = getMaxZoomLevel();
        ALOGI("INFO(%s):change MaxZoomLevel and ZoomRatio List.(%d)", __FUNCTION__, maxZoom);

        if (0 < maxZoom) {
            m_params.set(CameraParameters::KEY_ZOOM_SUPPORTED, "true");

            if (getSmoothZoomSupported() == true)
                m_params.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "true");
            else
                m_params.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "false");

            m_params.set(CameraParameters::KEY_MAX_ZOOM, maxZoom - 1);
            m_params.set(CameraParameters::KEY_ZOOM, ZOOM_LEVEL_0);

            int max_zoom_ratio = (int)getMaxZoomRatio();
            tempStr.setTo("");
            if (getZoomRatioList(tempStr, maxZoom, max_zoom_ratio, m_staticInfo->zoomRatioList) == NO_ERROR)
                m_params.set(CameraParameters::KEY_ZOOM_RATIOS, tempStr.c_str());
            else
                m_params.set(CameraParameters::KEY_ZOOM_RATIOS, "100");

            m_params.set("constant-growth-rate-zoom-supported", "true");

            ALOGV("INFO(%s):zoomRatioList=%s", "setDefaultParameter", tempStr.c_str());
        } else {
            m_params.set(CameraParameters::KEY_ZOOM_SUPPORTED, "false");
            m_params.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "false");
            m_params.set(CameraParameters::KEY_MAX_ZOOM, ZOOM_LEVEL_0);
            m_params.set(CameraParameters::KEY_ZOOM, ZOOM_LEVEL_0);
        }
    } else {
        m_params.set(CameraParameters::KEY_ZOOM_SUPPORTED, "false");
        m_params.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "false");
        m_params.set(CameraParameters::KEY_MAX_ZOOM, ZOOM_LEVEL_0);
        m_params.set(CameraParameters::KEY_ZOOM, ZOOM_LEVEL_0);
    }
}

bool ExynosCameraParameters::getSamsungCamera(void)
{
    return m_cameraInfo.samsungCamera;
}

bool ExynosCameraParameters::getZoomSupported(void)
{
    return m_staticInfo->zoomSupport;
}

bool ExynosCameraParameters::getSmoothZoomSupported(void)
{
    return m_staticInfo->smoothZoomSupport;
}

void ExynosCameraParameters::checkHorizontalViewAngle(void)
{
    m_params.setFloat(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, getHorizontalViewAngle());
}

void ExynosCameraParameters::setHorizontalViewAngle(int pictureW, int pictureH)
{
    double pi_camera = 3.1415926f;
    double distance;
    double ratio;
    double hViewAngle_half_rad = pi_camera / 180 * (double)m_staticInfo->horizontalViewAngle[SIZE_RATIO_16_9] / 2;

    distance = ((double)m_staticInfo->maxSensorW / (double)m_staticInfo->maxSensorH * 9 / 2)
                / tan(hViewAngle_half_rad);
    ratio = (double)pictureW / (double)pictureH;

    m_calculatedHorizontalViewAngle = (float)(atan(ratio * 9 / 2 / distance) * 2 * 180 / pi_camera);
}

float ExynosCameraParameters::getHorizontalViewAngle(void)
{
    int right_ratio = 177;

    if ((int)(m_staticInfo->maxSensorW * 100 / m_staticInfo->maxSensorH) == right_ratio) {
        return m_calculatedHorizontalViewAngle;
    } else {
        return m_staticInfo->horizontalViewAngle[m_cameraInfo.pictureSizeRatioId];
    }
}

float ExynosCameraParameters::getVerticalViewAngle(void)
{
    return m_staticInfo->verticalViewAngle;
}

void ExynosCameraParameters::getFnumber(int *num, int *den)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        *num = m_staticInfo->fNumber * COMMON_DENOMINATOR;
        *den = COMMON_DENOMINATOR;
    } else {
        *num = m_staticInfo->fNumberNum;
        *den = m_staticInfo->fNumberDen;
    }
}

void ExynosCameraParameters::getApertureValue(int *num, int *den)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        *num = m_staticInfo->aperture * COMMON_DENOMINATOR;
        *den = COMMON_DENOMINATOR;
    } else {
        *num = m_staticInfo->apertureNum;
        *den = m_staticInfo->apertureDen;
    }
}

int ExynosCameraParameters::getFocalLengthIn35mmFilm(void)
{
    return m_staticInfo->focalLengthIn35mmLength;
}

void ExynosCameraParameters::getFocalLength(int *num, int *den)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        *num = m_staticInfo->focalLength * COMMON_DENOMINATOR;
        *den = COMMON_DENOMINATOR;
    } else {
        *num = m_staticInfo->focalLengthNum;
        *den = m_staticInfo->focalLengthDen;
    }
}

void ExynosCameraParameters::getFocusDistances(int *num, int *den)
{
    *num = m_staticInfo->focusDistanceNum;
    *den = m_staticInfo->focusDistanceDen;
}

int ExynosCameraParameters::getMinExposureCompensation(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return m_staticInfo->exposureCompensationRange[MIN];
    } else {
        return m_staticInfo->minExposureCompensation;
    }
}

int ExynosCameraParameters::getMaxExposureCompensation(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return m_staticInfo->exposureCompensationRange[MAX];
    } else {
        return m_staticInfo->maxExposureCompensation;
    }
}

int ExynosCameraParameters::getMinExposureTime(void)
{
    return m_staticInfo->minExposureTime;
}

int ExynosCameraParameters::getMaxExposureTime(void)
{
    return m_staticInfo->maxExposureTime;
}

int64_t ExynosCameraParameters::getSensorMinExposureTime(void)
{
    return m_staticInfo->exposureTimeRange[MIN];
}

int64_t ExynosCameraParameters::getSensorMaxExposureTime(void)
{
    return m_staticInfo->exposureTimeRange[MAX];
}

int ExynosCameraParameters::getMinWBK(void)
{
    return m_staticInfo->minWBK;
}

int ExynosCameraParameters::getMaxWBK(void)
{
    return m_staticInfo->maxWBK;
}

float ExynosCameraParameters::getExposureCompensationStep(void)
{
    return m_staticInfo->exposureCompensationStep;
}

int ExynosCameraParameters::getMaxNumDetectedFaces(void)
{
    return m_staticInfo->maxNumDetectedFaces;
}

uint32_t ExynosCameraParameters::getMaxNumFocusAreas(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return m_staticInfo->max3aRegions[AF];
    } else {
        return m_staticInfo->maxNumFocusAreas;
    }
}

uint32_t ExynosCameraParameters::getMaxNumMeteringAreas(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return m_staticInfo->max3aRegions[AE];
    } else {
        return m_staticInfo->maxNumMeteringAreas;
    }
}

int ExynosCameraParameters::getMaxZoomLevel(void)
{
    int zoomLevel = 0;
    int samsungCamera = getSamsungCamera();

    if (samsungCamera || m_cameraId == CAMERA_ID_FRONT) {
        zoomLevel = m_staticInfo->maxZoomLevel;
    } else {
        zoomLevel = m_staticInfo->maxBasicZoomLevel;
    }
    return zoomLevel;
}

float ExynosCameraParameters::getMaxZoomRatio(void)
{
    return (float)m_staticInfo->maxZoomRatio;
}

float ExynosCameraParameters::getZoomRatio(int zoomLevel)
{
    float zoomRatio = 1.00f;
    if (getZoomSupported() == true)
        zoomRatio = (float)m_staticInfo->zoomRatioList[zoomLevel];
    else
        zoomRatio = 1000.00f;

    return zoomRatio;
}

bool ExynosCameraParameters::getVideoSnapshotSupported(void)
{
    return m_staticInfo->videoSnapshotSupport;
}

bool ExynosCameraParameters::getVideoStabilizationSupported(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        bool supported = false;
        for (size_t i = 0; i < m_staticInfo->videoStabilizationModesLength; i++) {
            if (m_staticInfo->videoStabilizationModes[i]
                    == ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_ON) {
                supported = true;
                break;
            }
        }
        return supported;
    } else {
        return m_staticInfo->videoStabilizationSupport;
    }
}

bool ExynosCameraParameters::getAutoWhiteBalanceLockSupported(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return true;
    } else {
        return m_staticInfo->autoWhiteBalanceLockSupport;
    }
}

bool ExynosCameraParameters::getAutoExposureLockSupported(void)
{
    if (getHalVersion() == IS_HAL_VER_3_2) {
        return true;
    } else {
        return m_staticInfo->autoExposureLockSupport;
    }
}

void ExynosCameraParameters::enableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(m_msgLock);
    m_enabledMsgType |= msgType;
}

void ExynosCameraParameters::disableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(m_msgLock);
    m_enabledMsgType &= ~msgType;
}

bool ExynosCameraParameters::msgTypeEnabled(int32_t msgType)
{
    Mutex::Autolock lock(m_msgLock);
    return (m_enabledMsgType & msgType);
}

void ExynosCameraParameters::m_initMetadata(void)
{
    memset(&m_metadata, 0x00, sizeof(struct camera2_shot_ext));
    struct camera2_shot *shot = &m_metadata.shot;

    // 1. ctl
    // request
    shot->ctl.request.id = 0;
    shot->ctl.request.metadataMode = METADATA_MODE_FULL;
    shot->ctl.request.frameCount = 0;

    // lens
    shot->ctl.lens.focusDistance = -1.0f;
    if (getHalVersion() == IS_HAL_VER_3_2) {
        shot->ctl.lens.aperture = m_staticInfo->aperture;
        shot->ctl.lens.focalLength = m_staticInfo->focalLength;
    } else {
        shot->ctl.lens.aperture = (float)m_staticInfo->apertureNum / (float)m_staticInfo->apertureDen;
        shot->ctl.lens.focalLength = (float)m_staticInfo->focalLengthNum / (float)m_staticInfo->focalLengthDen;
    }
    shot->ctl.lens.filterDensity = 0.0f;
    shot->ctl.lens.opticalStabilizationMode = ::OPTICAL_STABILIZATION_MODE_OFF;

    int minFps = (m_staticInfo->minFps == 0) ? 0 : (m_staticInfo->maxFps / 2);
    int maxFps = (m_staticInfo->maxFps == 0) ? 0 : m_staticInfo->maxFps;

    /* The min fps can not be '0'. Therefore it is set up default value '15'. */
    if (minFps == 0) {
        CLOGW("WRN(%s): Invalid min fps value(%d)", __FUNCTION__, minFps);
        minFps = 15;
    }

    /*  The initial fps can not be '0' and bigger than '30'. Therefore it is set up default value '30'. */
    if (maxFps == 0 || 30 < maxFps) {
        CLOGW("WRN(%s): Invalid max fps value(%d)", __FUNCTION__, maxFps);
        maxFps = 30;
    }

    /* sensor */
    shot->ctl.sensor.exposureTime = 0;
    shot->ctl.sensor.frameDuration = (1000 * 1000 * 1000) / maxFps;
    shot->ctl.sensor.sensitivity = 0;

    /* flash */
    shot->ctl.flash.flashMode = ::CAM2_FLASH_MODE_OFF;
    shot->ctl.flash.firingPower = 0;
    shot->ctl.flash.firingTime = 0;

    /* hotpixel */
    shot->ctl.hotpixel.mode = (enum processing_mode)0;

    /* demosaic */
    shot->ctl.demosaic.mode = (enum demosaic_processing_mode)0;

    /* noise */
#ifdef USE_NEW_NOISE_REDUCTION_ALGORITHM
    shot->ctl.noise.mode = ::PROCESSING_MODE_FAST;
#else
    shot->ctl.noise.mode = ::PROCESSING_MODE_OFF;
#endif
    shot->ctl.noise.strength = 5;

    /* shading */
    shot->ctl.shading.mode = (enum processing_mode)0;

    /* color */
    shot->ctl.color.mode = ::COLORCORRECTION_MODE_FAST;
    static const float colorTransform[9] = {
        1.0f, 0.f, 0.f,
        0.f, 1.f, 0.f,
        0.f, 0.f, 1.f
    };

    if (getHalVersion() == IS_HAL_VER_3_2) {
        for (size_t i = 0; i < sizeof(colorTransform)/sizeof(colorTransform[0]); i++) {
            shot->ctl.color.transform[i].num = colorTransform[i] * COMMON_DENOMINATOR;
            shot->ctl.color.transform[i].den = COMMON_DENOMINATOR;
        }
    } else {
        memcpy(shot->ctl.color.transform, colorTransform, sizeof(colorTransform));
    }

    /* tonemap */
    shot->ctl.tonemap.mode = ::TONEMAP_MODE_FAST;
    static const float tonemapCurve[4] = {
        0.f, 0.f,
        1.f, 1.f
    };

    int tonemapCurveSize = sizeof(tonemapCurve);
    int sizeOfCurve = sizeof(shot->ctl.tonemap.curveRed) / sizeof(shot->ctl.tonemap.curveRed[0]);

    for (int i = 0; i < sizeOfCurve; i ++) {
        memcpy(&(shot->ctl.tonemap.curveRed[i]),   tonemapCurve, tonemapCurveSize);
        memcpy(&(shot->ctl.tonemap.curveGreen[i]), tonemapCurve, tonemapCurveSize);
        memcpy(&(shot->ctl.tonemap.curveBlue[i]),  tonemapCurve, tonemapCurveSize);
    }

    /* edge */
#ifdef USE_NEW_NOISE_REDUCTION_ALGORITHM
    shot->ctl.edge.mode = ::PROCESSING_MODE_FAST;
#else
    shot->ctl.edge.mode = ::PROCESSING_MODE_OFF;
#endif
    shot->ctl.edge.strength = 5;

    /* scaler
     * Max Picture Size == Max Sensor Size - Sensor Margin
     */
    if (m_setParamCropRegion(0,
                             m_staticInfo->maxPictureW, m_staticInfo->maxPictureH,
                             m_staticInfo->maxPreviewW, m_staticInfo->maxPreviewH
                             ) != NO_ERROR) {
        ALOGE("ERR(%s):m_setZoom() fail", __FUNCTION__);
    }

    /* jpeg */
    shot->ctl.jpeg.quality = 96;
    shot->ctl.jpeg.thumbnailSize[0] = m_staticInfo->maxThumbnailW;
    shot->ctl.jpeg.thumbnailSize[1] = m_staticInfo->maxThumbnailH;
    shot->ctl.jpeg.thumbnailQuality = 100;
    shot->ctl.jpeg.gpsCoordinates[0] = 0;
    shot->ctl.jpeg.gpsCoordinates[1] = 0;
    shot->ctl.jpeg.gpsCoordinates[2] = 0;
    shot->ctl.jpeg.gpsProcessingMethod = 0;
    shot->ctl.jpeg.gpsTimestamp = 0L;
    shot->ctl.jpeg.orientation = 0L;

    /* stats */
    shot->ctl.stats.faceDetectMode = ::FACEDETECT_MODE_OFF;
    shot->ctl.stats.histogramMode = ::STATS_MODE_OFF;
    shot->ctl.stats.sharpnessMapMode = ::STATS_MODE_OFF;

    /* aa */
    shot->ctl.aa.captureIntent = ::AA_CAPTURE_INTENT_CUSTOM;
    shot->ctl.aa.mode = ::AA_CONTROL_AUTO;
    shot->ctl.aa.effectMode = ::AA_EFFECT_OFF;
    shot->ctl.aa.sceneMode = ::AA_SCENE_MODE_FACE_PRIORITY;
    shot->ctl.aa.videoStabilizationMode = (enum aa_videostabilization_mode)0;

    /* default metering is center */
    shot->ctl.aa.aeMode = ::AA_AEMODE_CENTER;
    shot->ctl.aa.aeRegions[0] = 0;
    shot->ctl.aa.aeRegions[1] = 0;
    shot->ctl.aa.aeRegions[2] = 0;
    shot->ctl.aa.aeRegions[3] = 0;
    shot->ctl.aa.aeRegions[4] = 1000;
    shot->ctl.aa.aeLock = ::AA_AE_LOCK_OFF;
#if defined(USE_SUBDIVIDED_EV)
    shot->ctl.aa.aeExpCompensation = 0; /* 21 is middle */
#else
    shot->ctl.aa.aeExpCompensation = 5; /* 5 is middle */
#endif
    shot->ctl.aa.aeTargetFpsRange[0] = minFps;
    shot->ctl.aa.aeTargetFpsRange[1] = maxFps;

    shot->ctl.aa.aeAntibandingMode = ::AA_AE_ANTIBANDING_AUTO;
    shot->ctl.aa.vendor_aeflashMode = ::AA_FLASHMODE_OFF;
    shot->ctl.aa.awbMode = ::AA_AWBMODE_WB_AUTO;
    shot->ctl.aa.awbLock = ::AA_AWB_LOCK_OFF;
    shot->ctl.aa.afMode = ::AA_AFMODE_OFF;
    shot->ctl.aa.afRegions[0] = 0;
    shot->ctl.aa.afRegions[1] = 0;
    shot->ctl.aa.afRegions[2] = 0;
    shot->ctl.aa.afRegions[3] = 0;
    shot->ctl.aa.afRegions[4] = 1000;
    shot->ctl.aa.afTrigger = (enum aa_af_trigger)0;
    shot->ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
    shot->ctl.aa.vendor_isoValue = 0;

    /* 2. dm */

    /* 3. utrl */
#ifdef SAMSUNG_COMPANION
    m_metadata.shot.uctl.companionUd.drc_mode = COMPANION_DRC_OFF;
    m_metadata.shot.uctl.companionUd.paf_mode = COMPANION_PAF_OFF;
    m_metadata.shot.uctl.companionUd.wdr_mode = COMPANION_WDR_OFF;
#endif
#ifdef USE_FW_ZOOMRATIO
    m_metadata.shot.uctl.zoomRatio = 1.00f;
#else
    m_metadata.shot.uctl.zoomRatio = 0;
#endif


    /* 4. udm */

    /* 5. magicNumber */
    shot->magicNumber = SHOT_MAGIC_NUMBER;

    setMetaSetfile(&m_metadata, 0x0);

    /* user request */
    m_metadata.drc_bypass = 1;
    m_metadata.dis_bypass = 1;
    m_metadata.dnr_bypass = 1;
    m_metadata.fd_bypass  = 1;
}

status_t ExynosCameraParameters::duplicateCtrlMetadata(void *buf)
{
    if (buf == NULL) {
        CLOGE("ERR: buf is NULL");
        return BAD_VALUE;
    }

    struct camera2_shot_ext *meta_shot_ext = (struct camera2_shot_ext *)buf;
    memcpy(&meta_shot_ext->shot.ctl, &m_metadata.shot.ctl, sizeof(struct camera2_ctl));
#ifdef SAMSUNG_COMPANION
    meta_shot_ext->shot.uctl.companionUd.wdr_mode = getRTHdr();
    meta_shot_ext->shot.uctl.companionUd.paf_mode = getPaf();
#endif

#ifdef SAMSUNG_DOF
    if(getShotMode() == SHOT_MODE_OUTFOCUS && m_curLensCount > 0) {
        meta_shot_ext->shot.uctl.lensUd.pos = m_metadata.shot.uctl.lensUd.pos;
        meta_shot_ext->shot.uctl.lensUd.posSize = m_metadata.shot.uctl.lensUd.posSize;
        meta_shot_ext->shot.uctl.lensUd.direction = m_metadata.shot.uctl.lensUd.direction;
        meta_shot_ext->shot.uctl.lensUd.slewRate = m_metadata.shot.uctl.lensUd.slewRate;
    }
#endif

#ifdef SAMSUNG_HRM
    if((getCameraId() == CAMERA_ID_BACK) && (m_metadata.shot.uctl.aaUd.hrmInfo.ir_data != 0)
                    && m_flagSensorHRM_Hint) {
        meta_shot_ext->shot.uctl.aaUd.hrmInfo = m_metadata.shot.uctl.aaUd.hrmInfo;
    }
#endif
#ifdef SAMSUNG_LIGHT_IR
    if((getCameraId() == CAMERA_ID_FRONT) && (m_metadata.shot.uctl.aaUd.illuminationInfo.visible_exptime != 0)
                    && m_flagSensorLight_IR_Hint) {
        meta_shot_ext->shot.uctl.aaUd.illuminationInfo = m_metadata.shot.uctl.aaUd.illuminationInfo;
    }
#endif
#ifdef SAMSUNG_GYRO
    if((getCameraId() == CAMERA_ID_BACK) && m_flagSensorGyroHint) {
        meta_shot_ext->shot.uctl.aaUd.gyroInfo = m_metadata.shot.uctl.aaUd.gyroInfo;
    }
#endif
#ifdef USE_FW_ZOOMRATIO
    if(getCameraId() == CAMERA_ID_BACK) {
        meta_shot_ext->shot.uctl.zoomRatio = m_metadata.shot.uctl.zoomRatio;
    }
#endif
#ifdef SAMSUNG_OIS_VDIS
    if(getCameraId() == CAMERA_ID_BACK) {
        meta_shot_ext->shot.uctl.lensUd.oisCoefVal = m_metadata.shot.uctl.lensUd.oisCoefVal;
    }
#endif
    meta_shot_ext->shot.uctl.vtMode = m_metadata.shot.uctl.vtMode;

    return NO_ERROR;
}

status_t ExynosCameraParameters::setFrameSkipCount(int count)
{
    m_frameSkipCounter.setCount(count);

    return NO_ERROR;
}

status_t ExynosCameraParameters::getFrameSkipCount(int *count)
{
    *count = m_frameSkipCounter.getCount();
    m_frameSkipCounter.decCount();

    return NO_ERROR;
}

int ExynosCameraParameters::getFrameSkipCount(void)
{
    return m_frameSkipCounter.getCount();
}

void ExynosCameraParameters::setIsFirstStartFlag(bool flag)
{
    m_flagFirstStart = flag;
}

int ExynosCameraParameters::getIsFirstStartFlag(void)
{
    return m_flagFirstStart;
}

ExynosCameraActivityControl *ExynosCameraParameters::getActivityControl(void)
{
    return m_activityControl;
}

status_t ExynosCameraParameters::setAutoFocusMacroPosition(int autoFocusMacroPosition)
{
    int oldAutoFocusMacroPosition = m_cameraInfo.autoFocusMacroPosition;
    m_cameraInfo.autoFocusMacroPosition = autoFocusMacroPosition;

    m_activityControl->setAutoFocusMacroPosition(oldAutoFocusMacroPosition, autoFocusMacroPosition);

    return NO_ERROR;
}

status_t ExynosCameraParameters::setDisEnable(bool enable)
{
    setMetaBypassDis(&m_metadata, enable == true ? 0 : 1);
    return NO_ERROR;
}

bool ExynosCameraParameters::getDisEnable(void)
{
    return m_metadata.dis_bypass;
}

status_t ExynosCameraParameters::setDrcEnable(bool enable)
{
    setMetaBypassDrc(&m_metadata, enable == true ? 0 : 1);
    return NO_ERROR;
}

bool ExynosCameraParameters::getDrcEnable(void)
{
    return m_metadata.drc_bypass;
}

status_t ExynosCameraParameters::setDnrEnable(bool enable)
{
    setMetaBypassDnr(&m_metadata, enable == true ? 0 : 1);
    return NO_ERROR;
}

bool ExynosCameraParameters::getDnrEnable(void)
{
    return m_metadata.dnr_bypass;
}

status_t ExynosCameraParameters::setFdEnable(bool enable)
{
    setMetaBypassFd(&m_metadata, enable == true ? 0 : 1);
    return NO_ERROR;
}

bool ExynosCameraParameters::getFdEnable(void)
{
    return m_metadata.fd_bypass;
}

status_t ExynosCameraParameters::setFdMode(enum facedetect_mode mode)
{
    setMetaCtlFdMode(&m_metadata, mode);
    return NO_ERROR;
}

status_t ExynosCameraParameters::getFdMeta(bool reprocessing, void *buf)
{
    if (buf == NULL) {
        CLOGE("ERR: buf is NULL");
        return BAD_VALUE;
    }

    struct camera2_shot_ext *meta_shot_ext = (struct camera2_shot_ext *)buf;

    /* disable face detection for reprocessing frame */
    if (reprocessing) {
        meta_shot_ext->fd_bypass = 1;
        meta_shot_ext->shot.ctl.stats.faceDetectMode = ::FACEDETECT_MODE_OFF;
    }

    return NO_ERROR;
}

void ExynosCameraParameters::setFlipHorizontal(int val)
{
    if (val < 0) {
        CLOGE("ERR(%s[%d]): setFlipHorizontal ignored, invalid value(%d)",
                __FUNCTION__, __LINE__, val);
        return;
    }

    m_cameraInfo.flipHorizontal = val;
}

int ExynosCameraParameters::getFlipHorizontal(void)
{
    if (m_cameraInfo.shotMode == SHOT_MODE_FRONT_PANORAMA) {
        return 0;
    } else {
        return m_cameraInfo.flipHorizontal;
    }
}

void ExynosCameraParameters::setFlipVertical(int val)
{
    if (val < 0) {
        CLOGE("ERR(%s[%d]): setFlipVertical ignored, invalid value(%d)",
                __FUNCTION__, __LINE__, val);
        return;
    }

    m_cameraInfo.flipVertical = val;
}

int ExynosCameraParameters::getFlipVertical(void)
{
    return m_cameraInfo.flipVertical;
}

bool ExynosCameraParameters::getCallbackNeedCSC(void)
{
    bool ret = true;

#if 0 /* LSI Code */
    int curShotMode = getShotMode();

    switch (curShotMode) {
    case SHOT_MODE_BEAUTY_FACE:
        ret = false;
        break;
    default:
        break;
    }
#else
    int previewW = 0, previewH = 0;
    int hwPreviewW = 0, hwPreviewH = 0;
    int previewFormat = getPreviewFormat();

    getPreviewSize(&previewW, &previewH);
    getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    if ((previewW == hwPreviewW)&&
        (previewH == hwPreviewH)&&
        (previewFormat == V4L2_PIX_FMT_NV21)) {
        ret = false;
    }
#endif
    return ret;
}

bool ExynosCameraParameters::getCallbackNeedCopy2Rendering(void)
{
    bool ret = false;

#if 0 /* LSI Code */
    int curShotMode = getShotMode();

    switch (curShotMode) {
    case SHOT_MODE_BEAUTY_FACE:
        ret = true;
        break;
    default:
        break;
    }
#else
    int previewW = 0, previewH = 0;

    getPreviewSize(&previewW, &previewH);
    if (previewW * previewH <= 1920*1080)
        ret = true;
#endif
    return ret;
}

#ifdef LLS_CAPTURE
int ExynosCameraParameters::getLLS(struct camera2_shot_ext *shot)
{
#ifdef OIS_CAPTURE
    m_llsValue = shot->shot.dm.stats.vendor_LowLightMode;
#endif

#ifdef RAWDUMP_CAPTURE
    return LLS_LEVEL_ZSL;
#endif

#if (LLS_VALUE_VERSION == 4)

    int ret = shot->shot.dm.stats.vendor_LowLightMode;

    if (m_cameraId == CAMERA_ID_FRONT) {
        if (isLDCapture(m_cameraId)) {
            if (m_flagLLSOn) {
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            } else {
                if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_ZSL_LIKE1) {
                    ret = LLS_LEVEL_ZSL;
                } else {
                    ret = shot->shot.dm.stats.vendor_LowLightMode;
                }
            }
        }
        return ret;
    }

    if (m_flagLLSOn) {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_OFF:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_FLASH_2) {
                ret = LLS_LEVEL_MULTI_MERGE_INDICATOR_2;
            } else if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_FLASH_3) {
                ret = LLS_LEVEL_MULTI_MERGE_INDICATOR_3;
            } else if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_FLASH_4) {
                ret = LLS_LEVEL_MULTI_MERGE_INDICATOR_4;
            } else if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH) {
                ret = LLS_LEVEL_ZSL_LIKE1;
            } else {
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            }
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_MANUAL_ISO
                || shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_SHARPEN_SINGLE) {
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            } else {
                ret = LLS_LEVEL_HIGH;
            }
            break;
        default:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        }
    } else {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_OFF:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH)
                ret = LLS_LEVEL_ZSL_LIKE1;
            else
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_MANUAL_ISO
                || shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_SHARPEN_SINGLE) {
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            } else {
                ret = LLS_LEVEL_HIGH;
            }
            break;
        default:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        }
    }

    ALOGV("DEBUG(%s[%d]):v4 m_flagLLSOn(%d) getFlashMode(%d), LowLightMode(%d)",
        __FUNCTION__, __LINE__, m_flagLLSOn, getFlashMode(), shot->shot.dm.stats.vendor_LowLightMode);

    return ret;

#elif (LLS_VALUE_VERSION == 3)

    int ret = shot->shot.dm.stats.vendor_LowLightMode;

    if (m_cameraId == CAMERA_ID_FRONT) {
        return shot->shot.dm.stats.vendor_LowLightMode;
    }

    if (m_flagLLSOn) {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_OFF:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH)
                ret = LLS_LEVEL_LOW;
            else
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            ret = LLS_LEVEL_HIGH;
            break;
        default:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        }
    } else {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_OFF:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH)
                ret = LLS_LEVEL_ZSL_LIKE;
            else
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            ret = LLS_LEVEL_HIGH;
            break;
        default:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        }
    }

    ALOGV("DEBUG(%s[%d]):m_flagLLSOn(%d) getFlashMode(%d), LowLightMode(%d)",
        __FUNCTION__, __LINE__, m_flagLLSOn, getFlashMode(), shot->shot.dm.stats.vendor_LowLightMode);

    return ret;

#elif (LLS_VALUE_VERSION == 2)

    int ret = shot->shot.dm.stats.vendor_LowLightMode;

    if (m_cameraId == CAMERA_ID_FRONT) {
        return shot->shot.dm.stats.vendor_LowLightMode;
    }

    if (m_flagLLSOn) {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_OFF:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_FLASH)
                ret = LLS_LEVEL_LOW;
            else
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            ret = LLS_LEVEL_HIGH;
            break;
        default:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        }
    } else {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_OFF:
            if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH)
                ret = LLS_LEVEL_ZSL_LIKE;
            else
                ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            ret = LLS_LEVEL_HIGH;
            break;
        default:
            ret = shot->shot.dm.stats.vendor_LowLightMode;
            break;
        }
    }

    ALOGV("DEBUG(%s[%d]):m_flagLLSOn(%d) getFlashMode(%d), LowLightMode(%d)",
        __FUNCTION__, __LINE__, m_flagLLSOn, getFlashMode(), shot->shot.dm.stats.vendor_LowLightMode);

    return ret;

#else /* not defined FLASHED_LLS_CAPTURE */

    int ret = LLS_LEVEL_ZSL;

    switch (getFlashMode()) {
    case FLASH_MODE_OFF:
        if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_LOW
            || shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH)
            ret = LLS_LEVEL_LOW;
        else if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_SIS)
            ret = LLS_LEVEL_SIS;
        break;
    case FLASH_MODE_AUTO:
        if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_LOW)
            ret = LLS_LEVEL_LOW;
        else if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_HIGH)
            ret = LLS_LEVEL_ZSL;
        else if (shot->shot.dm.stats.vendor_LowLightMode == STATE_LLS_LEVEL_SIS)
            ret = LLS_LEVEL_SIS;
        break;
    case FLASH_MODE_ON:
    case FLASH_MODE_TORCH:
    case FLASH_MODE_RED_EYE:
    default:
        ret = LLS_LEVEL_ZSL;
        break;
    }

#if defined(LLS_NOT_USE_SIS_FRONT)
    if ((getCameraId() == CAMERA_ID_FRONT) && (ret == LLS_LEVEL_SIS)) {
        ret = LLS_LEVEL_ZSL;
    }
#endif

    ALOGV("DEBUG(%s[%d]):m_flagLLSOn(%d) getFlashMode(%d), LowLightMode(%d), shotmode(%d)",
        __FUNCTION__, __LINE__, m_flagLLSOn, getFlashMode(), shot->shot.dm.stats.vendor_LowLightMode,getShotMode());

    return ret;
#endif
}


void ExynosCameraParameters::setLLSOn(uint32_t enable)
{
    m_flagLLSOn = enable;
}

bool ExynosCameraParameters::getLLSOn(void)
{
    return m_flagLLSOn;
}

void ExynosCameraParameters::setLLSValue(int value)
{
    m_LLSValue = value;
}

int ExynosCameraParameters::getLLSValue(void)
{
    return m_LLSValue;
}

void ExynosCameraParameters::m_setLLSShotMode()
{
    enum aa_mode mode = AA_CONTROL_USE_SCENE_MODE;
    enum aa_scene_mode sceneMode = AA_SCENE_MODE_LLS;

    setMetaCtlSceneMode(&m_metadata, mode, sceneMode);
}

#ifdef SET_LLS_CAPTURE_SETFILE
void ExynosCameraParameters::setLLSCaptureOn(bool enable)
{
    m_LLSCaptureOn = enable;
}

int ExynosCameraParameters::getLLSCaptureOn()
{
    return m_LLSCaptureOn;
}
#endif

#ifdef LLS_REPROCESSING
void ExynosCameraParameters::setLLSCaptureCount(int count)
{
    m_LLSCaptureCount = count;
}

int ExynosCameraParameters::getLLSCaptureCount()
{
    return m_LLSCaptureCount;
}
#endif
#endif

#ifdef SR_CAPTURE
void ExynosCameraParameters::setSROn(uint32_t enable)
{
    m_flagSRSOn = (enable > 0) ? true : false;
}

bool ExynosCameraParameters::getSROn(void)
{
    return m_flagSRSOn;
}
#endif

#ifdef OIS_CAPTURE
void ExynosCameraParameters::checkOISCaptureMode()
{
    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
    uint32_t lls = getLLSValue();

    if (getCaptureExposureTime() > 0) {
        ALOGD("DEBUG(%s[%d]: zsl-like capture mode off for manual shutter speed", __FUNCTION__, __LINE__);
        setOISCaptureModeOn(false);
        return;
    }

    if ((getRecordingHint() == true) || (getShotMode() > SHOT_MODE_AUTO && getShotMode() != SHOT_MODE_NIGHT)) {
        if((getRecordingHint() == true)
            || (getShotMode() != SHOT_MODE_BEAUTY_FACE && getShotMode() != SHOT_MODE_OUTFOCUS
                && getShotMode() != SHOT_MODE_SELFIE_ALARM && getShotMode() != SHOT_MODE_PRO_MODE)) {
            setOISCaptureModeOn(false);
            ALOGD("DEBUG(%s[%d]: zsl-like capture mode off for special shot", __FUNCTION__, __LINE__);
            return;
        }
    }

#ifdef SR_CAPTURE
    if (getSROn() == true) {
        ALOGD("DEBUG(%s[%d]: zsl-like multi capture mode off (SR mode)", __FUNCTION__, __LINE__);
        setOISCaptureModeOn(false);
            return;
    }
#endif

    if (getSeriesShotMode() == SERIES_SHOT_MODE_NONE && m_flashMgr->getNeedCaptureFlash()) {
        setOISCaptureModeOn(false);
        ALOGD("DEBUG(%s[%d]: zsl-like capture mode off for flash single capture", __FUNCTION__, __LINE__);
        return;
    }

#ifdef FLASHED_LLS_CAPTURE
    if (m_flashMgr->getNeedCaptureFlash() == true && getSeriesShotMode()== SERIES_SHOT_MODE_LLS) {
        ALOGD("DEBUG(%s[%d]: zsl-like flash lls capture mode on, lls(%d)", __FUNCTION__, __LINE__, m_llsValue);
        setOISCaptureModeOn(true);
        return;
    }
#endif

    if (getSeriesShotCount() > 0 && m_llsValue > 0) {
        ALOGD("DEBUG(%s[%d]: zsl-like multi capture mode on, lls(%d)", __FUNCTION__, __LINE__, m_llsValue);
        setOISCaptureModeOn(true);
        return;
    }

    ALOGD("DEBUG(%s[%d]: Low Light value(%d), m_flagLLSOn(%d),getFlashMode(%d)",
        __FUNCTION__, __LINE__, lls, m_flagLLSOn, getFlashMode());

    if (m_flagLLSOn) {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
        case FLASH_MODE_OFF:
            if (lls == LLS_LEVEL_ZSL_LIKE || lls == LLS_LEVEL_LOW || lls == LLS_LEVEL_FLASH
#ifdef SAMSUNG_LLS_DEBLUR
                || lls == LLS_LEVEL_ZSL_LIKE1
                || (lls >= LLS_LEVEL_MULTI_MERGE_2 && lls <= LLS_LEVEL_MULTI_MERGE_INDICATOR_4)
                || lls == LLS_LEVEL_DUMMY
#endif
                ) {
                setOISCaptureModeOn(true);
            }
            break;
        case FLASH_MODE_ON:
        case FLASH_MODE_TORCH:
        case FLASH_MODE_RED_EYE:
            if (lls == LLS_LEVEL_FLASH) {
                setOISCaptureModeOn(true);
            }
            break;
        default:
            setOISCaptureModeOn(false);
            break;
        }
    }else {
        switch (getFlashMode()) {
        case FLASH_MODE_AUTO:
            if ((lls == LLS_LEVEL_ZSL_LIKE || lls == LLS_LEVEL_ZSL_LIKE1)
                && m_flashMgr->getNeedFlash() == false) {
                setOISCaptureModeOn(true);
            }
            break;
        case FLASH_MODE_OFF:
            if (lls == LLS_LEVEL_ZSL_LIKE || lls == LLS_LEVEL_ZSL_LIKE1) {
                setOISCaptureModeOn(true);
            }
            break;
        default:
            setOISCaptureModeOn(false);
            break;
        }
    }
}

void ExynosCameraParameters::setOISCaptureModeOn(bool enable)
{
    m_flagOISCaptureOn = enable;
}

bool ExynosCameraParameters::getOISCaptureModeOn(void)
{
    return m_flagOISCaptureOn;
}
#endif

#ifdef RAWDUMP_CAPTURE
void ExynosCameraParameters::setRawCaptureModeOn(bool enable)
{
    m_flagRawCaptureOn = enable;
}

bool ExynosCameraParameters::getRawCaptureModeOn(void)
{
    return m_flagRawCaptureOn;
}
#endif

#ifdef SAMSUNG_DNG
void ExynosCameraParameters::setDNGCaptureModeOn(bool enable)
{
    m_flagDNGCaptureOn = enable;
}

bool ExynosCameraParameters::getDNGCaptureModeOn(void)
{
    return m_flagDNGCaptureOn;
}

void ExynosCameraParameters::setUseDNGCapture(bool enable)
{
    m_use_DNGCapture = enable;
}

bool ExynosCameraParameters::getUseDNGCapture(void)
{
    return m_use_DNGCapture;
}

void ExynosCameraParameters::setCheckMultiFrame(bool enable)
{
    m_flagMultiFrame = enable;
}

bool ExynosCameraParameters::getCheckMultiFrame(void)
{
    return m_flagMultiFrame;
}
#endif

#ifdef SAMSUNG_LLS_DEBLUR
void ExynosCameraParameters::checkLDCaptureMode()
{
    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
    m_flagLDCaptureMode = MULTI_SHOT_MODE_NONE;
    m_LDCaptureCount = 0;
    uint32_t lls = getLLSValue();
    int zoomLevel = getZoomLevel();
    float zoomRatio = getZoomRatio(zoomLevel) / 1000;

    if (m_cameraId == CAMERA_ID_FRONT &&
        ((getRecordingHint() == true)
        || (getDualMode() == true)
        || (getShotMode() > SHOT_MODE_AUTO && getShotMode() != SHOT_MODE_NIGHT))) {
        ALOGD("DEBUG(%s[%d]: LLS Deblur capture mode off for special shot", __FUNCTION__, __LINE__);
        m_flagLDCaptureMode = MULTI_SHOT_MODE_NONE;
        setLDCaptureCount(0);
        setLDCaptureLLSValue(LLS_LEVEL_ZSL);
        return;
    }

    if (m_cameraId == CAMERA_ID_BACK && zoomRatio >= 3.0f) {
        ALOGD("DEBUG(%s[%d]: LLS Deblur capture mode off. zoomRatio(%f) LLS Value(%d)",
            __FUNCTION__, __LINE__, zoomRatio, getLLSValue());
        m_flagLDCaptureMode = MULTI_SHOT_MODE_NONE;
        setLDCaptureCount(0);
        setLDCaptureLLSValue(LLS_LEVEL_ZSL);
        return;
    }

    if ((getOISCaptureModeOn() && m_cameraId == CAMERA_ID_BACK)
        || m_cameraId == CAMERA_ID_FRONT) {
        if (lls == LLS_LEVEL_MULTI_MERGE_2 || lls == LLS_LEVEL_MULTI_PICK_2
            || lls == LLS_LEVEL_MULTI_MERGE_INDICATOR_2) {
            m_flagLDCaptureMode = MULTI_SHOT_MODE_MULTI1;
            setLDCaptureCount(LD_CAPTURE_COUNT_MULTI1);
        } else if (lls == LLS_LEVEL_MULTI_MERGE_3 || lls == LLS_LEVEL_MULTI_PICK_3
            || lls == LLS_LEVEL_MULTI_MERGE_INDICATOR_3) {
            m_flagLDCaptureMode = MULTI_SHOT_MODE_MULTI2;
            setLDCaptureCount(LD_CAPTURE_COUNT_MULTI2);
        } else if (lls == LLS_LEVEL_MULTI_MERGE_4 || lls == LLS_LEVEL_MULTI_PICK_4
            || lls == LLS_LEVEL_MULTI_MERGE_INDICATOR_4) {
            m_flagLDCaptureMode = MULTI_SHOT_MODE_MULTI3;
            setLDCaptureCount(LD_CAPTURE_COUNT_MULTI3);
        }
#ifdef SAMSUNG_DEBLUR
        else if (lls == LLS_LEVEL_DUMMY) {
            m_flagLDCaptureMode = MULTI_SHOT_MODE_DEBLUR;
            setLDCaptureCount(DEBLUR_CAPTURE_COUNT);
        }
#endif
        else {
            m_flagLDCaptureMode = MULTI_SHOT_MODE_NONE;
            setLDCaptureCount(0);
        }
    }

    setLDCaptureLLSValue(lls);

#ifdef SET_LLS_CAPTURE_SETFILE
    if (m_cameraId == CAMERA_ID_BACK
        && (m_flagLDCaptureMode > MULTI_SHOT_MODE_NONE) && (m_flagLDCaptureMode <= MULTI_SHOT_MODE_MULTI3)) {
        CLOGD("DEBUG(%s[%d]): set LLS Capture mode on. lls value(%d)", __FUNCTION__, __LINE__, getLLSValue());
        setLLSCaptureOn(true);
    }
#endif
}

void ExynosCameraParameters::setLDCaptureMode(int LDMode)
{
    m_flagLDCaptureMode = LDMode;
}

int ExynosCameraParameters::getLDCaptureMode(void)
{
    return m_flagLDCaptureMode;
}

void ExynosCameraParameters::setLDCaptureLLSValue(uint32_t lls)
{
    m_flagLDCaptureLLSValue = lls;
}

int ExynosCameraParameters::getLDCaptureLLSValue(void)
{
    return m_flagLDCaptureLLSValue;
}

void ExynosCameraParameters::setLDCaptureCount(int count)
{
    m_LDCaptureCount = count;
}

int ExynosCameraParameters::getLDCaptureCount(void)
{
    return m_LDCaptureCount;
}
#endif

bool ExynosCameraParameters::setDeviceOrientation(int orientation)
{
    if (orientation < 0 || orientation % 90 != 0) {
        CLOGE("ERR(%s[%d]):Invalid orientation (%d)",
                __FUNCTION__, __LINE__, orientation);
        return false;
    }

    m_cameraInfo.deviceOrientation = orientation;

    /* fd orientation need to be calibrated, according to f/w spec */
    int hwRotation = BACK_ROTATION;

#if 0
    if (this->getCameraId() == CAMERA_ID_FRONT)
        hwRotation = FRONT_ROTATION;
#endif

    int fdOrientation = (orientation + hwRotation) % 360;

    CLOGD("DEBUG(%s[%d]):orientation(%d), hwRotation(%d), fdOrientation(%d)",
                __FUNCTION__, __LINE__, orientation, hwRotation, fdOrientation);

    return true;
}

int ExynosCameraParameters::getDeviceOrientation(void)
{
    return m_cameraInfo.deviceOrientation;
}

int ExynosCameraParameters::getFdOrientation(void)
{
    return (m_cameraInfo.deviceOrientation + BACK_ROTATION) % 360;;
}

void ExynosCameraParameters::getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange)
{
    if (flagReprocessing == true) {
        *setfile = m_setfileReprocessing;
        *yuvRange = m_yuvRangeReprocessing;
    } else {
        *setfile = m_setfile;
        *yuvRange = m_yuvRange;
    }
}

status_t ExynosCameraParameters::checkSetfileYuvRange(void)
{
    int oldSetFile = m_setfile;
    int oldYUVRange = m_yuvRange;

    /* general */
    m_getSetfileYuvRange(false, &m_setfile, &m_yuvRange);

    /* reprocessing */
    m_getSetfileYuvRange(true, &m_setfileReprocessing, &m_yuvRangeReprocessing);

    CLOGD("DEBUG(%s[%d]):m_cameraId(%d) : general[setfile(%d) YUV range(%d)] : reprocesing[setfile(%d) YUV range(%d)]",
        __FUNCTION__, __LINE__,
        m_cameraId,
        m_setfile, m_yuvRange,
        m_setfileReprocessing, m_yuvRangeReprocessing);

    return NO_ERROR;
}

void ExynosCameraParameters::setSetfileYuvRange(void)
{
    /* reprocessing */
    m_getSetfileYuvRange(true, &m_setfileReprocessing, &m_yuvRangeReprocessing);

    ALOGD("DEBUG(%s[%d]):m_cameraId(%d) : general[setfile(%d) YUV range(%d)] : reprocesing[setfile(%d) YUV range(%d)]",
        __FUNCTION__, __LINE__,
        m_cameraId,
        m_setfile, m_yuvRange,
        m_setfileReprocessing, m_yuvRangeReprocessing);

}

void ExynosCameraParameters::m_getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange)
{
    uint32_t currentSetfile = 0;
    uint32_t stateReg = 0;
    int flagYUVRange = YUV_FULL_RANGE;

    unsigned int minFps = 0;
    unsigned int maxFps = 0;
    getPreviewFpsRange(&minFps, &maxFps);

#ifdef SAMSUNG_COMPANION
    if (getRTHdr() == COMPANION_WDR_ON) {
        stateReg |= STATE_REG_RTHDR_ON;
    } else if (getRTHdr() == COMPANION_WDR_AUTO) {
        stateReg |= STATE_REG_RTHDR_AUTO;
    }
#endif

    if (m_isUHDRecordingMode() == true)
        stateReg |= STATE_REG_UHD_RECORDING;

    if (getDualMode() == true) {
        stateReg |= STATE_REG_DUAL_MODE;
        if (getDualRecordingHint() == true)
            stateReg |= STATE_REG_DUAL_RECORDINGHINT;
    } else {
        if (getRecordingHint() == true)
            stateReg |= STATE_REG_RECORDINGHINT;
    }

    if (flagReprocessing == true) {
        stateReg |= STATE_REG_FLAG_REPROCESSING;
    }

#ifdef SET_LLS_CAPTURE_SETFILE
    if (flagReprocessing == true && getLLSCaptureOn())
        stateReg |= STATE_REG_NEED_LLS;
#endif

#ifdef LLS_CAPTURE
    if (getRTHdr() == COMPANION_WDR_AUTO && flagReprocessing == true
         && getLLSValue() == LLS_LEVEL_SHARPEN_SINGLE) {
         stateReg |= STATE_REG_SHARPEN_SINGLE;
     }

    if (flagReprocessing == true && (getLongExposureShotCount() == 0)
        && getLLSValue() == LLS_LEVEL_MANUAL_ISO) {
        stateReg |= STATE_REG_MANUAL_ISO;
    }
#endif

    if (getLongExposureShotCount() > 0 && flagReprocessing == true) {
        stateReg |= STATE_STILL_CAPTURE_LONG;
    }

#ifdef SR_CAPTURE
    int zoomLevel = getZoomLevel();
    float zoomRatio = getZoomRatio(zoomLevel) / 1000;

    if (getRecordingHint() == false && flagReprocessing == true
#ifdef SET_LLS_CAPTURE_SETFILE
        && !getLLSCaptureOn()
#endif
        && (getLLSValue() != LLS_LEVEL_SHARPEN_SINGLE && getLLSValue() != LLS_LEVEL_MANUAL_ISO)
        && (getLongExposureShotCount() == 0)
        ) {
        if (zoomRatio >= 3.0f && zoomRatio < 4.0f) {
            stateReg |= STATE_REG_ZOOM;
            ALOGV("(%s)[%d] : currentSetfile zoom",__func__, __LINE__);
        } else if (zoomRatio >= 4.0f) {
            if (getSROn()) {
                stateReg |= STATE_REG_ZOOM_OUTDOOR;
                ALOGV("(%s)[%d] : currentSetfile zoomoutdoor",__func__, __LINE__);
            } else {
                stateReg |= STATE_REG_ZOOM_INDOOR;
                ALOGV("(%s)[%d] : currentSetfile zoomindoor",__func__, __LINE__);
            }
        }
    }
#endif

    if ((stateReg & STATE_REG_RECORDINGHINT)||
        (stateReg & STATE_REG_UHD_RECORDING)||
        (stateReg & STATE_REG_DUAL_RECORDINGHINT)) {
        if (flagReprocessing == false) {
            flagYUVRange = YUV_LIMITED_RANGE;
        }
    }

    if (m_cameraId == CAMERA_ID_FRONT) {
        int vtMode = getVtMode();

        if (0 < vtMode) {
            switch (vtMode) {
            case 1:
                currentSetfile = ISS_SUB_SCENARIO_FRONT_VT1;
                if(stateReg == STATE_STILL_CAPTURE)
                    currentSetfile = ISS_SUB_SCENARIO_FRONT_VT1_STILL_CAPTURE;
                break;
            case 2:
                currentSetfile = ISS_SUB_SCENARIO_FRONT_VT2;
                break;
            case 4:
                currentSetfile = ISS_SUB_SCENARIO_FRONT_VT4;
                break;
            default:
                currentSetfile = ISS_SUB_SCENARIO_FRONT_VT2;
                break;
            }
        } else if (getIntelligentMode() == 1) {
            currentSetfile = ISS_SUB_SCENARIO_FRONT_SMART_STAY;
        } else if (getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
            currentSetfile = ISS_SUB_SCENARIO_FRONT_PANORAMA;
        } else {
            switch(stateReg) {
                case STATE_STILL_PREVIEW:
#if defined(FRONT_CAMERA_USE_SAMSUNG_COMPANION)
                    if (!getUseCompanion())
                        currentSetfile = ISS_SUB_SCENARIO_FRONT_C2_OFF_STILL_PREVIEW;
                    else
#endif
                        currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW;
                    break;

                case STATE_STILL_PREVIEW_WDR_ON:
                    currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW_WDR_ON;
                    break;

                case STATE_STILL_PREVIEW_WDR_AUTO:
                    currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW_WDR_AUTO;
                    break;

                case STATE_VIDEO:
#if defined(FRONT_CAMERA_USE_SAMSUNG_COMPANION)
                    if (!getUseCompanion())
                        currentSetfile = ISS_SUB_SCENARIO_FRONT_C2_OFF_VIDEO;
                    else
#endif
                        currentSetfile = ISS_SUB_SCENARIO_VIDEO;
                    break;

                case STATE_VIDEO_WDR_ON:
                case STATE_UHD_VIDEO_WDR_ON:
                    currentSetfile = ISS_SUB_SCENARIO_VIDEO_WDR_ON;
                    break;

                case STATE_VIDEO_WDR_AUTO:
                case STATE_UHD_VIDEO_WDR_AUTO:
                    currentSetfile = ISS_SUB_SCENARIO_VIDEO_WDR_AUTO;
                    break;

                case STATE_STILL_CAPTURE:
                case STATE_VIDEO_CAPTURE:
                case STATE_UHD_PREVIEW_CAPTURE:
                case STATE_UHD_VIDEO_CAPTURE:
#if defined(FRONT_CAMERA_USE_SAMSUNG_COMPANION)
                    if (!getUseCompanion())
                        currentSetfile = ISS_SUB_SCENARIO_FRONT_C2_OFF_STILL_CAPTURE;
                    else
#endif
                    {
                        if(getHalVersion() == IS_HAL_VER_3_2) {
                            currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW;
                        } else {
                            currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE;
                        }
                    }
                    break;

                case STATE_STILL_CAPTURE_WDR_ON:
                case STATE_VIDEO_CAPTURE_WDR_ON:
                case STATE_UHD_PREVIEW_CAPTURE_WDR_ON:
                case STATE_UHD_VIDEO_CAPTURE_WDR_ON:
                    currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON;
                    break;

                case STATE_STILL_CAPTURE_WDR_AUTO:
                case STATE_VIDEO_CAPTURE_WDR_AUTO:
                case STATE_UHD_PREVIEW_CAPTURE_WDR_AUTO:
                case STATE_UHD_VIDEO_CAPTURE_WDR_AUTO:
                    currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO;
                    break;

                case STATE_DUAL_STILL_PREVIEW:
                case STATE_DUAL_STILL_CAPTURE:
                case STATE_DUAL_VIDEO_CAPTURE:
                    currentSetfile = ISS_SUB_SCENARIO_DUAL_STILL;
                    break;

                case STATE_DUAL_VIDEO:
                    currentSetfile = ISS_SUB_SCENARIO_DUAL_VIDEO;
                    break;

                case STATE_UHD_PREVIEW:
                case STATE_UHD_VIDEO:
                    currentSetfile = ISS_SUB_SCENARIO_UHD_30FPS;
                    break;

                default:
                    ALOGD("(%s)can't define senario of setfile.(0x%4x)",__func__, stateReg);
                    break;
            }
        }
    } else {
        switch(stateReg) {
            case STATE_STILL_PREVIEW:
                currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW;
                break;

            case STATE_STILL_PREVIEW_WDR_ON:
                currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW_WDR_ON;
                break;

            case STATE_STILL_PREVIEW_WDR_AUTO:
                currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW_WDR_AUTO;
                break;

            case STATE_STILL_CAPTURE:
            case STATE_VIDEO_CAPTURE:
            case STATE_DUAL_STILL_CAPTURE:
            case STATE_DUAL_VIDEO_CAPTURE:
            case STATE_UHD_PREVIEW_CAPTURE:
            case STATE_UHD_VIDEO_CAPTURE:
                if(getHalVersion() == IS_HAL_VER_3_2) {
                    currentSetfile = ISS_SUB_SCENARIO_STILL_PREVIEW;
                } else {
                    currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE;
                }
                break;

            case STATE_STILL_CAPTURE_WDR_ON:
            case STATE_VIDEO_CAPTURE_WDR_ON:
            case STATE_UHD_PREVIEW_CAPTURE_WDR_ON:
            case STATE_UHD_VIDEO_CAPTURE_WDR_ON:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON;
                break;

            case STATE_STILL_CAPTURE_WDR_AUTO:
            case STATE_VIDEO_CAPTURE_WDR_AUTO:
            case STATE_UHD_PREVIEW_CAPTURE_WDR_AUTO:
            case STATE_UHD_VIDEO_CAPTURE_WDR_AUTO:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO;
                break;

            case STATE_VIDEO:
                if (30 < minFps && 30 < maxFps) {
                    if (300 == minFps && 300 == maxFps) {
                        currentSetfile = ISS_SUB_SCENARIO_WVGA_300FPS;
                    } else if (60 == minFps && 60 == maxFps) {
                        currentSetfile = ISS_SUB_SCENARIO_FHD_60FPS;
                    } else if (240 == minFps && 240 == maxFps) {
                        currentSetfile = ISS_SUB_SCENARIO_FHD_240FPS;
                    } else {
                        currentSetfile = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
                    }
                } else {
                    currentSetfile = ISS_SUB_SCENARIO_VIDEO;
                }
                break;

            case STATE_VIDEO_WDR_ON:
                currentSetfile = ISS_SUB_SCENARIO_VIDEO_WDR_ON;
                break;

            case STATE_VIDEO_WDR_AUTO:
                currentSetfile = ISS_SUB_SCENARIO_VIDEO_WDR_AUTO;
                break;

            case STATE_DUAL_VIDEO:
                currentSetfile = ISS_SUB_SCENARIO_DUAL_VIDEO;
                break;

            case STATE_DUAL_STILL_PREVIEW:
                currentSetfile = ISS_SUB_SCENARIO_DUAL_STILL;
                break;

            case STATE_UHD_PREVIEW:
            case STATE_UHD_VIDEO:
                currentSetfile = ISS_SUB_SCENARIO_UHD_30FPS;
                break;

            case STATE_UHD_PREVIEW_WDR_ON:
            case STATE_UHD_VIDEO_WDR_ON:
                currentSetfile = ISS_SUB_SCENARIO_UHD_30FPS_WDR_ON;
                break;

#ifdef SR_CAPTURE
            case STATE_STILL_CAPTURE_ZOOM:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_ZOOM;
                break;
            case STATE_STILL_CAPTURE_ZOOM_OUTDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_ZOOM_OUTDOOR;
                break;
            case STATE_STILL_CAPTURE_ZOOM_INDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_ZOOM_INDOOR;
                break;
            case STATE_STILL_CAPTURE_WDR_ON_ZOOM:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON_ZOOM;
                break;
            case STATE_STILL_CAPTURE_WDR_ON_ZOOM_OUTDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON_ZOOM_OUTDOOR;
                break;
            case STATE_STILL_CAPTURE_WDR_ON_ZOOM_INDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON_ZOOM_INDOOR;
                break;
            case STATE_STILL_CAPTURE_WDR_AUTO_ZOOM:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO_ZOOM;
                break;
            case STATE_STILL_CAPTURE_WDR_AUTO_ZOOM_OUTDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO_ZOOM_OUTDOOR;
                break;
            case STATE_STILL_CAPTURE_WDR_AUTO_ZOOM_INDOOR:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO_ZOOM_INDOOR;
                break;
#endif
#ifdef SET_LLS_CAPTURE_SETFILE
#if (LLS_SETFILE_VERSION == 2)
            case STATE_STILL_CAPTURE_LLS:
                currentSetfile = ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE;
                break;
            case STATE_VIDEO_CAPTURE_WDR_ON_LLS:
                currentSetfile = ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE_WDR_ON;
                break;
            case STATE_STILL_CAPTURE_WDR_AUTO_LLS:
                currentSetfile = ISS_SUB_SCENARIO_MERGED_STILL_CAPTURE_WDR_AUTO;
                break;
#else
            case STATE_STILL_CAPTURE_LLS:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_LLS;
#ifdef LLS_REPROCESSING
                switch(getLLSCaptureCount()) {
                    case 1:
                        currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE;
                        break;
                    default:
                        break;
                }
#endif
                break;
            case STATE_VIDEO_CAPTURE_WDR_ON_LLS:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON_LLS;
#ifdef LLS_REPROCESSING
                switch(getLLSCaptureCount()) {
                    case 1:
                        currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_ON;
                        break;
                    default:
                        break;
                }
#endif
                break;
            case STATE_STILL_CAPTURE_WDR_AUTO_LLS:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_LLS;
#ifdef LLS_REPROCESSING
                switch(getLLSCaptureCount()) {
                    case 1:
                        currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_WDR_AUTO;
                        break;
                    default:
                        break;
                }
#endif
                break;
#endif
#endif
            case STATE_STILL_CAPTURE_WDR_AUTO_SHARPEN:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_SHARPEN;
                break;
            case STATE_STILL_CAPTURE_LONG:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_LONG;
                break;
            case STATE_STILL_CAPTURE_MANUAL_ISO:
                currentSetfile = ISS_SUB_SCENARIO_STILL_CAPTURE_MANUAL_ISO;
                break;
            default:
                ALOGD("(%s)can't define senario of setfile.(0x%4x)",__func__, stateReg);
                break;
        }
    }
#if 0
    ALOGD("(%s)[%d] : ===============================================================================",__func__, __LINE__);
    ALOGD("(%s)[%d] : CurrentState(0x%4x)",__func__, __LINE__, stateReg);
    ALOGD("(%s)[%d] : getRTHdr()(%d)",__func__, __LINE__, getRTHdr());
    ALOGD("(%s)[%d] : getRecordingHint()(%d)",__func__, __LINE__, getRecordingHint());
    ALOGD("(%s)[%d] : m_isUHDRecordingMode()(%d)",__func__, __LINE__, m_isUHDRecordingMode());
    ALOGD("(%s)[%d] : getDualMode()(%d)",__func__, __LINE__, getDualMode());
    ALOGD("(%s)[%d] : getDualRecordingHint()(%d)",__func__, __LINE__, getDualRecordingHint());
    ALOGD("(%s)[%d] : flagReprocessing(%d)",__func__, __LINE__, flagReprocessing);
    ALOGD("(%s)[%d] : ===============================================================================",__func__, __LINE__);
    ALOGD("(%s)[%d] : currentSetfile(%d)",__func__, __LINE__, currentSetfile);
    ALOGD("(%s)[%d] : flagYUVRange(%d)",__func__, __LINE__, flagYUVRange);
    ALOGD("(%s)[%d] : ===============================================================================",__func__, __LINE__);
#else
    ALOGD("(%s)[%d] : CurrentState (0x%4x), currentSetfile(%d)",__func__, __LINE__, stateReg, currentSetfile);
#endif

done:
    *setfile = currentSetfile;
    *yuvRange = flagYUVRange;
}

void ExynosCameraParameters::setUseDynamicBayer(bool enable)
{
    m_useDynamicBayer = enable;
}

bool ExynosCameraParameters::getUseDynamicBayer(void)
{
    return m_useDynamicBayer;
}

void ExynosCameraParameters::setUseDynamicBayerVideoSnapShot(bool enable)
{
    m_useDynamicBayerVideoSnapShot = enable;
}

bool ExynosCameraParameters::getUseDynamicBayerVideoSnapShot(void)
{
    return m_useDynamicBayerVideoSnapShot;
}

void ExynosCameraParameters::setUseDynamicScc(bool enable)
{
    m_useDynamicScc = enable;
}

bool ExynosCameraParameters::getUseDynamicScc(void)
{
    bool dynamicScc = m_useDynamicScc;
    bool reprocessing = isReprocessing();

    if (getRecordingHint() == true && reprocessing == false)
        dynamicScc = false;

    return dynamicScc;
}

void ExynosCameraParameters::setUseFastenAeStable(bool enable)
{
    m_useFastenAeStable = enable;
}

bool ExynosCameraParameters::getUseFastenAeStable(void)
{
    return m_useFastenAeStable;
}

#ifdef SAMSUNG_LLV
void ExynosCameraParameters::setLLV(bool enable)
{
    m_isLLVOn = enable;
}

bool ExynosCameraParameters::getLLV(void)
{
    bool isSizeSupported = true;
    int videoW, videoH;

    getVideoSize(&videoW, &videoH);
    if(videoW > 1920 || videoH > 1080)
        isSizeSupported = false;
    else {
        uint32_t minFPS = 0;
        uint32_t maxFPS = 0;
        getPreviewFpsRange(&minFPS, &maxFPS);
        if(minFPS > 30 || maxFPS > 30)
            isSizeSupported = false;
    }

    return (m_isLLVOn && isSizeSupported);
}
#endif

status_t ExynosCameraParameters::calcPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;

    int previewW = 0, previewH = 0, previewFormat = 0;
    int hwPreviewW = 0, hwPreviewH = 0, hwPreviewFormat = 0;
    previewFormat = getPreviewFormat();
    hwPreviewFormat = getHwPreviewFormat();

    getHwPreviewSize(&hwPreviewW, &hwPreviewH);
#ifdef SUPPORT_SW_VDIS
    if(isSWVdisMode())
        m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);
#endif /*SUPPORT_SW_VDIS*/
    getPreviewSize(&previewW, &previewH);

    ret = getCropRectAlign(hwPreviewW, hwPreviewH,
                           previewW, previewH,
                           &cropX, &cropY,
                           &cropW, &cropH,
                           CAMERA_MAGIC_ALIGN, 2,
                           0, 1);

    srcRect->x = cropX;
    srcRect->y = cropY;
    srcRect->w = cropW;
    srcRect->h = cropH;
    if (getHalVersion() == IS_HAL_VER_3_2) {
        srcRect->fullW = hwPreviewW;
        srcRect->fullH = hwPreviewH;
    } else {
        srcRect->fullW = cropW;
        srcRect->fullH = cropH;
    }
    srcRect->colorFormat = hwPreviewFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = previewW;
    dstRect->h = previewH;
    dstRect->fullW = previewW;
    dstRect->fullH = previewH;
    dstRect->colorFormat = previewFormat;

    return NO_ERROR;
}

status_t ExynosCameraParameters::calcHighResolutionPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;

    int previewW = 0, previewH = 0, previewFormat = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    previewFormat = getPreviewFormat();
    pictureFormat = getPictureFormat();

    if (isOwnScc(m_cameraId) == true)
        getPictureSize(&pictureW, &pictureH);
    else
        getHwPictureSize(&pictureW, &pictureH);
    getPreviewSize(&previewW, &previewH);

    srcRect->x = 0;
    srcRect->y = 0;
    srcRect->w = pictureW;
    srcRect->h = pictureH;
    srcRect->fullW = pictureW;
    srcRect->fullH = pictureH;
    srcRect->colorFormat = pictureFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = previewW;
    dstRect->h = previewH;
    dstRect->fullW = previewW;
    dstRect->fullH = previewH;
    dstRect->colorFormat = previewFormat;

    return NO_ERROR;
}

status_t ExynosCameraParameters::calcRecordingGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;

    int hwPreviewW = 0, hwPreviewH = 0, hwPreviewFormat = 0;
    int videoW = 0, videoH = 0, videoFormat = 0;
    float zoomRatio = getZoomRatio(0) / 1000;

    hwPreviewFormat = getHwPreviewFormat();
    videoFormat     = getVideoFormat();

    getHwPreviewSize(&hwPreviewW, &hwPreviewH);
#ifdef SUPPORT_SW_VDIS
    if(isSWVdisMode())
        m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);
#endif /*SUPPORT_SW_VDIS*/
    getVideoSize(&videoW, &videoH);

    if (hwPreviewW < videoW || hwPreviewH < videoH) {
        cropW = hwPreviewW;
        cropH = hwPreviewH;
    } else  {
        ret = getCropRectAlign(hwPreviewW, hwPreviewH,
                         videoW, videoH,
                         &cropX, &cropY,
                         &cropW, &cropH,
                         2, 2,
                         0, zoomRatio);
    }

    srcRect->x = cropX;
    srcRect->y = cropY;
    srcRect->w = cropW;
    srcRect->h = cropH;
    srcRect->fullW = hwPreviewW;
    srcRect->fullH = hwPreviewH;
    srcRect->colorFormat = hwPreviewFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = videoW;
    dstRect->h = videoH;
    dstRect->fullW = videoW;
    dstRect->fullH = videoH;
    dstRect->colorFormat = videoFormat;

    return NO_ERROR;
}

status_t ExynosCameraParameters::calcPictureRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int hwSensorW = 0, hwSensorH = 0;
    int hwPictureW = 0, hwPictureH = 0, hwPictureFormat = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    int previewW = 0, previewH = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;

    int zoomLevel = 0;
    int bayerFormat = getBayerFormat();
    float zoomRatio = 1.0f;

    /* TODO: check state ready for start */
    pictureFormat = getPictureFormat();
    zoomLevel = getZoomLevel();
    getHwPictureSize(&hwPictureW, &hwPictureH);
    getPictureSize(&pictureW, &pictureH);

    getHwSensorSize(&hwSensorW, &hwSensorH);
    getPreviewSize(&previewW, &previewH);

    zoomRatio = getZoomRatio(zoomLevel) / 1000;
    /* TODO: get crop size from ctlMetadata */
    ret = getCropRectAlign(hwSensorW, hwSensorH,
                     previewW, previewH,
                     &cropX, &cropY,
                     &cropW, &cropH,
                     CAMERA_MAGIC_ALIGN, 2,
                     zoomLevel, zoomRatio);

    zoomRatio = getZoomRatio(0) / 1000;
    ret = getCropRectAlign(cropW, cropH,
                     pictureW, pictureH,
                     &crop_crop_x, &crop_crop_y,
                     &crop_crop_w, &crop_crop_h,
                     2, 2,
                     0, zoomRatio);

    ALIGN_UP(crop_crop_x, 2);
    ALIGN_UP(crop_crop_y, 2);

#if 0
    ALOGD("DEBUG(%s):hwSensorSize (%dx%d), previewSize (%dx%d)",
            __FUNCTION__, hwSensorW, hwSensorH, previewW, previewH);
    ALOGD("DEBUG(%s):hwPictureSize (%dx%d), pictureSize (%dx%d)",
            __FUNCTION__, hwPictureW, hwPictureH, pictureW, pictureH);
    ALOGD("DEBUG(%s):size cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            __FUNCTION__, cropX, cropY, cropW, cropH, zoomLevel);
    ALOGD("DEBUG(%s):size2 cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            __FUNCTION__, crop_crop_x, crop_crop_y, crop_crop_w, crop_crop_h, zoomLevel);
    ALOGD("DEBUG(%s):size pictureFormat = 0x%x, JPEG_INPUT_COLOR_FMT = 0x%x",
            __FUNCTION__, pictureFormat, JPEG_INPUT_COLOR_FMT);
#endif

    srcRect->x = crop_crop_x;
    srcRect->y = crop_crop_y;
    srcRect->w = crop_crop_w;
    srcRect->h = crop_crop_h;
    srcRect->fullW = cropW;
    srcRect->fullH = cropH;
    srcRect->colorFormat = pictureFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = pictureW;
    dstRect->h = pictureH;
    dstRect->fullW = pictureW;
    dstRect->fullH = pictureH;
    dstRect->colorFormat = JPEG_INPUT_COLOR_FMT;

    return NO_ERROR;
}

status_t ExynosCameraParameters::calcPictureRect(int originW, int originH, ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;

    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;
    float zoomRatio = getZoomRatio(0) / 1000;
#if 0
    int zoom = 0;
    int bayerFormat = CAMERA_BAYER_FORMAT;
#endif
    /* TODO: check state ready for start */
    pictureFormat = getPictureFormat();
    getPictureSize(&pictureW, &pictureH);

    /* TODO: get crop size from ctlMetadata */
    ret = getCropRectAlign(originW, originH,
                     pictureW, pictureH,
                     &crop_crop_x, &crop_crop_y,
                     &crop_crop_w, &crop_crop_h,
                     2, 2,
                     0, zoomRatio);

    ALIGN_UP(crop_crop_x, 2);
    ALIGN_UP(crop_crop_y, 2);

#if 0
    CLOGD("DEBUG(%s):originSize (%dx%d) pictureSize (%dx%d)",
            __FUNCTION__, originW, originH, pictureW, pictureH);
    CLOGD("DEBUG(%s):size2 cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            __FUNCTION__, crop_crop_x, crop_crop_y, crop_crop_w, crop_crop_h, zoom);
    CLOGD("DEBUG(%s):size pictureFormat = 0x%x, JPEG_INPUT_COLOR_FMT = 0x%x",
            __FUNCTION__, pictureFormat, JPEG_INPUT_COLOR_FMT);
#endif

    srcRect->x = crop_crop_x;
    srcRect->y = crop_crop_y;
    srcRect->w = crop_crop_w;
    srcRect->h = crop_crop_h;
    srcRect->fullW = originW;
    srcRect->fullH = originH;
    srcRect->colorFormat = pictureFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = pictureW;
    dstRect->h = pictureH;
    dstRect->fullW = pictureW;
    dstRect->fullH = pictureH;
    dstRect->colorFormat = JPEG_INPUT_COLOR_FMT;

    return NO_ERROR;
}

status_t ExynosCameraParameters::getPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int hwBnsW   = 0;
    int hwBnsH   = 0;
    int hwBcropW = 0;
    int hwBcropH = 0;
    int zoomLevel = 0;
    float zoomRatio = 1.00f;
    int sizeList[SIZE_LUT_INDEX_END];
    int hwSensorMarginW = 0;
    int hwSensorMarginH = 0;
    float bnsRatio = 0;

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_staticInfo->previewSizeLut == NULL
        || m_staticInfo->previewSizeLutMax <= m_cameraInfo.previewSizeRatioId
        || (getUsePureBayerReprocessing() == false &&
            m_cameraInfo.pictureSizeRatioId != m_cameraInfo.previewSizeRatioId)
        || m_getPreviewSizeList(sizeList) != NO_ERROR)
        return calcPreviewBayerCropSize(srcRect, dstRect);

    /* use LUT */
    hwBnsW = sizeList[BNS_W];
    hwBnsH = sizeList[BNS_H];
    hwBcropW = sizeList[BCROP_W];
    hwBcropH = sizeList[BCROP_H];

    if (getRecordingHint() == true) {
        if (m_cameraInfo.previewSizeRatioId != m_cameraInfo.videoSizeRatioId) {
            ALOGV("WARN(%s):preview ratioId(%d) != videoRatioId(%d), use previewRatioId",
                __FUNCTION__, m_cameraInfo.previewSizeRatioId, m_cameraInfo.videoSizeRatioId);
        }
    }

    int curBnsW = 0, curBnsH = 0;
    getBnsSize(&curBnsW, &curBnsH);
    if (SIZE_RATIO(curBnsW, curBnsH) != SIZE_RATIO(hwBnsW, hwBnsH))
        ALOGW("ERROR(%s[%d]): current BNS size(%dx%d) is NOT same with Hw BNS size(%dx%d)",
                __FUNCTION__, __LINE__, curBnsW, curBnsH, hwBnsW, hwBnsH);

    zoomLevel = getZoomLevel();
    zoomRatio = getZoomRatio(zoomLevel) / 1000;

    /* Skip to calculate the crop size with zoom level
     * Condition 1 : High-speed camcording D-zoom with External Scaler
     * Condition 2 : HAL3 (Service calculates the crop size by itself
     */
    int fastFpsMode = getFastFpsMode();
    if ((fastFpsMode > CONFIG_MODE::HIGHSPEED_60 &&
        fastFpsMode < CONFIG_MODE::MAX &&
        getZoomPreviewWIthScaler() == true) ||
        getHalVersion() == IS_HAL_VER_3_2) {
        ALOGV("DEBUG(%s[%d]):hwBnsSize (%dx%d), hwBcropSize(%dx%d), fastFpsMode(%d)",
                __FUNCTION__, __LINE__,
                hwBnsW, hwBnsH,
                hwBcropW, hwBcropH,
                fastFpsMode);
    } else {
#if defined(SCALER_MAX_SCALE_UP_RATIO)
        /*
         * After dividing float & casting int,
         * zoomed size can be smaller too much.
         * so, when zoom until max, ceil up about floating point.
         */
        if (ALIGN_UP((int)((float)hwBcropW / zoomRatio), CAMERA_BCROP_ALIGN) * SCALER_MAX_SCALE_UP_RATIO < hwBcropW ||
            ALIGN_UP((int)((float)hwBcropH / zoomRatio), 2) * SCALER_MAX_SCALE_UP_RATIO < hwBcropH) {
            hwBcropW = ALIGN_UP((int)ceil((float)hwBcropW / zoomRatio), CAMERA_BCROP_ALIGN);
            hwBcropH = ALIGN_UP((int)ceil((float)hwBcropH / zoomRatio), 2);
        } else
#endif
        {
            hwBcropW = ALIGN_UP((int)((float)hwBcropW / zoomRatio), CAMERA_BCROP_ALIGN);
            hwBcropH = ALIGN_UP((int)((float)hwBcropH / zoomRatio), 2);
        }
    }

    /* Re-calculate the BNS size for removing Sensor Margin */
    getSensorMargin(&hwSensorMarginW, &hwSensorMarginH);
    m_adjustSensorMargin(&hwSensorMarginW, &hwSensorMarginH);

    hwBnsW = hwBnsW - hwSensorMarginW;
    hwBnsH = hwBnsH - hwSensorMarginH;

    /* src */
    srcRect->x = 0;
    srcRect->y = 0;
    srcRect->w = hwBnsW;
    srcRect->h = hwBnsH;

    if (getHalVersion() == IS_HAL_VER_3_2) {
        int cropRegionX = 0, cropRegionY = 0, cropRegionW = 0, cropRegionH = 0;
        int maxSensorW = 0, maxSensorH = 0;
        float scaleRatioX = 0.0f, scaleRatioY = 0.0f;
        status_t ret = NO_ERROR;

        m_getCropRegion(&cropRegionX, &cropRegionY, &cropRegionW, &cropRegionH);
        getMaxSensorSize(&maxSensorW, &maxSensorH);

        /* 1. Scale down the crop region to adjust with the bcrop input size */
        scaleRatioX = (float) hwBnsW / (float) maxSensorW;
        scaleRatioY = (float) hwBnsH / (float) maxSensorH;
        cropRegionX = (int) (cropRegionX * scaleRatioX);
        cropRegionY = (int) (cropRegionY * scaleRatioY);
        cropRegionW = (int) (cropRegionW * scaleRatioX);
        cropRegionH = (int) (cropRegionH * scaleRatioY);

        if (cropRegionW < 1 || cropRegionH < 1) {
            cropRegionW = hwBnsW;
            cropRegionH = hwBnsH;
        }

        /* 2. Calculate the real crop region with considering the target ratio */
        if ((cropRegionW > hwBcropW) && (cropRegionH > hwBcropH)) {
            dstRect->x = ALIGN_DOWN((cropRegionX + ((cropRegionW - hwBcropW) >> 1)), 2);
            dstRect->y = ALIGN_DOWN((cropRegionY + ((cropRegionH - hwBcropH) >> 1)), 2);
            dstRect->w = hwBcropW;
            dstRect->h = hwBcropH;
        } else {
            ret = getCropRectAlign(cropRegionW, cropRegionH,
                    hwBcropW, hwBcropH,
                    &(dstRect->x), &(dstRect->y),
                    &(dstRect->w), &(dstRect->h),
                    CAMERA_MAGIC_ALIGN, 2,
                    0, 0.0f);
            dstRect->x = ALIGN_DOWN((cropRegionX + dstRect->x), 2);
            dstRect->y = ALIGN_DOWN((cropRegionY + dstRect->y), 2);
        }
    } else {
        if (hwBnsW > hwBcropW) {
            dstRect->x = ALIGN_UP(((hwBnsW - hwBcropW) >> 1), 2);
            dstRect->w = hwBcropW;
        } else {
            dstRect->x = 0;
            dstRect->w = hwBnsW;
        }

        if (hwBnsH > hwBcropH) {
            dstRect->y = ALIGN_UP(((hwBnsH - hwBcropH) >> 1), 2);
            dstRect->h = hwBcropH;
        } else {
            dstRect->y = 0;
            dstRect->h = hwBnsH;
        }
    }

    m_setHwBayerCropRegion(dstRect->w, dstRect->h, dstRect->x, dstRect->y);
#ifdef DEBUG_PERFRAME
    ALOGD("DEBUG(%s):zoomLevel=%d", __FUNCTION__, zoomLevel);
    ALOGD("DEBUG(%s):hwBnsSize (%dx%d), hwBcropSize (%d, %d)(%dx%d)",
            __FUNCTION__, srcRect->w, srcRect->h, dstRect->x, dstRect->y, dstRect->w, dstRect->h);
#endif

    return NO_ERROR;
}

status_t ExynosCameraParameters::calcPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int hwSensorW = 0, hwSensorH = 0;
    int hwPictureW = 0, hwPictureH = 0;
    int pictureW = 0, pictureH = 0;
    int previewW = 0, previewH = 0;
    int hwSensorMarginW = 0, hwSensorMarginH = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
#if 0
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;
    int pictureFormat = 0, hwPictureFormat = 0;
#endif
    int zoomLevel = 0;
    int maxZoomRatio = 0;
    int bayerFormat = getBayerFormat();
    float zoomRatio = getZoomRatio(0) / 1000;

#ifdef DEBUG_RAWDUMP
    if (checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    /* TODO: check state ready for start */
#if 0
    pictureFormat = getPictureFormat();
#endif
    zoomLevel = getZoomLevel();
    maxZoomRatio = getMaxZoomRatio() / 1000;
    getHwPictureSize(&hwPictureW, &hwPictureH);
    getPictureSize(&pictureW, &pictureH);

    getHwSensorSize(&hwSensorW, &hwSensorH);
    getPreviewSize(&previewW, &previewH);
    getSensorMargin(&hwSensorMarginW, &hwSensorMarginH);
    m_adjustSensorMargin(&hwSensorMarginW, &hwSensorMarginH);

    zoomRatio = getZoomRatio(zoomLevel) / 1000;

    hwSensorW -= hwSensorMarginW;
    hwSensorH -= hwSensorMarginH;

    if (getHalVersion() == IS_HAL_VER_3_2) {
        int cropRegionX = 0, cropRegionY = 0, cropRegionW = 0, cropRegionH = 0;
        int maxSensorW = 0, maxSensorH = 0;
        float scaleRatioX = 0.0f, scaleRatioY = 0.0f;

        m_getCropRegion(&cropRegionX, &cropRegionY, &cropRegionW, &cropRegionH);
        getMaxSensorSize(&maxSensorW, &maxSensorH);

        /* 1. Scale down the crop region to adjust with the bcrop input size */
        scaleRatioX = (float) hwSensorW / (float) maxSensorW;
        scaleRatioY = (float) hwSensorH / (float) maxSensorH;
        cropRegionX = (int) (cropRegionX * scaleRatioX);
        cropRegionY = (int) (cropRegionY * scaleRatioY);
        cropRegionW = (int) (cropRegionW * scaleRatioX);
        cropRegionH = (int) (cropRegionH * scaleRatioY);

        if (cropRegionW < 1 || cropRegionH < 1) {
            cropRegionW = hwSensorW;
            cropRegionH = hwSensorH;
        }

        /* 2. Calculate the real crop region with considering the target ratio */
        ret = getCropRectAlign(cropRegionW, cropRegionH,
                previewW, previewH,
                &cropX, &cropY,
                &cropW, &cropH,
                CAMERA_MAGIC_ALIGN, 2,
                0, 0.0f);

        cropX = ALIGN_DOWN((cropRegionX + cropX), 2);
        cropY = ALIGN_DOWN((cropRegionY + cropY), 2);
    } else {
        ret = getCropRectAlign(hwSensorW, hwSensorH,
                previewW, previewH,
                &cropX, &cropY,
                &cropW, &cropH,
                CAMERA_MAGIC_ALIGN, 2,
                zoomLevel, zoomRatio);

        cropX = ALIGN_DOWN(cropX, 2);
        cropY = ALIGN_DOWN(cropY, 2);
        cropW = hwSensorW - (cropX * 2);
        cropH = hwSensorH - (cropY * 2);
    }

    if (getUsePureBayerReprocessing() == false) {
        int pictureCropX = 0, pictureCropY = 0;
        int pictureCropW = 0, pictureCropH = 0;

        zoomLevel = 0;
        zoomRatio = getZoomRatio(zoomLevel) / 1000;

        ret = getCropRectAlign(cropW, cropH,
                pictureW, pictureH,
                &pictureCropX, &pictureCropY,
                &pictureCropW, &pictureCropH,
                CAMERA_MAGIC_ALIGN, 2,
                zoomLevel, zoomRatio);

        pictureCropX = ALIGN_DOWN(pictureCropX, 2);
        pictureCropY = ALIGN_DOWN(pictureCropY, 2);
        pictureCropW = cropW - (pictureCropX * 2);
        pictureCropH = cropH - (pictureCropY * 2);

        if (pictureCropW < pictureW / maxZoomRatio || pictureCropH < pictureH / maxZoomRatio) {
            ALOGW("WRN(%s[%d]): zoom ratio is upto x%d, crop(%dx%d), picture(%dx%d)", __FUNCTION__, __LINE__, maxZoomRatio, cropW, cropH, pictureW, pictureH);
            float src_ratio = 1.0f;
            float dst_ratio = 1.0f;
            /* ex : 1024 / 768 */
            src_ratio = ROUND_OFF_HALF(((float)cropW / (float)cropH), 2);
            /* ex : 352  / 288 */
            dst_ratio = ROUND_OFF_HALF(((float)pictureW / (float)pictureH), 2);

            if (dst_ratio <= src_ratio) {
                /* shrink w */
                cropX = ALIGN_DOWN(((int)(hwSensorW - ((pictureH / maxZoomRatio) * src_ratio)) >> 1), 2);
                cropY = ALIGN_DOWN(((hwSensorH - (pictureH / maxZoomRatio)) >> 1), 2);
            } else {
                /* shrink h */
                cropX = ALIGN_DOWN(((hwSensorW - (pictureW / maxZoomRatio)) >> 1), 2);
                cropY = ALIGN_DOWN(((int)(hwSensorH - ((pictureW / maxZoomRatio) / src_ratio)) >> 1), 2);
            }
            cropW = hwSensorW - (cropX * 2);
            cropH = hwSensorH - (cropY * 2);
        }
    }

#if 0
    ALOGD("DEBUG(%s):hwSensorSize (%dx%d), previewSize (%dx%d)",
            __FUNCTION__, hwSensorW, hwSensorH, previewW, previewH);
    ALOGD("DEBUG(%s):hwPictureSize (%dx%d), pictureSize (%dx%d)",
            __FUNCTION__, hwPictureW, hwPictureH, pictureW, pictureH);
    ALOGD("DEBUG(%s):size cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            __FUNCTION__, cropX, cropY, cropW, cropH, zoomLevel);
    ALOGD("DEBUG(%s):size2 cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            __FUNCTION__, crop_crop_x, crop_crop_y, crop_crop_w, crop_crop_h, zoomLevel);
    ALOGD("DEBUG(%s):size pictureFormat = 0x%x, JPEG_INPUT_COLOR_FMT = 0x%x",
            __FUNCTION__, pictureFormat, JPEG_INPUT_COLOR_FMT);
#endif

    srcRect->x = 0;
    srcRect->y = 0;
    srcRect->w = hwSensorW;
    srcRect->h = hwSensorH;
    srcRect->fullW = hwSensorW;
    srcRect->fullH = hwSensorH;
    srcRect->colorFormat = bayerFormat;

    dstRect->x = cropX;
    dstRect->y = cropY;
    dstRect->w = cropW;
    dstRect->h = cropH;
    dstRect->fullW = cropW;
    dstRect->fullH = cropH;
    dstRect->colorFormat = bayerFormat;

    m_setHwBayerCropRegion(dstRect->w, dstRect->h, dstRect->x, dstRect->y);
    return NO_ERROR;
}

status_t ExynosCameraParameters::calcPreviewDzoomCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int previewW = 0, previewH = 0;
    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;

    int zoomLevel = 0;
    int maxZoomRatio = 0;
    float zoomRatio = getZoomRatio(0) / 1000;

    /* TODO: check state ready for start */
    zoomLevel = getZoomLevel();
    maxZoomRatio = getMaxZoomRatio() / 1000;
    getHwPreviewSize(&previewW, &previewH);
    zoomRatio = getZoomRatio(zoomLevel) / 1000;

    ret = getCropRectAlign(srcRect->w, srcRect->h,
                     previewW, previewH,
                     &srcRect->x, &srcRect->y,
                     &srcRect->w, &srcRect->h,
                     2, 2,
                     zoomLevel, zoomRatio);
    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = previewW;
    dstRect->h = previewH;

    ALOGV("INFO(%s[%d]):SRC cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d ratio = %f", __FUNCTION__, __LINE__, srcRect->x, srcRect->y, srcRect->w, srcRect->h, zoomLevel, zoomRatio);
    ALOGV("INFO(%s[%d]):DST cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d ratio = %f", __FUNCTION__, __LINE__, dstRect->x, dstRect->y, dstRect->w, dstRect->h, zoomLevel, zoomRatio);

    return NO_ERROR;
}

status_t ExynosCameraParameters::getPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int hwBnsW   = 0;
    int hwBnsH   = 0;
    int hwBcropW = 0;
    int hwBcropH = 0;
    int zoomLevel = 0;
    float zoomRatio = 1.00f;
    int hwSensorMarginW = 0;
    int hwSensorMarginH = 0;

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_staticInfo->pictureSizeLut == NULL
        || m_staticInfo->pictureSizeLutMax <= m_cameraInfo.pictureSizeRatioId
        || m_cameraInfo.pictureSizeRatioId != m_cameraInfo.previewSizeRatioId)
        return calcPictureBayerCropSize(srcRect, dstRect);

    /* use LUT */
    hwBnsW   = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BNS_W];
    hwBnsH   = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BNS_H];
    hwBcropW = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BCROP_W];
    hwBcropH = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BCROP_H];

    if (getHalVersion() != IS_HAL_VER_3_2) {
#ifdef SR_CAPTURE
        if(getSROn()) {
            zoomLevel = ZOOM_LEVEL_0;
        } else {
            zoomLevel = getZoomLevel();
        }
#else
        zoomLevel = getZoomLevel();
#endif
        zoomRatio = getZoomRatio(zoomLevel) / 1000;

#if defined(SCALER_MAX_SCALE_UP_RATIO)
        /*
         * After dividing float & casting int,
         * zoomed size can be smaller too much.
         * so, when zoom until max, ceil up about floating point.
         */
        if (ALIGN_UP((int)((float)hwBcropW / zoomRatio), CAMERA_BCROP_ALIGN) * SCALER_MAX_SCALE_UP_RATIO < hwBcropW ||
                ALIGN_UP((int)((float)hwBcropH / zoomRatio), 2) * SCALER_MAX_SCALE_UP_RATIO < hwBcropH) {
            hwBcropW = ALIGN_UP((int)ceil((float)hwBcropW / zoomRatio), CAMERA_BCROP_ALIGN);
            hwBcropH = ALIGN_UP((int)ceil((float)hwBcropH / zoomRatio), 2);
        } else
#endif
        {
            hwBcropW = ALIGN_UP((int)((float)hwBcropW / zoomRatio), CAMERA_BCROP_ALIGN);
            hwBcropH = ALIGN_UP((int)((float)hwBcropH / zoomRatio), 2);
        }
    }

    /* Re-calculate the BNS size for removing Sensor Margin.
       On Capture Stream(3AA_M2M_Input), the BNS is not used.
       So, the BNS ratio is not needed to be considered for sensor margin */
    getSensorMargin(&hwSensorMarginW, &hwSensorMarginH);
    hwBnsW = hwBnsW - hwSensorMarginW;
    hwBnsH = hwBnsH - hwSensorMarginH;

    /* src */
    srcRect->x = 0;
    srcRect->y = 0;
    srcRect->w = hwBnsW;
    srcRect->h = hwBnsH;

    if (getHalVersion() == IS_HAL_VER_3_2) {
        int cropRegionX = 0, cropRegionY = 0, cropRegionW = 0, cropRegionH = 0;
        int maxSensorW = 0, maxSensorH = 0;
        float scaleRatioX = 0.0f, scaleRatioY = 0.0f;
        status_t ret = NO_ERROR;

        m_getCropRegion(&cropRegionX, &cropRegionY, &cropRegionW, &cropRegionH);
        getMaxSensorSize(&maxSensorW, &maxSensorH);

        /* 1. Scale down the crop region to adjust with the bcrop input size */
        scaleRatioX = (float) hwBnsW / (float) maxSensorW;
        scaleRatioY = (float) hwBnsH / (float) maxSensorH;
        cropRegionX = (int) (cropRegionX * scaleRatioX);
        cropRegionY = (int) (cropRegionY * scaleRatioY);
        cropRegionW = (int) (cropRegionW * scaleRatioX);
        cropRegionH = (int) (cropRegionH * scaleRatioY);

        if (cropRegionW < 1 || cropRegionH < 1) {
            cropRegionW = hwBnsW;
            cropRegionH = hwBnsH;
        }

        /* 2. Calculate the real crop region with considering the target ratio */
        if ((cropRegionW > hwBcropW) && (cropRegionH > hwBcropH)) {
            dstRect->x = ALIGN_DOWN((cropRegionX + ((cropRegionW - hwBcropW) >> 1)), 2);
            dstRect->y = ALIGN_DOWN((cropRegionY + ((cropRegionH - hwBcropH) >> 1)), 2);
            dstRect->w = hwBcropW;
            dstRect->h = hwBcropH;
        } else {
            ret = getCropRectAlign(cropRegionW, cropRegionH,
                    hwBcropW, hwBcropH,
                    &(dstRect->x), &(dstRect->y),
                    &(dstRect->w), &(dstRect->h),
                    CAMERA_MAGIC_ALIGN, 2,
                    0, 0.0f);
            dstRect->x = ALIGN_DOWN((cropRegionX + dstRect->x), 2);
            dstRect->y = ALIGN_DOWN((cropRegionY + dstRect->y), 2);
        }
    } else {
        /* dst */
        if (hwBnsW > hwBcropW) {
            dstRect->x = ALIGN_UP(((hwBnsW - hwBcropW) >> 1), 2);
            dstRect->w = hwBcropW;
        } else {
            dstRect->x = 0;
            dstRect->w = hwBnsW;
        }

        if (hwBnsH > hwBcropH) {
            dstRect->y = ALIGN_UP(((hwBnsH - hwBcropH) >> 1), 2);
            dstRect->h = hwBcropH;
        } else {
            dstRect->y = 0;
            dstRect->h = hwBnsH;
        }
    }

#if DEBUG
    ALOGD("DEBUG(%s):zoomRatio=%f", __FUNCTION__, zoomRatio);
    ALOGD("DEBUG(%s):hwBnsSize (%dx%d), hwBcropSize (%d, %d)(%dx%d)",
            __FUNCTION__, srcRect->w, srcRect->h, dstRect->x, dstRect->y, dstRect->w, dstRect->h);
#endif

    return NO_ERROR;
}

status_t ExynosCameraParameters::calcPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int maxSensorW = 0, maxSensorH = 0;
    int hwSensorW = 0, hwSensorH = 0;
    int hwPictureW = 0, hwPictureH = 0, hwPictureFormat = 0;
    int hwSensorCropX = 0, hwSensorCropY = 0;
    int hwSensorCropW = 0, hwSensorCropH = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    int previewW = 0, previewH = 0;
    int hwSensorMarginW = 0, hwSensorMarginH = 0;

    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;

    int zoomLevel = 0;
    float zoomRatio = 1.0f;
    int maxZoomRatio = 0;
    int bayerFormat = getBayerFormat();

#ifdef DEBUG_RAWDUMP
    if (checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    /* TODO: check state ready for start */
    pictureFormat = getPictureFormat();
    zoomLevel = getZoomLevel();
    maxZoomRatio = getMaxZoomRatio() / 1000;
    getHwPictureSize(&hwPictureW, &hwPictureH);
    getPictureSize(&pictureW, &pictureH);

    getMaxSensorSize(&maxSensorW, &maxSensorH);
    getHwSensorSize(&hwSensorW, &hwSensorH);
    getPreviewSize(&previewW, &previewH);
    getSensorMargin(&hwSensorMarginW, &hwSensorMarginH);

    zoomRatio = getZoomRatio(zoomLevel) / 1000;

    hwSensorW -= hwSensorMarginW;
    hwSensorH -= hwSensorMarginH;

    if (getUsePureBayerReprocessing() == true) {
        if (getHalVersion() == IS_HAL_VER_3_2) {
            int cropRegionX = 0, cropRegionY = 0, cropRegionW = 0, cropRegionH = 0;
            float scaleRatioX = 0.0f, scaleRatioY = 0.0f;

            m_getCropRegion(&cropRegionX, &cropRegionY, &cropRegionW, &cropRegionH);

            /* 1. Scale down the crop region to adjust with the bcrop input size */
            scaleRatioX = (float) hwSensorW / (float) maxSensorW;
            scaleRatioY = (float) hwSensorH / (float) maxSensorH;
            cropRegionX = (int) (cropRegionX * scaleRatioX);
            cropRegionY = (int) (cropRegionY * scaleRatioY);
            cropRegionW = (int) (cropRegionW * scaleRatioX);
            cropRegionH = (int) (cropRegionH * scaleRatioY);

            if (cropRegionW < 1 || cropRegionH < 1) {
                cropRegionW = hwSensorW;
                cropRegionH = hwSensorH;
            }

            ret = getCropRectAlign(cropRegionW, cropRegionH,
                    pictureW, pictureH,
                    &cropX, &cropY,
                    &cropW, &cropH,
                    CAMERA_MAGIC_ALIGN, 2,
                    0, 0.0f);

            cropX = ALIGN_DOWN((cropRegionX + cropX), 2);
            cropY = ALIGN_DOWN((cropRegionY + cropY), 2);
        } else {
            ret = getCropRectAlign(hwSensorW, hwSensorH,
                    pictureW, pictureH,
                    &cropX, &cropY,
                    &cropW, &cropH,
                    CAMERA_MAGIC_ALIGN, 2,
                    zoomLevel, zoomRatio);

            cropX = ALIGN_DOWN(cropX, 2);
            cropY = ALIGN_DOWN(cropY, 2);
            cropW = hwSensorW - (cropX * 2);
            cropH = hwSensorH - (cropY * 2);
        }

        if (cropW < pictureW / maxZoomRatio || cropH < pictureH / maxZoomRatio) {
            ALOGW("WRN(%s[%d]): zoom ratio is upto x%d, crop(%dx%d), picture(%dx%d)", __FUNCTION__, __LINE__, maxZoomRatio, cropW, cropH, pictureW, pictureH);
            cropX = ALIGN_DOWN(((hwSensorW - (pictureW / maxZoomRatio)) >> 1), 2);
            cropY = ALIGN_DOWN(((hwSensorH - (pictureH / maxZoomRatio)) >> 1), 2);
            cropW = hwSensorW - (cropX * 2);
            cropH = hwSensorH - (cropY * 2);
        }
    } else {
        zoomLevel = 0;
        if (getHalVersion() == IS_HAL_VER_3_2)
            zoomRatio = 0.0f;
        else
            zoomRatio = getZoomRatio(zoomLevel) / 1000;

        getHwBayerCropRegion(&hwSensorCropW, &hwSensorCropH, &hwSensorCropX, &hwSensorCropY);

        ret = getCropRectAlign(hwSensorCropW, hwSensorCropH,
                     pictureW, pictureH,
                     &cropX, &cropY,
                     &cropW, &cropH,
                     CAMERA_MAGIC_ALIGN, 2,
                     zoomLevel, zoomRatio);

        cropX = ALIGN_DOWN(cropX, 2);
        cropY = ALIGN_DOWN(cropY, 2);
        cropW = hwSensorCropW - (cropX * 2);
        cropH = hwSensorCropH - (cropY * 2);

        if (cropW < pictureW / maxZoomRatio || cropH < pictureH / maxZoomRatio) {
            ALOGW("WRN(%s[%d]): zoom ratio is upto x%d, crop(%dx%d), picture(%dx%d)", __FUNCTION__, __LINE__, maxZoomRatio, cropW, cropH, pictureW, pictureH);
            cropX = ALIGN_DOWN(((hwSensorCropW - (pictureW / maxZoomRatio)) >> 1), 2);
            cropY = ALIGN_DOWN(((hwSensorCropH - (pictureH / maxZoomRatio)) >> 1), 2);
            cropW = hwSensorCropW - (cropX * 2);
            cropH = hwSensorCropH - (cropY * 2);
        }
    }

#if 1
    ALOGD("DEBUG(%s):maxSensorSize (%dx%d), hwSensorSize (%dx%d), previewSize (%dx%d)",
            __FUNCTION__, maxSensorW, maxSensorH, hwSensorW, hwSensorH, previewW, previewH);
    ALOGD("DEBUG(%s):hwPictureSize (%dx%d), pictureSize (%dx%d)",
            __FUNCTION__, hwPictureW, hwPictureH, pictureW, pictureH);
    ALOGD("DEBUG(%s):size cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            __FUNCTION__, cropX, cropY, cropW, cropH, zoomLevel);
    ALOGD("DEBUG(%s):size2 cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            __FUNCTION__, crop_crop_x, crop_crop_y, crop_crop_w, crop_crop_h, zoomLevel);
    ALOGD("DEBUG(%s):size pictureFormat = 0x%x, JPEG_INPUT_COLOR_FMT = 0x%x",
            __FUNCTION__, pictureFormat, JPEG_INPUT_COLOR_FMT);
#endif

    srcRect->x = 0;
    srcRect->y = 0;
    srcRect->w = maxSensorW;
    srcRect->h = maxSensorH;
    srcRect->fullW = maxSensorW;
    srcRect->fullH = maxSensorH;
    srcRect->colorFormat = bayerFormat;

    dstRect->x = cropX;
    dstRect->y = cropY;
    dstRect->w = cropW;
    dstRect->h = cropH;
    dstRect->fullW = cropW;
    dstRect->fullH = cropH;
    dstRect->colorFormat = bayerFormat;
    return NO_ERROR;
}

status_t ExynosCameraParameters::m_getPreviewBdsSize(ExynosRect *dstRect)
{
    int hwBdsW = 0;
    int hwBdsH = 0;
    int sizeList[SIZE_LUT_INDEX_END];

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_staticInfo->previewSizeLut == NULL
        || m_staticInfo->previewSizeLutMax <= m_cameraInfo.previewSizeRatioId
        || m_getPreviewSizeList(sizeList) != NO_ERROR) {
        ExynosRect rect;
        return calcPreviewBDSSize(&rect, dstRect);
    }

    /* use LUT */
    hwBdsW = sizeList[BDS_W];
    hwBdsH = sizeList[BDS_H];

    if (getRecordingHint() == true) {
        int videoW = 0, videoH = 0;
        getVideoSize(&videoW, &videoH);

        if (m_cameraInfo.previewSizeRatioId != m_cameraInfo.videoSizeRatioId)
            ALOGV("WARN(%s[%d]):preview ratioId(%d) != videoRatioId(%d), use previewRatioId",
                    __FUNCTION__, __LINE__,
                    m_cameraInfo.previewSizeRatioId, m_cameraInfo.videoSizeRatioId);

        if ((videoW == 3840 && videoH == 2160) || (videoW == 2560 && videoH == 1440)) {
            hwBdsW = videoW;
            hwBdsH = videoH;
        }
    }
#ifdef USE_BDS_WIDE_SELFIE
    else if (getShotMode() == SHOT_MODE_FRONT_PANORAMA && !getRecordingHint()) {
        hwBdsW = WIDE_SELFIE_WIDTH;
        hwBdsH = WIDE_SELFIE_HEIGHT;
    }
#endif

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = hwBdsW;
    dstRect->h = hwBdsH;

#ifdef DEBUG_PERFRAME
    ALOGD("DEBUG(%s):hwBdsSize (%dx%d)", __FUNCTION__, dstRect->w, dstRect->h);
#endif

    return NO_ERROR;
}

status_t ExynosCameraParameters::getPreviewBdsSize(ExynosRect *dstRect)
{
    status_t ret = NO_ERROR;

    ret = m_getPreviewBdsSize(dstRect);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):calcPreviewBDSSize() fail", __FUNCTION__, __LINE__);
        return ret;
    }

    if (this->getHWVdisMode() == true) {
        int disW = ALIGN_UP((int)(dstRect->w * HW_VDIS_W_RATIO), 2);
        int disH = ALIGN_UP((int)(dstRect->h * HW_VDIS_H_RATIO), 2);

        CLOGV("DEBUG(%s[%d]):HWVdis adjusted BDS Size (%d x %d) -> (%d x %d)",
            __FUNCTION__, __LINE__, dstRect->w, dstRect->h, disW, disH);

        /*
         * check H/W VDIS size(BDS dst size) is too big than bayerCropSize(BDS out size).
         */
        ExynosRect bnsSize, bayerCropSize;

        if (getPreviewBayerCropSize(&bnsSize, &bayerCropSize) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):getPreviewBayerCropSize() fail", __FUNCTION__, __LINE__);
        } else {
            if (bayerCropSize.w < disW || bayerCropSize.h < disH) {
                CLOGV("DEBUG(%s[%d]):bayerCropSize (%d x %d) is smaller than (%d x %d). so force bayerCropSize",
                    __FUNCTION__, __LINE__, bayerCropSize.w, bayerCropSize.h, disW, disH);

                disW = bayerCropSize.w;
                disH = bayerCropSize.h;
            }
        }

        dstRect->w = disW;
        dstRect->h = disH;
    }

#ifdef DEBUG_PERFRAME
    CLOGD("DEBUG(%s):hwBdsSize (%dx%d)", __FUNCTION__, dstRect->w, dstRect->h);
#endif

    return ret;
}

status_t ExynosCameraParameters::calcPreviewBDSSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int hwSensorW = 0, hwSensorH = 0;
    int hwPictureW = 0, hwPictureH = 0;
    int pictureW = 0, pictureH = 0;
    int previewW = 0, previewH = 0;
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;
#if 0
    int pictureFormat = 0, hwPictureFormat = 0;
#endif
    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;

    int zoomLevel = 0;
    int bayerFormat = getBayerFormat();
    float zoomRatio = 1.0f;

#ifdef DEBUG_RAWDUMP
    if (checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    /* TODO: check state ready for start */
#if 0
    pictureFormat = getPictureFormat();
#endif
    zoomLevel = getZoomLevel();
    getHwPictureSize(&hwPictureW, &hwPictureH);
    getPictureSize(&pictureW, &pictureH);
    zoomRatio = getZoomRatio(zoomLevel) / 1000;

    getHwSensorSize(&hwSensorW, &hwSensorH);
    getPreviewSize(&previewW, &previewH);

    /* TODO: get crop size from ctlMetadata */
    ret = getCropRectAlign(hwSensorW, hwSensorH,
                     previewW, previewH,
                     &cropX, &cropY,
                     &cropW, &cropH,
                     CAMERA_MAGIC_ALIGN, 2,
                     zoomLevel, zoomRatio);

    zoomRatio = getZoomRatio(0) / 1000;

    ret = getCropRectAlign(cropW, cropH,
                     previewW, previewH,
                     &crop_crop_x, &crop_crop_y,
                     &crop_crop_w, &crop_crop_h,
                     2, 2,
                     0, zoomRatio);

    cropX = ALIGN_UP(cropX, 2);
    cropY = ALIGN_UP(cropY, 2);
    cropW = hwSensorW - (cropX * 2);
    cropH = hwSensorH - (cropY * 2);

//    ALIGN_UP(crop_crop_x, 2);
//    ALIGN_UP(crop_crop_y, 2);

#if 0
    ALOGD("DEBUG(%s):hwSensorSize (%dx%d), previewSize (%dx%d)",
            __FUNCTION__, hwSensorW, hwSensorH, previewW, previewH);
    ALOGD("DEBUG(%s):hwPictureSize (%dx%d), pictureSize (%dx%d)",
            __FUNCTION__, hwPictureW, hwPictureH, pictureW, pictureH);
    ALOGD("DEBUG(%s):size cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            __FUNCTION__, cropX, cropY, cropW, cropH, zoomLevel);
    ALOGD("DEBUG(%s):size2 cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            __FUNCTION__, crop_crop_x, crop_crop_y, crop_crop_w, crop_crop_h, zoomLevel);
    ALOGD("DEBUG(%s):size pictureFormat = 0x%x, JPEG_INPUT_COLOR_FMT = 0x%x",
            __FUNCTION__, pictureFormat, JPEG_INPUT_COLOR_FMT);
#endif

    srcRect->x = cropX;
    srcRect->y = cropY;
    srcRect->w = cropW;
    srcRect->h = cropH;
    srcRect->fullW = cropW;
    srcRect->fullH = cropH;
    srcRect->colorFormat = bayerFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->colorFormat = JPEG_INPUT_COLOR_FMT;
    /* For Front Single Scenario, BDS should not be used */
    if (m_cameraId == CAMERA_ID_FRONT && getDualMode() == false) {
        getPreviewBayerCropSize(&bnsSize, &bayerCropSize);
        dstRect->w = bayerCropSize.w;
        dstRect->h = bayerCropSize.h;
        dstRect->fullW = bayerCropSize.w;
        dstRect->fullH = bayerCropSize.h;
    } else {
        dstRect->w = previewW;
        dstRect->h = previewH;
        dstRect->fullW = previewW;
        dstRect->fullH = previewH;
    }

    if (dstRect->w > srcRect->w)
        dstRect->w = srcRect->w;
    if (dstRect->h > srcRect->h)
        dstRect->h = srcRect->h;

    return NO_ERROR;
}

status_t ExynosCameraParameters::getPictureBdsSize(ExynosRect *dstRect)
{
    int hwBdsW = 0;
    int hwBdsH = 0;

    /* matched ratio LUT is not existed, use equation */
    if (m_useSizeTable == false
        || m_staticInfo->pictureSizeLut == NULL
        || m_staticInfo->pictureSizeLutMax <= m_cameraInfo.pictureSizeRatioId) {
        ExynosRect rect;
        return calcPictureBDSSize(&rect, dstRect);
    }

    /* use LUT */
    hwBdsW = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BDS_W];
    hwBdsH = m_staticInfo->pictureSizeLut[m_cameraInfo.pictureSizeRatioId][BDS_H];

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = hwBdsW;
    dstRect->h = hwBdsH;

    return NO_ERROR;
}

status_t ExynosCameraParameters::getFastenAeStableSensorSize(int *hwSensorW, int *hwSensorH)
{
    *hwSensorW = m_staticInfo->videoSizeLutHighSpeed120[0][SENSOR_W];
    *hwSensorH = m_staticInfo->videoSizeLutHighSpeed120[0][SENSOR_H];

    return NO_ERROR;
}

status_t ExynosCameraParameters::getFastenAeStableBcropSize(int *hwBcropW, int *hwBcropH)
{
    *hwBcropW = m_staticInfo->videoSizeLutHighSpeed120[0][BCROP_W];
    *hwBcropH = m_staticInfo->videoSizeLutHighSpeed120[0][BCROP_H];

    return NO_ERROR;
}

status_t ExynosCameraParameters::getFastenAeStableBdsSize(int *hwBdsW, int *hwBdsH)
{
    *hwBdsW = m_staticInfo->videoSizeLutHighSpeed120[0][BDS_W];
    *hwBdsH = m_staticInfo->videoSizeLutHighSpeed120[0][BDS_H];

    return NO_ERROR;
}


status_t ExynosCameraParameters::calcPictureBDSSize(ExynosRect *srcRect, ExynosRect *dstRect)
{
    int ret = 0;

    int maxSensorW = 0, maxSensorH = 0;
    int hwPictureW = 0, hwPictureH = 0;
    int pictureW = 0, pictureH = 0;
    int previewW = 0, previewH = 0;
#if 0
    int pictureFormat = 0, hwPictureFormat = 0;
#endif
    int cropX = 0, cropY = 0;
    int cropW = 0, cropH = 0;
    int crop_crop_x = 0, crop_crop_y = 0;
    int crop_crop_w = 0, crop_crop_h = 0;

    int zoomLevel = 0;
    int bayerFormat = getBayerFormat();
    float zoomRatio = 1.0f;

#ifdef DEBUG_RAWDUMP
    if (checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    /* TODO: check state ready for start */
#if 0
    pictureFormat = getPictureFormat();
#endif
    zoomLevel = getZoomLevel();
    getHwPictureSize(&hwPictureW, &hwPictureH);
    getPictureSize(&pictureW, &pictureH);

    getMaxSensorSize(&maxSensorW, &maxSensorH);
    getPreviewSize(&previewW, &previewH);

    zoomRatio = getZoomRatio(zoomLevel) / 1000;
    /* TODO: get crop size from ctlMetadata */
    ret = getCropRectAlign(maxSensorW, maxSensorH,
                     pictureW, pictureH,
                     &cropX, &cropY,
                     &cropW, &cropH,
                     CAMERA_MAGIC_ALIGN, 2,
                     zoomLevel, zoomRatio);

    zoomRatio = getZoomRatio(0) / 1000;
    ret = getCropRectAlign(cropW, cropH,
                     pictureW, pictureH,
                     &crop_crop_x, &crop_crop_y,
                     &crop_crop_w, &crop_crop_h,
                     2, 2,
                     0, zoomRatio);

    cropX = ALIGN_UP(cropX, 2);
    cropY = ALIGN_UP(cropY, 2);
    cropW = maxSensorW - (cropX * 2);
    cropH = maxSensorH - (cropY * 2);

//    ALIGN_UP(crop_crop_x, 2);
//    ALIGN_UP(crop_crop_y, 2);

#if 0
    ALOGD("DEBUG(%s):SensorSize (%dx%d), previewSize (%dx%d)",
            __FUNCTION__, maxSensorW, maxSensorH, previewW, previewH);
    ALOGD("DEBUG(%s):hwPictureSize (%dx%d), pictureSize (%dx%d)",
            __FUNCTION__, hwPictureW, hwPictureH, pictureW, pictureH);
    ALOGD("DEBUG(%s):size cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            __FUNCTION__, cropX, cropY, cropW, cropH, zoomLevel);
    ALOGD("DEBUG(%s):size2 cropX = %d, cropY = %d, cropW = %d, cropH = %d, zoom = %d",
            __FUNCTION__, crop_crop_x, crop_crop_y, crop_crop_w, crop_crop_h, zoomLevel);
    ALOGD("DEBUG(%s):size pictureFormat = 0x%x, JPEG_INPUT_COLOR_FMT = 0x%x",
            __FUNCTION__, pictureFormat, JPEG_INPUT_COLOR_FMT);
#endif

    srcRect->x = cropX;
    srcRect->y = cropY;
    srcRect->w = cropW;
    srcRect->h = cropH;
    srcRect->fullW = cropW;
    srcRect->fullH = cropH;
    srcRect->colorFormat = bayerFormat;

    dstRect->x = 0;
    dstRect->y = 0;
    dstRect->w = pictureW;
    dstRect->h = pictureH;
    dstRect->fullW = pictureW;
    dstRect->fullH = pictureH;
    dstRect->colorFormat = JPEG_INPUT_COLOR_FMT;

    if (dstRect->w > srcRect->w)
        dstRect->w = srcRect->w;
    if (dstRect->h > srcRect->h)
        dstRect->h = srcRect->h;

    return NO_ERROR;
}

status_t ExynosCameraParameters::calcNormalToTpuSize(int srcW, int srcH, int *dstW, int *dstH)
{
    status_t ret = NO_ERROR;
    if (srcW < 0 || srcH < 0) {
        CLOGE("ERR(%s[%d]):src size is invalid(%d x %d)", __FUNCTION__, __LINE__, srcW, srcH);
        return INVALID_OPERATION;
    }

    int disW = ALIGN_UP((int)(srcW * HW_VDIS_W_RATIO), 2);
    int disH = ALIGN_UP((int)(srcH * HW_VDIS_H_RATIO), 2);

    *dstW = disW;
    *dstH = disH;
    CLOGD("DEBUG(%s[%d]):HWVdis adjusted BDS Size (%d x %d) -> (%d x %d)", __FUNCTION__, __LINE__, srcW, srcH, disW, disH);

    return NO_ERROR;
}

status_t ExynosCameraParameters::calcTpuToNormalSize(int srcW, int srcH, int *dstW, int *dstH)
{
    status_t ret = NO_ERROR;
    if (srcW < 0 || srcH < 0) {
        CLOGE("ERR(%s[%d]):src size is invalid(%d x %d)", __FUNCTION__, __LINE__, srcW, srcH);
        return INVALID_OPERATION;
    }

    int disW = ALIGN_DOWN((int)(srcW / HW_VDIS_W_RATIO), 2);
    int disH = ALIGN_DOWN((int)(srcH / HW_VDIS_H_RATIO), 2);

    *dstW = disW;
    *dstH = disH;
    CLOGD("DEBUG(%s[%d]):HWVdis adjusted BDS Size (%d x %d) -> (%d x %d)", __FUNCTION__, __LINE__, srcW, srcH, disW, disH);

    return ret;
}

void ExynosCameraParameters::setUsePureBayerReprocessing(bool enable)
{
    m_usePureBayerReprocessing = enable;
}

bool ExynosCameraParameters::getUsePureBayerReprocessing(void)
{
    int oldMode = m_usePureBayerReprocessing;

    if (getRecordingHint() == true) {
        if (getDualMode() == true)
            m_usePureBayerReprocessing = (getCameraId() == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_DUAL_RECORDING : USE_PURE_BAYER_REPROCESSING_FRONT_ON_DUAL_RECORDING;
        else
            m_usePureBayerReprocessing = (getCameraId() == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_RECORDING : USE_PURE_BAYER_REPROCESSING_FRONT_ON_RECORDING;
    } else {
        if (getDualMode() == true)
            m_usePureBayerReprocessing = (getCameraId() == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING_ON_DUAL : USE_PURE_BAYER_REPROCESSING_FRONT_ON_DUAL;
        else
            m_usePureBayerReprocessing = (getCameraId() == CAMERA_ID_BACK) ? USE_PURE_BAYER_REPROCESSING : USE_PURE_BAYER_REPROCESSING_FRONT;
    }

    if (oldMode != m_usePureBayerReprocessing) {
        CLOGD("DEBUG(%s[%d]):bayer usage is changed (%d -> %d)", __FUNCTION__, __LINE__, oldMode, m_usePureBayerReprocessing);
    }

    return m_usePureBayerReprocessing;
}

#ifdef SAMSUNG_LBP
bool ExynosCameraParameters::getUseBestPic(void)
{
    return m_useBestPic;
}
#endif

int32_t ExynosCameraParameters::getReprocessingBayerMode(void)
{
    int32_t mode = REPROCESSING_BAYER_MODE_NONE;
    bool useDynamicBayer = (getRecordingHint() == true || getDualRecordingHint() == true) ?
        getUseDynamicBayerVideoSnapShot() : getUseDynamicBayer();

    if (isReprocessing() == false)
        return mode;

    if (useDynamicBayer == true) {
        if (getUsePureBayerReprocessing() == true)
            mode = REPROCESSING_BAYER_MODE_PURE_DYNAMIC;
        else
            mode = REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC;
    } else {
        if (getUsePureBayerReprocessing() == true)
            mode = REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON;
        else
            mode = REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON;
    }

    return mode;
}

void ExynosCameraParameters::setAdaptiveCSCRecording(bool enable)
{
    m_useAdaptiveCSCRecording = enable;
}

bool ExynosCameraParameters::getAdaptiveCSCRecording(void)
{
    return m_useAdaptiveCSCRecording;
}

bool ExynosCameraParameters::doCscRecording(void)
{
    bool ret = true;
    int hwPreviewW = 0, hwPreviewH = 0;
    int videoW = 0, videoH = 0;
#ifdef USE_LIVE_BROADCAST
    bool plbmode = getPLBMode();
#endif

    getHwPreviewSize(&hwPreviewW, &hwPreviewH);
#ifdef SUPPORT_SW_VDIS
    if(isSWVdisMode())
        m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);
#endif /*SUPPORT_SW_VDIS*/

    getVideoSize(&videoW, &videoH);
    CLOGV("DEBUG(%s[%d]):hwPreviewSize = %d x %d",  __FUNCTION__, __LINE__, hwPreviewW, hwPreviewH);
    CLOGV("DEBUG(%s[%d]):videoSize     = %d x %d",  __FUNCTION__, __LINE__, videoW, videoH);

    if (((videoW == 3840 && videoH == 2160) || (videoW == 1920 && videoH == 1080) || (videoW == 2560 && videoH == 1440))
        && m_useAdaptiveCSCRecording == true
        && videoW == hwPreviewW
        && videoH == hwPreviewH
#ifdef USE_LIVE_BROADCAST
        && plbmode == false
#endif
    ) {
        ret = false;
    }

#if 0 /* temporary */
#ifdef USE_3DNR_ALWAYS
    if (USE_3DNR_ALWAYS == false)
        ret = true;
#else
        ret = true;
#endif
#endif

    return ret;
}

int ExynosCameraParameters::getHalPixelFormat(void)
{
    int setfile = 0;
    int yuvRange = 0;
    int previewFormat = getHwPreviewFormat();
    int halFormat = 0;

    m_getSetfileYuvRange(false, &setfile, &yuvRange);

    halFormat = convertingHalPreviewFormat(previewFormat, yuvRange);

    return halFormat;
}

#if (TARGET_ANDROID_VER_MAJ >= 4 && TARGET_ANDROID_VER_MIN >= 4)
int ExynosCameraParameters::convertingHalPreviewFormat(int previewFormat, int yuvRange)
{
    int halFormat = 0;

    switch (previewFormat) {
    case V4L2_PIX_FMT_NV21:
        CLOGD("DEBUG(%s[%d]): preview format NV21", __FUNCTION__, __LINE__);
        halFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
        break;
    case V4L2_PIX_FMT_NV21M:
        CLOGD("DEBUG(%s[%d]): preview format NV21M", __FUNCTION__, __LINE__);
        if (yuvRange == YUV_FULL_RANGE) {
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL;
        } else if (yuvRange == YUV_LIMITED_RANGE) {
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M;
        } else {
            CLOGW("WRN(%s[%d]): invalid yuvRange, force set to full range", __FUNCTION__, __LINE__);
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL;
        }
        break;
    case V4L2_PIX_FMT_YVU420:
        CLOGD("DEBUG(%s[%d]): preview format YVU420", __FUNCTION__, __LINE__);
        halFormat = HAL_PIXEL_FORMAT_YV12;
        break;
    case V4L2_PIX_FMT_YVU420M:
        CLOGD("DEBUG(%s[%d]): preview format YVU420M", __FUNCTION__, __LINE__);
        halFormat = HAL_PIXEL_FORMAT_EXYNOS_YV12_M;
        break;
    default:
        CLOGE("ERR(%s[%d]): unknown preview format(%d)", __FUNCTION__, __LINE__, previewFormat);
        break;
    }

    return halFormat;
}
#else
int ExynosCameraParameters::convertingHalPreviewFormat(int previewFormat, int yuvRange)
{
    int halFormat = 0;

    switch (previewFormat) {
    case V4L2_PIX_FMT_NV21:
        halFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
        break;
    case V4L2_PIX_FMT_NV21M:
        if (yuvRange == YUV_FULL_RANGE) {
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_FULL;
        } else if (yuvRange == YUV_LIMITED_RANGE) {
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP;
        } else {
            CLOGW("WRN(%s[%d]): invalid yuvRange, force set to full range", __FUNCTION__, __LINE__);
            halFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_FULL;
        }
        break;
    case V4L2_PIX_FMT_YVU420:
        halFormat = HAL_PIXEL_FORMAT_YV12;
        break;
    case V4L2_PIX_FMT_YVU420M:
        halFormat = HAL_PIXEL_FORMAT_EXYNOS_YV12;
        break;
    default:
        CLOGE("ERR(%s[%d]): unknown preview format(%d)", __FUNCTION__, __LINE__, previewFormat);
        break;
    }

    return halFormat;
}
#endif

void ExynosCameraParameters::setDvfsLock(bool lock) {
    m_dvfsLock = lock;
}

bool ExynosCameraParameters::getDvfsLock(void) {
    return m_dvfsLock;
}

int ExynosCameraParameters::getBayerFormat(void)
{
    int bayerFormat = CAMERA_BAYER_FORMAT;

#ifdef CAMERA_PACKED_BAYER_ENABLE
    bayerFormat = CAMERA_BAYER_FORMAT;
#else
    /* hack : always set unpack bayer. HAL can't know pro mode */
    if (getDualMode()) {
        bayerFormat = CAMERA_BAYER_FORMAT;
    } else {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    return bayerFormat;
}

bool ExynosCameraParameters::m_getBayerPackModeEnable(void)
{
    bool ret = false;

#ifdef CAMERA_PACKED_BAYER_ENABLE
    ret = true;
#endif

#ifdef DEBUG_RAWDUMP
    if (ret == true) {
        if (checkBayerDumpEnable() == true) {
            ret = false;
        }
    }
#endif

#if 0 // we should add shot mode for pro mode.
    if (ret == true) {
        if (getShotMode() == SHOT_MODE_LONG_EXPOSURE) {
            ret = false;
        }
    }
#endif

    return ret;
}

#ifdef DEBUG_RAWDUMP
bool ExynosCameraParameters::checkBayerDumpEnable(void)
{
#ifndef RAWDUMP_CAPTURE
    char enableRawDump[PROPERTY_VALUE_MAX];
    property_get("ro.debug.rawdump", enableRawDump, "0");

    if (strcmp(enableRawDump, "1") == 0) {
        /*CLOGD("checkBayerDumpEnable : 1");*/
        return true;
    } else {
        /*CLOGD("checkBayerDumpEnable : 0");*/
        return false;
    }
#endif
    return true;
}
#endif  /* DEBUG_RAWDUMP */

bool ExynosCameraParameters::setConfig(struct ExynosConfigInfo* config)
{
    memcpy(m_exynosconfig, config, sizeof(struct ExynosConfigInfo));
    setConfigMode(m_exynosconfig->mode);
    return true;
}
struct ExynosConfigInfo* ExynosCameraParameters::getConfig()
{
    return m_exynosconfig;
}

bool ExynosCameraParameters::setConfigMode(uint32_t mode)
{
    bool ret = false;
    switch(mode){
    case CONFIG_MODE::NORMAL:
    case CONFIG_MODE::HIGHSPEED_60:
    case CONFIG_MODE::HIGHSPEED_120:
    case CONFIG_MODE::HIGHSPEED_240:
        m_exynosconfig->current = &m_exynosconfig->info[mode];
        m_exynosconfig->mode = mode;
        ret = true;
        break;
    default:
        CLOGE("ERR(%s[%d]): unknown config mode (%d)", __FUNCTION__, __LINE__, mode);
    }
    return ret;
}

int ExynosCameraParameters::getConfigMode()
{
    int ret = -1;
    switch(m_exynosconfig->mode){
    case CONFIG_MODE::NORMAL:
    case CONFIG_MODE::HIGHSPEED_60:
    case CONFIG_MODE::HIGHSPEED_120:
    case CONFIG_MODE::HIGHSPEED_240:
        ret = m_exynosconfig->mode;
        break;
    default:
        CLOGE("ERR(%s[%d]): unknown config mode (%d)", __FUNCTION__, __LINE__, m_exynosconfig->mode);
    }

    return ret;
}

void ExynosCameraParameters::setZoomActiveOn(bool enable)
{
    m_zoom_activated = enable;
}

bool ExynosCameraParameters::getZoomActiveOn(void)
{
    return m_zoom_activated;
}

status_t ExynosCameraParameters::setMarkingOfExifFlash(int flag)
{
    m_firing_flash_marking = flag;

    return NO_ERROR;
}

int ExynosCameraParameters::getMarkingOfExifFlash(void)
{
    return m_firing_flash_marking;
}

bool ExynosCameraParameters::increaseMaxBufferOfPreview(void)
{
    if((getShotMode() == SHOT_MODE_BEAUTY_FACE)||(getShotMode() == SHOT_MODE_FRONT_PANORAMA)
#ifdef LLS_CAPTURE
        || (getLLSOn() == true && getCameraId() == CAMERA_ID_FRONT)
#endif
        ) {
        return true;
    } else {
        return false;
    }
}

bool ExynosCameraParameters::getSensorOTFSupported(void)
{
    return m_staticInfo->flite3aaOtfSupport;
}

bool ExynosCameraParameters::isReprocessing(void)
{
    bool reprocessing = false;
    int cameraId = getCameraId();
    bool flagDual = getDualMode();

        if (cameraId == CAMERA_ID_BACK) {
#if defined(MAIN_CAMERA_DUAL_REPROCESSING) && defined(MAIN_CAMERA_SINGLE_REPROCESSING)
            reprocessing = (flagDual == true) ? MAIN_CAMERA_DUAL_REPROCESSING : MAIN_CAMERA_SINGLE_REPROCESSING;
#else
            ALOGW("WRN(%s[%d]): MAIN_CAMERA_DUAL_REPROCESSING/MAIN_CAMERA_SINGLE_REPROCESSING is not defined", __FUNCTION__, __LINE__);
#endif
        } else {
#if defined(FRONT_CAMERA_DUAL_REPROCESSING) && defined(FRONT_CAMERA_SINGLE_REPROCESSING)
            reprocessing = (flagDual == true) ? FRONT_CAMERA_DUAL_REPROCESSING : FRONT_CAMERA_SINGLE_REPROCESSING;
#else
            ALOGW("WRN(%s[%d]): FRONT_CAMERA_DUAL_REPROCESSING/FRONT_CAMERA_SINGLE_REPROCESSING is not defined", __FUNCTION__, __LINE__);
#endif
        }

    return reprocessing;
}

bool ExynosCameraParameters::isSccCapture(void)
{
    bool sccCapture = false;
    int cameraId = getCameraId();
    bool flagDual = getDualMode();

        if (cameraId == CAMERA_ID_BACK) {
#if defined(MAIN_CAMERA_DUAL_SCC_CAPTURE) && defined(MAIN_CAMERA_SINGLE_SCC_CAPTURE)
            sccCapture = (flagDual == true) ? MAIN_CAMERA_DUAL_SCC_CAPTURE : MAIN_CAMERA_SINGLE_SCC_CAPTURE;
#else
            ALOGW("WRN(%s[%d]): MAIN_CAMERA_DUAL_SCC_CAPTURE/MAIN_CAMERA_SINGLE_SCC_CAPTUREis not defined", __FUNCTION__, __LINE__);
#endif
        } else {
#if defined(FRONT_CAMERA_DUAL_SCC_CAPTURE) && defined(FRONT_CAMERA_SINGLE_SCC_CAPTURE)
            sccCapture = (flagDual == true) ? FRONT_CAMERA_DUAL_SCC_CAPTURE : FRONT_CAMERA_SINGLE_SCC_CAPTURE;
#else
            ALOGW("WRN(%s[%d]): FRONT_CAMERA_DUAL_SCC_CAPTURE/FRONT_CAMERA_SINGLE_SCC_CAPTURE is not defined", __FUNCTION__, __LINE__);
#endif
        }

    return sccCapture;
}

bool ExynosCameraParameters::isFlite3aaOtf(void)
{
    bool flagOtfInput = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();
    bool flagSensorOtf = getSensorOTFSupported();

    if (flagSensorOtf == false)
        return flagOtfInput;

            if (cameraId == CAMERA_ID_BACK) {
                /* for 52xx scenario */
                flagOtfInput = true;

                if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_FLITE_3AA_OTF
                    flagOtfInput = MAIN_CAMERA_DUAL_FLITE_3AA_OTF;
#else
                    ALOGW("WRN(%s[%d]): MAIN_CAMERA_DUAL_FLITE_3AA_OTF is not defined", __FUNCTION__, __LINE__);
#endif
                } else {
#ifdef MAIN_CAMERA_SINGLE_FLITE_3AA_OTF
                    flagOtfInput = MAIN_CAMERA_SINGLE_FLITE_3AA_OTF;
#else
                    ALOGW("WRN(%s[%d]): MAIN_CAMERA_SINGLE_FLITE_3AA_OTF is not defined", __FUNCTION__, __LINE__);
#endif
                }
            } else {
                if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_FLITE_3AA_OTF
                    flagOtfInput = FRONT_CAMERA_DUAL_FLITE_3AA_OTF;
#else
                    ALOGW("WRN(%s[%d]): FRONT_CAMERA_DUAL_FLITE_3AA_OTF is not defined", __FUNCTION__, __LINE__);
#endif
                } else {
#ifdef FRONT_CAMERA_SINGLE_FLITE_3AA_OTF
                    flagOtfInput = FRONT_CAMERA_SINGLE_FLITE_3AA_OTF;
#else
                    ALOGW("WRN(%s[%d]): FRONT_CAMERA_SINGLE_FLITE_3AA_OTF is not defined", __FUNCTION__, __LINE__);
#endif
                }
            }

    return flagOtfInput;
}

bool ExynosCameraParameters::is3aaIspOtf(void)
{
    bool ret = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();

        if (cameraId == CAMERA_ID_BACK) {
            if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_3AA_ISP_OTF
                ret = MAIN_CAMERA_DUAL_3AA_ISP_OTF;
#else
                ALOGW("WRN(%s[%d]): MAIN_CAMERA_DUAL_3AA_ISP_OTF is not defined", __FUNCTION__, __LINE__);
#endif
            } else {
#ifdef MAIN_CAMERA_SINGLE_3AA_ISP_OTF
                ret = MAIN_CAMERA_SINGLE_3AA_ISP_OTF;
#else
                ALOGW("WRN(%s[%d]): MAIN_CAMERA_SINGLE_3AA_ISP_OTF is not defined", __FUNCTION__, __LINE__);
#endif
            }
        } else {
            if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_3AA_ISP_OTF
                ret = FRONT_CAMERA_DUAL_3AA_ISP_OTF;
#else
                ALOGW("WRN(%s[%d]): FRONT_CAMERA_DUAL_3AA_ISP_OTF is not defined", __FUNCTION__, __LINE__);
#endif
            } else {
#ifdef FRONT_CAMERA_SINGLE_3AA_ISP_OTF
                ret = FRONT_CAMERA_SINGLE_3AA_ISP_OTF;
#else
                ALOGW("WRN(%s[%d]): FRONT_CAMERA_SINGLE_3AA_ISP_OTF is not defined", __FUNCTION__, __LINE__);
#endif
            }
        }

    return ret;
}


bool ExynosCameraParameters::isIspTpuOtf(void)
{
    bool ret = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();

    if (cameraId == CAMERA_ID_BACK) {
        if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_ISP_TPU_OTF
            ret = MAIN_CAMERA_DUAL_ISP_TPU_OTF;
#else
            ALOGW("WRN(%s[%d]): MAIN_CAMERA_DUAL_ISP_TPU_OTF is not defined", __FUNCTION__, __LINE__);
#endif
        } else {
#ifdef MAIN_CAMERA_SINGLE_ISP_TPU_OTF
            ret = MAIN_CAMERA_SINGLE_ISP_TPU_OTF;
#else
            ALOGW("WRN(%s[%d]): MAIN_CAMERA_SINGLE_ISP_TPU_OTF is not defined", __FUNCTION__, __LINE__);
#endif
        }
    } else {
        if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_ISP_TPU_OTF
            ret = FRONT_CAMERA_DUAL_ISP_TPU_OTF;
#else
            ALOGW("WRN(%s[%d]): FRONT_CAMERA_DUAL_ISP_TPU_OTF is not defined", __FUNCTION__, __LINE__);
#endif
        } else {
#ifdef FRONT_CAMERA_SINGLE_ISP_TPU_OTF
            ret = FRONT_CAMERA_SINGLE_ISP_TPU_OTF;
#else
            ALOGW("WRN(%s[%d]): FRONT_CAMERA_SINGLE_ISP_TPU_OTF is not defined", __FUNCTION__, __LINE__);
#endif
        }
    }

    if (flagDual == false) {
        ret = m_getIspTpuOtf(ret);

#ifdef USE_3DNR_ALWAYS
        if (USE_3DNR_ALWAYS == false) {
            ret = true;
        }

#ifdef USE_LIVE_BROADCAST
        if (getPLBMode() == true) {
            ret = true;
        }
#endif /* USE_LIVE_BROADCAST */
#endif /* USE_3DNR_ALWAYS */
    }

    return ret;
}

bool ExynosCameraParameters::isReprocessing3aaIspOTF(void)
{
    bool otf = false;

    int cameraId = getCameraId();
    bool flagDual = getDualMode();

        if (cameraId == CAMERA_ID_BACK) {
            if (flagDual == true) {
#ifdef MAIN_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING
                otf = MAIN_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING;
#else
                ALOGW("WRN(%s[%d]): MAIN_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING is not defined", __FUNCTION__, __LINE__);
#endif
            } else {
#ifdef MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING
                otf = MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING;
#else
                ALOGW("WRN(%s[%d]): MAIN_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING is not defined", __FUNCTION__, __LINE__);
#endif
            }
        } else {
            if (flagDual == true) {
#ifdef FRONT_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING
                otf = FRONT_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING;
#else
                ALOGW("WRN(%s[%d]): FRONT_CAMERA_DUAL_3AA_ISP_OTF_REPROCESSING is not defined", __FUNCTION__, __LINE__);
#endif
            } else {
#ifdef FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING
                otf = FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING;
#else
                ALOGW("WRN(%s[%d]): FRONT_CAMERA_SINGLE_3AA_ISP_OTF_REPROCESSING is not defined", __FUNCTION__, __LINE__);
#endif
            }
        }

    if (otf == true) {
        bool flagDirtyBayer = false;

        int reprocessingBayerMode = this->getReprocessingBayerMode();
        switch(reprocessingBayerMode) {
        case REPROCESSING_BAYER_MODE_NONE:
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
            flagDirtyBayer = false;
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
        default:
            flagDirtyBayer = true;
            break;
        }

        if (flagDirtyBayer == true) {
            ALOGW("WRN(%s[%d]): otf == true. but, flagDirtyBayer == true. so force false on 3aa_isp otf",
                __FUNCTION__, __LINE__);

            otf = false;
        }
    }

    return otf;
}

bool ExynosCameraParameters::getSupportedZoomPreviewWIthScaler(void)
{
    bool ret = false;
    int cameraId = getCameraId();
    bool flagDual = getDualMode();
    int fastFpsMode = getFastFpsMode();
    int vrMode = getVRMode();

    if (cameraId == CAMERA_ID_BACK) {
        if (fastFpsMode > CONFIG_MODE::HIGHSPEED_60
            && fastFpsMode < CONFIG_MODE::MAX
            && vrMode != 1) {
            ret = true;
        }
    } else {
        if (flagDual == true) {
            ret = true;
        }
    }

    return ret;
}

void ExynosCameraParameters::setZoomPreviewWIthScaler(bool enable)
{
    m_zoomWithScaler = enable;
}

bool ExynosCameraParameters::getZoomPreviewWIthScaler(void)
{
    return m_zoomWithScaler;
}

int ExynosCameraParameters::getHalVersion(void)
{
    return m_halVersion;
}

void ExynosCameraParameters::setHalVersion(int halVersion)
{
    m_halVersion = halVersion;
    m_activityControl->setHalVersion(m_halVersion);

    ALOGI("INFO(%s[%d]): m_halVersion(%d)", __FUNCTION__, __LINE__, m_halVersion);

    return;
}

#ifdef USE_FADE_IN_ENTRANCE
status_t ExynosCameraParameters::checkFirstEntrance(const CameraParameters& params)
{
    const char *strNewFirstEntrance = params.get("entrance");
    bool curFirstEntrance = getFirstEntrance();
    bool firstEntrance = false;

    CLOGD("DEBUG(%s):newFirstEntrance %s", "setParameters", strNewFirstEntrance);

    if (strNewFirstEntrance != NULL && !strcmp(strNewFirstEntrance, "true")) {
        firstEntrance = true;
    } else {
        firstEntrance = false;
    }

    if (curFirstEntrance != firstEntrance) {
        setFirstEntrance(firstEntrance);
        m_params.set("entrance", strNewFirstEntrance);
    }
    return NO_ERROR;
}

void ExynosCameraParameters::setFirstEntrance(bool value)
{
    m_flagFirstEntrance = value;
}

bool ExynosCameraParameters::getFirstEntrance(void)
{
    return m_flagFirstEntrance;
}
#endif

#ifdef SAMSUNG_OT
bool ExynosCameraParameters::getObjectTrackingEnable(void)
{
    return m_startObjectTracking;
}

bool ExynosCameraParameters::getObjectTrackingChanged(void)
{
    return m_objectTrackingAreaChanged;
}

void ExynosCameraParameters::setObjectTrackingChanged(bool newValue)
{
    m_objectTrackingAreaChanged = newValue;

    return;
}

int ExynosCameraParameters::getObjectTrackingAreas(int* validFocusArea, ExynosRect2* areas, int* weights)
{
    if(m_objectTrackingArea == NULL || m_objectTrackingWeight == NULL) {
        ALOGE("ERR(%s[%d]): NULL pointer!", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    *validFocusArea = m_cameraInfo.numValidFocusArea;
    for (int i = 0; i < *validFocusArea; i++) {
        areas[i] = m_objectTrackingArea[i];
        weights[i] = m_objectTrackingWeight[i];
    }

    return NO_ERROR;
}

void ExynosCameraParameters::setObjectTrackingGet(bool newValue)
{
    m_objectTrackingGet = newValue;

    return;
}

bool ExynosCameraParameters::getObjectTrackingGet(void)
{
    return m_objectTrackingGet;
}
#endif
#ifdef SAMSUNG_LBP
void ExynosCameraParameters::setNormalBestFrameCount(uint32_t count)
{
    m_normal_best_frame_count = count;
}

uint32_t ExynosCameraParameters::getNormalBestFrameCount(void)
{
    return m_normal_best_frame_count;
}

void ExynosCameraParameters::resetNormalBestFrameCount(void)
{
    m_normal_best_frame_count = 0;
}

void ExynosCameraParameters::setSCPFrameCount(uint32_t count)
{
    m_scp_frame_count = count;
}

uint32_t ExynosCameraParameters::getSCPFrameCount(void)
{
    return m_scp_frame_count;
}

void ExynosCameraParameters::resetSCPFrameCount(void)
{
    m_scp_frame_count = 0;
}

void ExynosCameraParameters::setBayerFrameCount(uint32_t count)
{
    m_bayer_frame_count = count;
}

uint32_t ExynosCameraParameters::getBayerFrameCount(void)
{
    return m_bayer_frame_count;
}

void ExynosCameraParameters::resetBayerFrameCount(void)
{
    m_bayer_frame_count = 0;
}
#endif

#ifdef SAMSUNG_JQ
void ExynosCameraParameters::setJpegQtableStatus(int state)
{
    Mutex::Autolock lock(m_JpegQtableLock);

    m_jpegQtableStatus = state;
}

int ExynosCameraParameters::getJpegQtableStatus(void)
{
    Mutex::Autolock lock(m_JpegQtableLock);

    return m_jpegQtableStatus;
}

void ExynosCameraParameters::setJpegQtableOn(bool isOn)
{
    Mutex::Autolock lock(m_JpegQtableLock);

    m_isJpegQtableOn = isOn;
}

bool ExynosCameraParameters::getJpegQtableOn(void)
{
    Mutex::Autolock lock(m_JpegQtableLock);

    return m_isJpegQtableOn;
}

void ExynosCameraParameters::setJpegQtable(unsigned char* qtable)
{
    Mutex::Autolock lock(m_JpegQtableLock);

    memcpy(m_jpegQtable, qtable, sizeof(m_jpegQtable));
}

void ExynosCameraParameters::getJpegQtable(unsigned char* qtable)
{
    Mutex::Autolock lock(m_JpegQtableLock);

    memcpy(qtable, m_jpegQtable, sizeof(m_jpegQtable));
}
#endif

#ifdef SAMSUNG_BD
void ExynosCameraParameters::setBlurInfo(unsigned char *data, unsigned int size)
{
    char sizeInfo[2] = {(unsigned char)((size >> 8) & 0xFF), (unsigned char)(size & 0xFF)};

    memset(m_staticInfo->bd_exif_info.bd_exif, 0, BD_EXIF_SIZE);
    memcpy(m_staticInfo->bd_exif_info.bd_exif, BD_EXIF_TAG, sizeof(BD_EXIF_TAG));
    memcpy(m_staticInfo->bd_exif_info.bd_exif + sizeof(BD_EXIF_TAG) - 1, sizeInfo, sizeof(sizeInfo));
    memcpy(m_staticInfo->bd_exif_info.bd_exif + sizeof(BD_EXIF_TAG) + sizeof(sizeInfo) -1, data, size);
}
#endif

#ifdef SAMSUNG_LLS_DEBLUR
void ExynosCameraParameters::setLLSdebugInfo(unsigned char *data, unsigned int size)
{
    char sizeInfo[2] = {(unsigned char)((size >> 8) & 0xFF), (unsigned char)(size & 0xFF)};

    memset(m_staticInfo->lls_exif_info.lls_exif, 0, LLS_EXIF_SIZE);
    memcpy(m_staticInfo->lls_exif_info.lls_exif, LLS_EXIF_TAG, sizeof(LLS_EXIF_TAG));
    memcpy(m_staticInfo->lls_exif_info.lls_exif + sizeof(LLS_EXIF_TAG) - 1, sizeInfo, sizeof(sizeInfo));
    memcpy(m_staticInfo->lls_exif_info.lls_exif + sizeof(LLS_EXIF_TAG) + sizeof(sizeInfo)- 1, data, size);
}
#endif

#ifdef SAMSUNG_HRM
void ExynosCameraParameters::m_setHRM(int ir_data, int status)
{
    setMetaCtlHRM(&m_metadata, ir_data, status);
}
void ExynosCameraParameters::m_setHRM_Hint(bool flag)
{
    m_flagSensorHRM_Hint = flag;
}
#endif
#ifdef SAMSUNG_LIGHT_IR
void ExynosCameraParameters::m_setLight_IR(SensorListenerEvent_t data)
{
    setMetaCtlLight_IR(&m_metadata, data);
}
void ExynosCameraParameters::m_setLight_IR_Hint(bool flag)
{
    m_flagSensorLight_IR_Hint = flag;
}
#endif
#ifdef SAMSUNG_GYRO
void ExynosCameraParameters::m_setGyro(SensorListenerEvent_t data)
{
    setMetaCtlGyro(&m_metadata, data);
}
void ExynosCameraParameters::m_setGyroHint(bool flag)
{
    m_flagSensorGyroHint = flag;
}
#endif

#ifdef FORCE_CAL_RELOAD
bool ExynosCameraParameters::m_checkCalibrationDataValid(char *sensor_fw)
{
    bool ret = true;
    char *tok = NULL;
    char *calcheck = NULL;
    char *last;
    char readdata[50];
    FILE *fp = NULL;

    fp = fopen("/sys/class/camera/rear/rear_force_cal_load", "r");
    if (fp == NULL) {
        return ret;
    } else {
        if (fgets(readdata, sizeof(readdata), fp) == NULL) {
            CLOGE("ERR(%s[%d]):failed to read sysfs entry", __FUNCTION__, __LINE__);
        }
        CLOGD("DEBUG(%s[%d]):%s", __FUNCTION__, __LINE__, readdata);

        fclose(fp);
    }

    tok = strtok_r(sensor_fw, " ", &last);
    if (tok != NULL) {
        calcheck = strtok_r(NULL, " ", &last);
    }

    if (calcheck != NULL && strlen(calcheck) >= 2 && !strncmp(calcheck, "NG", 2)) {
        CLOGE("ERR(%s):Cal data has error", __FUNCTION__);
        ret = false;
    }

    return ret;
}
#endif

#ifdef SAMSUNG_COMPANION
void ExynosCameraParameters::setUseCompanion(bool use)
{
    m_use_companion = use;
}

bool ExynosCameraParameters::getUseCompanion()
{
    return m_use_companion;
}
#endif

void ExynosCameraParameters::setIsThumbnailCallbackOn(bool enable)
{
    m_IsThumbnailCallbackOn = enable;
}

bool ExynosCameraParameters::getIsThumbnailCallbackOn()
{
    return m_IsThumbnailCallbackOn;
}
}; /* namespace android */

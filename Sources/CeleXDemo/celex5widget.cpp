﻿#include "celex5widget.h"
#include <QTimer>
#include <QFileDialog>
#include <QCoreApplication>
#include <QPainter>
#include <QPushButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QDebug>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#define READ_BIN_TIME            1
#define UPDATE_PLAY_INFO_TIME    30

cv::VideoWriter    m_writer,m_writer1;
VideoStream*       m_pVideoStream = new VideoStream;
std::ofstream      g_ofWriteMat;

SensorDataObserver::SensorDataObserver(CX5SensorDataServer *sensorData, QWidget *parent)
    : QWidget(parent)
    , m_imageMode1(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageMode2(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageMode3(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageMode4(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageEvent1(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageEvent2(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageEvent3(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageEvent4(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_pCeleX5(NULL)
    , m_bLoopModeEnabled(false)
    , m_iPicMode(0)
    , m_bShowMultipleWindows(false)
    , m_uiTemperature(0)
    , m_iFPS(30)
    , m_iPlaybackFPS(1000)
    , m_uiFullFrameFPS(100)
    , m_uiRealFullFrameFPS(0)
    , m_bSavePics(false)
    , m_bRecordingImages(false)
    , m_lFullFrameCount(0)
    , m_lEventFrameCount(0)
    , m_lOpticalFrameCount(0)
    , m_emDisplayType(Realtime)
    , m_iIntervalCounter(0)
    , m_iPicIntervalCount(0)
{
    m_pSensorData = sensorData;
    m_pSensorData->registerData(this, CeleX5DataManager::CeleX_Frame_Data);

    m_imageMode1.fill(Qt::black);
    m_imageMode2.fill(Qt::black);
    m_imageMode3.fill(Qt::black);
    m_imageMode4.fill(Qt::black);

    m_pUpdateTimer = new QTimer(this);
    connect(m_pUpdateTimer, SIGNAL(timeout()), this, SLOT(onUpdateImage()));
    m_pUpdateTimer->start(33);

    for (int i = 0; i < 4; i++)
        m_pBuffer[i] = new uchar[CELEX5_PIXELS_NUMBER];
}

SensorDataObserver::~SensorDataObserver()
{
    m_pSensorData->unregisterData(this, CeleX5DataManager::CeleX_Frame_Data);
    for (int i = 0; i < 4; i++)
    {
        delete[] m_pBuffer[i];
        m_pBuffer[i] = NULL;
    }
}

//This function is only for playback
void SensorDataObserver::onFrameDataUpdated(CeleX5ProcessedData* pSensorData)
{
    //cout << __FUNCTION__ << endl;
    if (/*!m_pCeleX5->isLoopModeEnabled() && */m_emDisplayType == Realtime)
    {
        return;
    }
    //cout << (int)pSensorData->getSensorMode() << ", loop = " << pSensorData->getLoopNum() << endl;
    m_uiRealFullFrameFPS = pSensorData->getFullFrameFPS();
    m_uiTemperature = pSensorData->getTemperature();
    unsigned char* pBuffer = NULL;
    int colorMode = 0;
    if (pSensorData->getSensorMode() == CeleX5::Event_Address_Only_Mode)
    {
        if (0 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventBinaryPic);
        else if (1 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventDenoisedBinaryPic);
        else if (2 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventCountPic);
        else if (3 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventDenoisedCountPic);
    }
    else if (pSensorData->getSensorMode() == CeleX5::Event_Optical_Flow_Mode)
    {
        if (0 == m_iPicMode)
        {
            pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Pic);
            colorMode = 1;
        }
        else if (1 == m_iPicMode)
        {
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventBinaryPic);
        }
    }
    else if (pSensorData->getSensorMode() == CeleX5::Event_Intensity_Mode)
    {
        if (0 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventBinaryPic);
        else if (1 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventGrayPic);
        else if (2 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventAccumulatedPic);
        else if (3 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventSuperimposedPic);
        else if (4 == m_iPicMode)
            pBuffer = pSensorData->getEventPicBuffer(CeleX5::EventCountPic);
        //
        if (3 == m_iPicMode)
            colorMode = 4;
        else
            colorMode = 0;
    }
    else if (pSensorData->getSensorMode() == CeleX5::Full_Picture_Mode)
    {
        pBuffer = pSensorData->getFullPicBuffer();
    }
    else
    {
        if (m_bLoopModeEnabled)
        {
            pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Pic);
            colorMode = 1;
        }
        else
        {
            if (0 == m_iPicMode)
                pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Pic);
            else if (1 == m_iPicMode)
                pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Speed_Pic);
            else if (2 == m_iPicMode)
                pBuffer = pSensorData->getOpticalFlowPicBuffer(CeleX5::Full_Optical_Flow_Direction_Pic);

            colorMode = m_iPicMode + 1;
        }
    }
    updateImage(pBuffer, pSensorData->getSensorMode(), pSensorData->getLoopNum(), colorMode);

    if (m_emDisplayType == ConvertBin2Video)
    {
        char jpegfile[32]="temp.jpg";
        m_imageMode1.save("temp.jpg", "JPG");
        m_pVideoStream->avWtiter(jpegfile);
    }
    else if (m_emDisplayType == ConvertBin2CSV)
    {
        std::vector<EventData> vecEvent;
        m_pCeleX5->getEventDataVector(vecEvent);
        int dataSize = vecEvent.size();
        if (pSensorData->getSensorMode() == CeleX5::Event_Address_Only_Mode)
        {
            for (int i = 0; i < dataSize; i++)
            {
                g_ofWriteMat << vecEvent[i].row << ',' << vecEvent[i].col << ',' << vecEvent[i].t << endl;
            }
        }
        else if (pSensorData->getSensorMode() == CeleX5::Event_Intensity_Mode)
        {
            for (int i = 0; i < dataSize; i++)
            {
                g_ofWriteMat << vecEvent[i].row << ',' << vecEvent[i].col << ',' << vecEvent[i].brightness
                             << ',' << vecEvent[i].polarity << ',' <<  vecEvent[i].t << endl;
            }
        }
        else if (pSensorData->getSensorMode() == CeleX5::Event_Optical_Flow_Mode)
        {
            for (int i = 0; i < dataSize; i++)
            {
                g_ofWriteMat << vecEvent[i].row << ',' << vecEvent[i].col << ',' << vecEvent[i].brightness
                             << ',' <<  vecEvent[i].t << endl;
            }
        }
    }

    if (m_bSavePics)
    {
        if (pSensorData->getSensorMode() == CeleX5::Event_Address_Only_Mode)
        {
            updateEventImage(pSensorData->getEventPicBuffer(CeleX5::EventBinaryPic), 1);
            updateEventImage(pSensorData->getEventPicBuffer(CeleX5::EventCountPic), 2);
            updateEventImage(pSensorData->getEventPicBuffer(CeleX5::EventDenoisedBinaryPic), 3);
            updateEventImage(pSensorData->getEventPicBuffer(CeleX5::EventDenoisedCountPic), 4);
        }
        else if (pSensorData->getSensorMode() == CeleX5::Event_Intensity_Mode)
        {
            updateEventImage(pSensorData->getEventPicBuffer(CeleX5::EventBinaryPic), 1);
            updateEventImage(pSensorData->getEventPicBuffer(CeleX5::EventCountPic), 2);
            updateEventImage(pSensorData->getEventPicBuffer(CeleX5::EventGrayPic), 3);
        }
        savePics(pSensorData->getSensorMode());
    }

    if (m_emDisplayType != Realtime)
    {
        if (1000 / m_iPlaybackFPS > 1)
        {
#ifdef _WIN32
            Sleep(1000 / m_iPlaybackFPS);
#else
            usleep(1000000 / m_iPlaybackFPS);
#endif
        }
    }
}

void SensorDataObserver::setCeleX5(CeleX5 *pCeleX5)
{
    m_pCeleX5 = pCeleX5;
}

void SensorDataObserver::setLoopModeEnabled(bool enable)
{
    m_bLoopModeEnabled = enable;
}

void SensorDataObserver::setPictureMode(int picMode)
{
    m_iPicMode = picMode;
}

void SensorDataObserver::setDisplayFPS(int count)
{
    if (m_emDisplayType == Realtime)
    {
        m_iFPS = count;
        m_pUpdateTimer->start(1000/m_iFPS);
    }
    else
    {
        m_iPlaybackFPS = count;
    }
}

void SensorDataObserver::setFullFrameFPS(uint16_t value)
{
    m_uiFullFrameFPS = value;
}

void SensorDataObserver::setMultipleShowEnabled(bool enable)
{
    m_bShowMultipleWindows = enable;
}

void SensorDataObserver::setDisplayType(DisplayType type)
{
    m_lFullFrameCount = 0;
    m_lEventFrameCount = 0;
    m_lOpticalFrameCount = 0;
    m_emDisplayType = type;
    if (Realtime == type)
    {
        m_pUpdateTimer->start(1000/m_iFPS);
    }
    else
    {
        m_pUpdateTimer->stop();
    }
}

void SensorDataObserver::setSaveBmp(bool save)
{
    m_bSavePics = save;
}

bool SensorDataObserver::isSavingBmp()
{
    return m_bSavePics;
}

void SensorDataObserver::setBinFileName(QString filePath)
{
    QStringList nameList = filePath.split("/");

    QString fileName = nameList[nameList.size()-1];

    if (fileName.size() > 15)
        m_qsBinFileName = fileName.mid(9, 15);
    else
        m_qsBinFileName = fileName.left(filePath.size() -4);
}

void SensorDataObserver::setPicIntervalCount(int count)
{
    m_iPicIntervalCount = count;
}

void SensorDataObserver::setRecordingImgState(bool state)
{
    m_bRecordingImages = state;
    m_lFullFrameCount = 0;
    m_lEventFrameCount = 0;
}

void SensorDataObserver::setRecordingTime(QString time)
{
    m_qsRecordingTime = time;
}

void SensorDataObserver::updateImage(unsigned char *pBuffer1, CeleX5::CeleX5Mode mode, int loopNum, int colorMode)
{
#ifdef _LOG_TIME_CONSUMING_
    struct timeval tv_begin, tv_end;
    gettimeofday(&tv_begin, NULL);
#endif    
    if (NULL == pBuffer1)
    {
        return;
    }
    //cout << "mode = " << mode << ", loopNum = " << loopNum << endl;
    uchar* pp1 = NULL;
    if (loopNum == 1)
        pp1 = m_imageMode1.bits();
    else if (loopNum == 2)
        pp1 = m_imageMode2.bits();
    else if (loopNum == 3)
        pp1 = m_imageMode3.bits();
    else if (loopNum == 4)
        pp1 = m_imageMode4.bits();
    else
        pp1 = m_imageMode1.bits();

    //cout << "type = " << (int)m_pCeleX5->getFixedSenserMode() << endl;
    int value = 0;
    for (int i = 0; i < CELEX5_ROW; ++i)
    {
        for (int j = 0; j < CELEX5_COL; ++j)
        {
            value = pBuffer1[i*CELEX5_COL+j];
            if (1 == colorMode) //optical-flow frame
            {
                if (0 == value)
                {
                    *pp1 = 0;
                    *(pp1+1) = 0;
                    *(pp1+2) = 0;
                }
                else if (value < 50) //blue
                {
                    *pp1 = 0;
                    *(pp1+1) = 0;
                    *(pp1+2) = 255;
                }
                else if (value < 100)
                {
                    *pp1 = 0;
                    *(pp1+1) = 255;
                    *(pp1+2) = 255;
                }
                else if (value < 150) //green
                {
                    *pp1 = 0;
                    *(pp1+1) = 255;
                    *(pp1+2) = 0;
                }
                else if (value < 200) //yellow
                {
                    *pp1 = 255;
                    *(pp1+1) = 255;
                    *(pp1+2) = 0;
                }
                else //red
                {
                    *pp1 = 255;
                    *(pp1+1) = 0;
                    *(pp1+2) = 0;
                }
            }
            else if (2 == colorMode) //optical-flow speed
            {
                if (0 == value)
                {
                    *pp1 = 0;
                    *(pp1+1) = 0;
                    *(pp1+2) = 0;
                }
                else if (value < 20) //red
                {
                    *pp1 = 255;
                    *(pp1+1) = 0;
                    *(pp1+2) = 0;
                }
                else if (value < 40) //yellow
                {
                    *pp1 = 255;
                    *(pp1+1) = 255;
                    *(pp1+2) = 0;
                }
                else if (value < 60) //green
                {
                    *pp1 = 0;
                    *(pp1+1) = 255;
                    *(pp1+2) = 0;
                }
                else if (value < 80) //green blue
                {
                    *pp1 = 0;
                    *(pp1+1) = 255;
                    *(pp1+2) = 255;
                }
                else //blue
                {
                    *pp1 = 0;
                    *(pp1+1) = 0;
                    *(pp1+2) = 255;
                }
            }
            else if (3 == colorMode) //optical-flow direction
            {
                if (0 == value)
                {
                    *pp1 = 0;
                    *(pp1+1) = 0;
                    *(pp1+2) = 0;
                }
                else if (value < 21 || value > 210) //30 300 red
                {
                    *pp1 = 255;
                    *(pp1+1) = 0;
                    *(pp1+2) = 0;
                }
                else if (value > 32 && value <= 96) //45 135 blue
                {
                    *pp1 = 0;
                    *(pp1+1) = 0;
                    *(pp1+2) = 255;
                }
                else if (value > 96 && value <= 159) //135 225 green
                {
                    *pp1 = 0;
                    *(pp1+1) = 255;
                    *(pp1+2) = 0;
                }
                else if (value > 159 && value < 223) //225 315 yellow
                {
                    *pp1 = 255;
                    *(pp1+1) = 255;
                    *(pp1+2) = 0;
                }
                else
                {
                    *pp1 = 0;
                    *(pp1+1) = 0;
                    *(pp1+2) = 0;
                }
            }
            else if (4 == colorMode) //event superimposed pic
            {
                if (255 == value)
                {
                    *pp1 = 0;
                    *(pp1+1) = 255;
                    *(pp1+2) = 0;
                }
                else
                {
                    *pp1 = value;
                    *(pp1+1) = value;
                    *(pp1+2) = value;
                }
            }
            else
            {
                *pp1 = value;
                *(pp1+1) = value;
                *(pp1+2) = value;
            }
            pp1+= 3;
        }
    }

#ifdef _LOG_TIME_CONSUMING_
    gettimeofday(&tv_end, NULL);
    //    cout << "updateImage time = " << tv_end.tv_usec - tv_begin.tv_usec << endl;
#endif
    update();
}

void SensorDataObserver::updateEventImage(unsigned char *pBuffer, int index)
{
    if (NULL == pBuffer)
    {
        return;
    }
    //cout << "mode = " << mode << ", loopNum = " << loopNum << endl;
    uchar* pp1 = NULL;
    if (1 == index)
        pp1 = m_imageEvent1.bits();
    else if (2 == index)
        pp1 = m_imageEvent2.bits();
    else if (3 == index)
        pp1 = m_imageEvent3.bits();
    else if (4 == index)
        pp1 = m_imageEvent4.bits();

    //cout << "type = " << (int)m_pCeleX5->getFixedSenserMode() << endl;
    int value = 0;
    for (int i = 0; i < CELEX5_ROW; ++i)
    {
        for (int j = 0; j < CELEX5_COL; ++j)
        {
            value = pBuffer[i*CELEX5_COL+j];
            *pp1 = value;
            *(pp1+1) = value;
            *(pp1+2) = value;
            pp1+= 3;
        }
    }
}

void SensorDataObserver::saveRecordingImage(unsigned char *pBuffer, int index)
{
    if (NULL == pBuffer)
    {
        return;
    }
    uchar* pp = NULL;
    if (1 == index)
        pp = m_imageEvent1.bits();
    else if (2 == index)
        pp = m_imageEvent2.bits();

    int value = 0;
    for (int i = 0; i < CELEX5_ROW; ++i)
    {
        for (int j = 0; j < CELEX5_COL; ++j)
        {
            value = pBuffer[i*CELEX5_COL+j];
            *pp = value;
            pp+= 1;
        }
    }
    if (m_lFullFrameCount > 999999 || m_lEventFrameCount > 999999)
    {
        m_lFullFrameCount = 0;
        m_lEventFrameCount = 0;

        const QDateTime now = QDateTime::currentDateTime();
        m_qsRecordingTime = now.toString(QLatin1String("yyyyMMdd_hhmmss"));
    }

    if (index == 1)
    {
        //-------- Full_Picture_Mode --------
        QString qsNum1 = QString("%1").arg(m_lFullFrameCount, 6, 10, QChar('0'));
        QString name1 = QCoreApplication::applicationDirPath() + "/image_fullpic/" + m_qsRecordingTime + "_" + qsNum1 + ".jpg";
        char nn1[256] = {0};
        memcpy(nn1, name1.toStdString().c_str(), name1.size());
        m_imageEvent1.save(nn1, "JPG");
        m_lFullFrameCount++;
    }
    else
    {
        //-------- Event_Address_Only_Mode --------
        QString qsNum2 = QString("%1").arg(m_lEventFrameCount, 6, 10, QChar('0'));
        QString name2 = QCoreApplication::applicationDirPath() + "/image_event_count/" + m_qsRecordingTime + "_" + qsNum2 + ".jpg";
        char file_path2[256] = {0};
        memcpy(file_path2, name2.toStdString().c_str(), name2.size());
        m_imageEvent2.save(file_path2, "JPG");
        m_lEventFrameCount++;
    }
}

void SensorDataObserver::savePics(CeleX5::CeleX5Mode mode)
{
    QDir dir;
    dir.cd(QCoreApplication::applicationDirPath());

    if (mode == CeleX5::Full_Picture_Mode)
    {
        m_iIntervalCounter++;
        if ((m_iIntervalCounter-1) % (m_iPicIntervalCount+1) != 0)
            return;

        if (!dir.exists("image_fullpic"))
        {
            dir.mkdir("image_fullpic");
        }
        QString qsNum = QString("%1").arg(m_lFullFrameCount, 6, 10, QChar('0'));
        QString name1 = QCoreApplication::applicationDirPath() + "/image_fullpic/" + m_qsBinFileName + "_" + qsNum + ".jpg";
        char nn1[256] = {0};
        memcpy(nn1, name1.toStdString().c_str(), name1.size());
        m_imageMode1.save(nn1, "JPG");
        m_lFullFrameCount++;
    }
    else if (mode == CeleX5::Event_Address_Only_Mode)
    {
        m_iIntervalCounter++;
        if ((m_iIntervalCounter-1) % (m_iPicIntervalCount+1) != 0)
            return;

        if (!dir.exists("image_event_binary"))
        {
            dir.mkdir("image_event_binary");
        }
        if (!dir.exists("image_event_count"))
        {
            dir.mkdir("image_event_count");
        }
        if (!dir.exists("image_denoised_event_binary"))
        {
            dir.mkdir("image_denoised_event_binary");
        }
        if (!dir.exists("image_denoised_event_count"))
        {
            dir.mkdir("image_denoised_event_count");
        }

        QString qsNum = QString("%1").arg(m_lEventFrameCount, 6, 10, QChar('0'));

        QString name1 = QCoreApplication::applicationDirPath() + "/image_event_binary/" + m_qsBinFileName + "_" + qsNum + ".jpg";
        char file_path1[256] = {0};
        memcpy(file_path1, name1.toStdString().c_str(), name1.size());
        m_imageEvent1.save(file_path1, "JPG");

        QString name2 = QCoreApplication::applicationDirPath() + "/image_event_count/" + m_qsBinFileName + "_" + qsNum + ".jpg";
        char file_path2[256] = {0};
        memcpy(file_path2, name2.toStdString().c_str(), name2.size());
        m_imageEvent2.save(file_path2, "JPG");

        QString name3 = QCoreApplication::applicationDirPath() + "/image_denoised_event_binary/" + m_qsBinFileName + "_" + qsNum + ".jpg";
        char file_path3[256] = {0};
        memcpy(file_path3, name3.toStdString().c_str(), name3.size());
        m_imageEvent3.save(file_path3, "JPG");

        QString name4 = QCoreApplication::applicationDirPath() + "/image_denoised_event_count/" + m_qsBinFileName + "_" + qsNum + ".jpg";
        char file_path4[256] = {0};
        memcpy(file_path4, name4.toStdString().c_str(), name4.size());
        m_imageEvent4.save(file_path4, "JPG");

        m_lEventFrameCount++;
    }
    else if (mode == CeleX5::Event_Intensity_Mode)
    {
        m_iIntervalCounter++;
        if ((m_iIntervalCounter-1) % (m_iPicIntervalCount+1) != 0)
            return;

        if (!dir.exists("image_event_binary"))
        {
            dir.mkdir("image_event_binary");
        }
        if (!dir.exists("image_event_count"))
        {
            dir.mkdir("image_event_count");
        }
        if (!dir.exists("image_event_gray"))
        {
            dir.mkdir("image_event_gray");
        }

        QString qsNum = QString("%1").arg(m_lEventFrameCount, 6, 10, QChar('0'));

        QString name1 = QCoreApplication::applicationDirPath() + "/image_event_binary/" + m_qsBinFileName + "_" + qsNum + ".jpg";
        char file_path1[256] = {0};
        memcpy(file_path1, name1.toStdString().c_str(), name1.size());
        m_imageEvent1.save(file_path1, "JPG");

        QString name2 = QCoreApplication::applicationDirPath() + "/image_event_count/" + m_qsBinFileName + "_" + qsNum + ".jpg";
        char file_path2[256] = {0};
        memcpy(file_path2, name2.toStdString().c_str(), name2.size());
        m_imageEvent2.save(file_path2, "JPG");

        QString name3 = QCoreApplication::applicationDirPath() + "/image_event_gray/" + m_qsBinFileName + "_" + qsNum + ".jpg";
        char file_path3[256] = {0};
        memcpy(file_path3, name3.toStdString().c_str(), name3.size());
        m_imageEvent3.save(file_path3, "JPG");

        m_lEventFrameCount++;
    }
    else if (mode == CeleX5::Full_Optical_Flow_S_Mode ||
             mode == CeleX5::Full_Optical_Flow_M_Mode ||
             mode == CeleX5::Event_Optical_Flow_Mode)
    {
        m_iIntervalCounter++;
        if ((m_iIntervalCounter-1) % (m_iPicIntervalCount+1) != 0)
            return;

        if (!dir.exists("image_optical"))
        {
            dir.mkdir("image_optical");
        }

        QString qsNum = QString("%1").arg(m_lOpticalFrameCount, 6, 10, QChar('0'));

        QString name1 = QCoreApplication::applicationDirPath() + "/image_optical/" + m_qsBinFileName + "_" + qsNum + ".jpg";
        char file_path[256] = {0};
        memcpy(file_path, name1.toStdString().c_str(), name1.size());
        m_imageMode1.save(file_path, "JPG");

        m_lOpticalFrameCount++;
    }
}

void SensorDataObserver::paintEvent(QPaintEvent *event)
{
#ifdef _LOG_TIME_CONSUMING_
    struct timeval tv_begin, tv_end;
    gettimeofday(&tv_begin, NULL);
#endif

    Q_UNUSED(event);
    QFont font;
    font.setPixelSize(20);
    QPainter painter(this);
    painter.setPen(QColor(255, 255, 255));
    painter.setFont(font);

    if (m_bLoopModeEnabled)
    {
        painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
        painter.drawPixmap(660, 0, QPixmap::fromImage(m_imageMode2).scaled(640, 400));
        painter.drawPixmap(0, 440, QPixmap::fromImage(m_imageMode3).scaled(640, 400));
    }
    else
    {
        if (m_bShowMultipleWindows)
        {
            if (m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Address_Only_Mode)
            {
                painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
                painter.fillRect(QRect(0, 0, 165, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 0, 165, 22), "Event Binary Pic");

                painter.drawPixmap(660, 0, QPixmap::fromImage(m_imageMode2).scaled(640, 400));
                painter.fillRect(QRect(660, 0, 255, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(660, 0, 255, 22), "Event Denoised Binary Pic");

                painter.drawPixmap(0, 440, QPixmap::fromImage(m_imageMode3).scaled(640, 400));
                painter.fillRect(QRect(0, 440, 160, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 440, 160, 22), "Event Count Pic");
            }
            else if (m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Optical_Flow_Mode)
            {
                painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
                painter.fillRect(QRect(0, 0, 230, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 0, 230, 22), "Event Optical-flow Pic");

                painter.drawPixmap(660, 0, QPixmap::fromImage(m_imageMode2).scaled(640, 400));
                painter.fillRect(QRect(660, 0, 165, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(660, 0, 165, 22), "Event Binary Pic");
            }
            else if (m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Intensity_Mode)
            {
                painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
                painter.fillRect(QRect(0, 0, 170, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 0, 170, 22), "Event Binary Pic");

                painter.drawPixmap(660, 0, QPixmap::fromImage(m_imageMode2).scaled(640, 400));
                painter.fillRect(QRect(660, 0, 200, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(660, 0, 200, 22), "Event Gray Pic");

                painter.drawPixmap(0, 440, QPixmap::fromImage(m_imageMode3).scaled(640, 400));
                painter.fillRect(QRect(0, 440, 230, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 440, 230, 22), "Event Accumulated Pic");

                painter.drawPixmap(660, 440, QPixmap::fromImage(m_imageMode4).scaled(640, 400));
                painter.fillRect(QRect(660, 440, 230, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(660, 440, 230, 22), "Event Superimposed Pic");
            }
            else if (m_pCeleX5->getSensorFixedMode() == CeleX5::Full_Optical_Flow_S_Mode)
            {
                painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
                painter.fillRect(QRect(0, 0, 170, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 0, 170, 22), "Optical-flow Pic");

                painter.drawPixmap(660, 0, QPixmap::fromImage(m_imageMode2).scaled(640, 400));
                painter.fillRect(QRect(660, 0, 230, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(660, 0, 230, 22), "Optical-flow Speed Pic");

                painter.drawPixmap(0, 440, QPixmap::fromImage(m_imageMode3).scaled(640, 400));
                painter.fillRect(QRect(0, 440, 270, 22), QBrush(Qt::darkBlue));
                painter.drawText(QRect(0, 440, 270, 22), "Optical-flow Direction Pic");
            }
            else
            {
                painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(640, 400));
            }
        }
        else
        {
            painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1));
            if (m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Address_Only_Mode ||
                m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Optical_Flow_Mode ||
                m_pCeleX5->getSensorFixedMode() == CeleX5::Event_Intensity_Mode)
            {
                //painter.fillRect(QRect(10, 10, 80, 30), QBrush(Qt::blue));
                //painter.drawText(QRect(14, 14, 80, 30), "T: " + QString::number(m_uiTemperature));
            }
            else
            {
                //painter.fillRect(QRect(10, 10, 120, 30), QBrush(Qt::blue));
                //painter.drawText(QRect(14, 14, 120, 30), "FPS: " + QString::number(m_uiRealFullFrameFPS) + "/" + QString::number(m_uiFullFrameFPS));
            }
        }
    }

#ifdef _LOG_TIME_CONSUMING_
    gettimeofday(&tv_end, NULL);
    //    cout << "paintEvent time = " << tv_end.tv_usec - tv_begin.tv_usec << endl;
#endif
}

void SensorDataObserver::onUpdateImage()
{
    if (m_pCeleX5->isLoopModeEnabled())
    {
        m_pCeleX5->getFullPicBuffer(m_pBuffer[0]);
        updateImage(m_pBuffer[0], CeleX5::Full_Picture_Mode, 1, 0);

        if (0 == m_iPicMode)
            m_pCeleX5->getEventPicBuffer(m_pBuffer[1], CeleX5::EventBinaryPic);
        else if (1 == m_iPicMode)
            m_pCeleX5->getEventPicBuffer(m_pBuffer[1], CeleX5::EventDenoisedBinaryPic);
        else if (2 == m_iPicMode)
            m_pCeleX5->getEventPicBuffer(m_pBuffer[1], CeleX5::EventCountPic);
        else if (3 == m_iPicMode)
            m_pCeleX5->getEventPicBuffer(m_pBuffer[1], CeleX5::EventDenoisedCountPic);
        updateImage(m_pBuffer[1], CeleX5::Event_Address_Only_Mode, 2, 0);

        m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[2], CeleX5::Full_Optical_Flow_Pic);
        updateImage(m_pBuffer[2], CeleX5::Full_Optical_Flow_S_Mode, 3, 1);

        if (m_bRecordingImages)
        {
            saveRecordingImage(m_pBuffer[0], 1);

            m_pCeleX5->getEventPicBuffer(m_pBuffer[3], CeleX5::EventCountPic);
            saveRecordingImage(m_pBuffer[3], 2);
        }
    }
    else
    {
        CeleX5::CeleX5Mode mode = m_pCeleX5->getSensorFixedMode();
        if (mode == CeleX5::Event_Address_Only_Mode)
        {
            if (m_bShowMultipleWindows)
            {
                m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventBinaryPic);
                m_pCeleX5->getEventPicBuffer(m_pBuffer[1], CeleX5::EventDenoisedBinaryPic);
                m_pCeleX5->getEventPicBuffer(m_pBuffer[2], CeleX5::EventCountPic);
                updateImage(m_pBuffer[0], mode, 1, 0);
                updateImage(m_pBuffer[1], mode, 2, 0);
                updateImage(m_pBuffer[2], mode, 3, 0);
            }
            else
            {
                if (0 == m_iPicMode)
                    m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventBinaryPic);
                else if (1 == m_iPicMode)
                    m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventDenoisedBinaryPic);
                else if (2 == m_iPicMode)
                    m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventCountPic);
                else if (3 == m_iPicMode)
                    m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventDenoisedCountPic);
                updateImage(m_pBuffer[0], mode, 1, 0);
            }
            if (m_bRecordingImages)
            {
                saveRecordingImage(m_pBuffer[0], 2);
            }
        }
        else if (mode == CeleX5::Event_Optical_Flow_Mode)
        {
            if (m_bShowMultipleWindows)
            {
                m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[0]);
                m_pCeleX5->getEventPicBuffer(m_pBuffer[1], CeleX5::EventBinaryPic);
                updateImage(m_pBuffer[0], mode, 1, 1);
                updateImage(m_pBuffer[1], mode, 2, 0);
            }
            else
            {
                if (0 == m_iPicMode)
                {
                    m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[0]);
                    updateImage(m_pBuffer[0], mode, 1, 1);
                }
                else if (1 == m_iPicMode)
                {
                    m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventBinaryPic);
                    updateImage(m_pBuffer[0], mode, 1, 0);
                }
            }
        }
        else if (mode == CeleX5::Event_Intensity_Mode)
        {
            if (m_bShowMultipleWindows)
            {
                m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventBinaryPic);
                m_pCeleX5->getEventPicBuffer(m_pBuffer[1], CeleX5::EventGrayPic);
                m_pCeleX5->getEventPicBuffer(m_pBuffer[2], CeleX5::EventAccumulatedPic);
                m_pCeleX5->getEventPicBuffer(m_pBuffer[3], CeleX5::EventSuperimposedPic);
                updateImage(m_pBuffer[0], mode, 1, 0);
                updateImage(m_pBuffer[1], mode, 2, 0);
                updateImage(m_pBuffer[2], mode, 3, 0);
                updateImage(m_pBuffer[3], mode, 4, 4);
            }
            else
            {
                if (0 == m_iPicMode)
                    m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventBinaryPic);
                else if (1 == m_iPicMode)
                    m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventGrayPic);
                else if (2 == m_iPicMode)
                    m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventAccumulatedPic);
                else if (3 == m_iPicMode)
                    m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventSuperimposedPic);
                else if (4 == m_iPicMode)
                    m_pCeleX5->getEventPicBuffer(m_pBuffer[0], CeleX5::EventCountPic);
                int colorMode = 0;
                if (3 == m_iPicMode)
                    colorMode = 4;
                updateImage(m_pBuffer[0], mode, 1, colorMode);
            }
        }
        else if (mode == CeleX5::Full_Picture_Mode)
        {
            m_pCeleX5->getFullPicBuffer(m_pBuffer[0]);
            updateImage(m_pBuffer[0], mode, 1, 0);
            if (m_bRecordingImages)
            {
                saveRecordingImage(m_pBuffer[0], 1);
            }
        }
        else
        {
            if (m_bShowMultipleWindows)
            {
                m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[0], CeleX5::Full_Optical_Flow_Pic);
                m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[1], CeleX5::Full_Optical_Flow_Speed_Pic);
                m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[2], CeleX5::Full_Optical_Flow_Direction_Pic);
                updateImage(m_pBuffer[0], mode, 1, 1);
                updateImage(m_pBuffer[1], mode, 2, 2);
                updateImage(m_pBuffer[2], mode, 3, 3);
            }
            else
            {
                if (0 == m_iPicMode)
                    m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[0], CeleX5::Full_Optical_Flow_Pic);
                else if (1 == m_iPicMode)
                    m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[0], CeleX5::Full_Optical_Flow_Speed_Pic);
                else if (2 == m_iPicMode)
                    m_pCeleX5->getOpticalFlowPicBuffer(m_pBuffer[0], CeleX5::Full_Optical_Flow_Direction_Pic);
                int colorMode = m_iPicMode+1;
                updateImage(m_pBuffer[0], mode, 1, colorMode);
            }
        }
        if(m_writer1.isOpened())
            m_writer1.write(cv::Mat(800,1280,CV_8UC1,m_pBuffer[0]));

        m_uiRealFullFrameFPS = m_pCeleX5->getFullFrameFPS();
    }
}

CeleX5Widget::CeleX5Widget(QWidget *parent)
    : QWidget(parent)
    , m_pCFGWidget(NULL)
    , m_pAdSettingWidget(NULL)
    , m_pCeleX5Cfg(NULL)
    , m_pPlaybackBg(NULL)
    , m_pVersionWidget(NULL)
    , m_pBtnPlayPause(NULL)
    , m_emDeviceType(CeleX5::CeleX5_MIPI)
    , m_emSensorFixedMode(CeleX5::Event_Address_Only_Mode)
    , m_uiSaveBinDuration(5)
    , m_bPlaybackPaused(false)
{
    m_pCeleX5 = new CeleX5;
    m_pCeleX5->openSensor(m_emDeviceType);

    m_pCeleX5->getCeleX5Cfg();
    m_mapCfgDefault = m_pCeleX5->getCeleX5Cfg();
    //
    m_pPipeOutDataTimer = new QTimer(this);

    if (m_emDeviceType == CeleX5::CeleX5_OpalKelly)
    {
        connect(m_pPipeOutDataTimer, SIGNAL(timeout()), this, SLOT(onPipeoutDataTimer()));
        m_pPipeOutDataTimer->start(1);
    }

    m_pReadBinTimer = new QTimer(this);
    //m_pReadBinTimer->setSingleShot(true);
    connect(m_pReadBinTimer, SIGNAL(timeout()), this, SLOT(onReadBinTimer()));

    m_pUpdatePlayInfoTimer = new QTimer(this);
    connect(m_pUpdatePlayInfoTimer, SIGNAL(timeout()), this, SLOT(onUpdatePlayInfo()));

    m_pRecordDataTimer = new QTimer(this);
    connect(m_pRecordDataTimer, SIGNAL(timeout()), this, SLOT(onRecordDataTimer()));

    QGridLayout* layoutBtn = new QGridLayout;
    //layoutBtn->setContentsMargins(0, 0, 0, 880);
    createButtons(layoutBtn);

    m_pSensorDataObserver = new SensorDataObserver(m_pCeleX5->getSensorDataServer(), this);
    m_pSensorDataObserver->show();
    //m_pSensorDataObserver->setGeometry(40, 130, 1280, 1000);
    m_pSensorDataObserver->setCeleX5(m_pCeleX5);

    //--- create comboBox to select image type
    int comboTop = 110;
    m_pCbBoxImageType = new QComboBox(this);
    QString style1 = "QComboBox {font: 18px Calibri; color: #FFFF00; border: 2px solid darkgrey; "
                     "border-radius: 5px; background: green;}";
    QString style2 = "QComboBox:editable {background: green;}";

    m_pCbBoxImageType->setGeometry(1070, comboTop, 250, 30);
    m_pCbBoxImageType->show();
    m_pCbBoxImageType->setStyleSheet(style1 + style2);
    m_pCbBoxImageType->insertItem(0, "Event Binary Pic");
    m_pCbBoxImageType->insertItem(1, "Event Denoised Binary Pic");
    m_pCbBoxImageType->insertItem(2, "Event Count Pic");
    m_pCbBoxImageType->insertItem(3, "Event Denoised Count Pic");
    m_pCbBoxImageType->setCurrentIndex(0);
    connect(m_pCbBoxImageType, SIGNAL(currentIndexChanged(int)), this, SLOT(onImageTypeChanged(int)));

    m_pCbBoxLoopEventType = new QComboBox(this);
    m_pCbBoxLoopEventType->setGeometry(1085, comboTop, 250, 30);
    m_pCbBoxLoopEventType->show();
    m_pCbBoxLoopEventType->setStyleSheet(style1 + style2);
    m_pCbBoxLoopEventType->insertItem(0, "Event Binary Pic");
    m_pCbBoxLoopEventType->insertItem(1, "Event Denoised Binary Pic");
    m_pCbBoxLoopEventType->insertItem(2, "Event Count Pic");
    m_pCbBoxLoopEventType->setCurrentIndex(0);
    connect(m_pCbBoxLoopEventType, SIGNAL(currentIndexChanged(int)), this, SLOT(onLoopEventTypeChanged(int)));
    m_pCbBoxLoopEventType->hide();

    //--- create comboBox to select sensor mode
    //Fixed Mode
    m_pCbBoxFixedMode = createModeComboBox("Fixed", QRect(40, comboTop, 300, 30), this, false, 0);
    m_pCbBoxFixedMode->setCurrentText("Fixed - Event_Address_Only Mode");
    m_pCbBoxFixedMode->show();
    //Loop A
    m_pCbBoxLoopAMode = createModeComboBox("LoopA", QRect(40, comboTop, 300, 30), this, true, 1);
    //Loop B
    m_pCbBoxLoopBMode = createModeComboBox("LoopB", QRect(40+660, comboTop, 300, 30), this, true, 2);
    //Loop C
    m_pCbBoxLoopCMode = createModeComboBox("LoopC", QRect(40, comboTop+440, 300, 30), this, true, 3);

    m_pBtnShowStyle = createButton("Show Multiple Windows", QRect(350, comboTop, 220, 30), this);
    connect(m_pBtnShowStyle, SIGNAL(released()), this, SLOT(onShowMultipleWindows()));

    QHBoxLayout* layoutSensorImage = new QHBoxLayout;
    layoutSensorImage->setContentsMargins(30, 0, 0, 0);
    layoutSensorImage->addWidget(m_pSensorDataObserver);

    QVBoxLayout* pMainLayout = new QVBoxLayout;
    pMainLayout->addLayout(layoutBtn);
    pMainLayout->addSpacing(65);
    //pMainLayout->addLayout(layoutComboBox);
    pMainLayout->addLayout(layoutSensorImage);
    this->setLayout(pMainLayout);

    m_pRowCycleSlider = new CfgSlider(this, 1, 100, 1, 6, this);
    m_pRowCycleSlider->setGeometry(1350, 150, 500, 70);
    m_pRowCycleSlider->setBiasType("Row Cycle Count");
    m_pRowCycleSlider->setDisplayName("Row Cycle Count");
    m_pRowCycleSlider->setObjectName("Row Cycle Count");
    m_pRowCycleSlider->hide();

    m_pEventCountSlider = new CfgSlider(this, 10000, 1000000, 1, 100000, this);
    m_pEventCountSlider->setGeometry(1350, 150, 500, 70);
    m_pEventCountSlider->setBiasType("Event Count");
    m_pEventCountSlider->setDisplayName("Event Count");
    m_pEventCountSlider->setObjectName("Event Count");
    m_pEventCountSlider->hide();

    m_pFrameSlider = new CfgSlider(this, 100, 1000000, 1, 30000, this);
    m_pFrameSlider->setGeometry(1350, 150, 500, 70);
    m_pFrameSlider->setBiasType("Frame Time");
    m_pFrameSlider->setDisplayName("   Frame Time (us)");
    m_pFrameSlider->setObjectName("Frame Time");

    m_pFPSSlider = new CfgSlider(this, 1, 1000, 1, 30, this);
    m_pFPSSlider->setGeometry(1350, 250, 500, 70);
    m_pFPSSlider->setBiasType("Display FPS");
    m_pFPSSlider->setDisplayName("   Display FPS (f/s)");
    m_pFPSSlider->setObjectName("Display FPS");
    //
    //--- create comboBox to select image type
    QComboBox* pEventShowType = new QComboBox(this);
    pEventShowType->setGeometry(1350, comboTop, 250, 30);
    pEventShowType->show();
    pEventShowType->setStyleSheet(style1 + style2);
    pEventShowType->insertItem(0, "Event Show By Time");
    pEventShowType->insertItem(1, "Event Show By Count");
    pEventShowType->insertItem(2, "Event Show By Row Cycle");
    pEventShowType->setCurrentIndex(0);
    connect(pEventShowType, SIGNAL(currentIndexChanged(int)), this, SLOT(onEventShowTypeChanged(int)));

    CfgSlider* pSlider3 = new CfgSlider(this, 1, 255, 1, 30, this);
    pSlider3->setGeometry(1350, 350, 500, 70);
    pSlider3->setBiasType("Event Count Step");
    pSlider3->setDisplayName("Event Count Step");
    pSlider3->setObjectName("Event Count Step");


    CfgSlider* pSlider4 = new CfgSlider(this, 0, 100, 1, 10, this);
    pSlider4->setGeometry(1350, 500, 500, 70);
    pSlider4->setBiasType("Event Start Pos");
    pSlider4->setDisplayName("Event Start Pos (ms)");
    pSlider4->setObjectName("Event Start Pos");
}

CeleX5Widget::~CeleX5Widget()
{
    if (m_pCeleX5)
    {
        delete m_pCeleX5;
        m_pCeleX5 = NULL;
    }
    if (m_pSensorDataObserver)
    {
        delete m_pSensorDataObserver;
        m_pSensorDataObserver = NULL;
    }
}

void CeleX5Widget::closeEvent(QCloseEvent *)
{
    cout << "CeleX5Widget::closeEvent" << endl;
    //QWidget::closeEvent(event);
    if (m_pCFGWidget)
    {
        m_pCFGWidget->close();
    }
    if (m_pAdSettingWidget)
    {
        m_pAdSettingWidget->close();
    }
}

void CeleX5Widget::playback(QPushButton *pButton)
{
    if ("Playback" == pButton->text())
    {
        m_pBtnShowStyle->hide();
        QString filePath = QFileDialog::getOpenFileName(this, "Open a bin file", QCoreApplication::applicationDirPath(), "Bin Files (*.bin)");
        if (filePath.isEmpty())
            return;

        if (m_pCeleX5->openBinFile(filePath.toStdString()))
        {
            pButton->setText("Exit Playback");
            setButtonEnable(pButton);
            //
            m_pSensorDataObserver->setBinFileName(filePath);
            m_pSensorDataObserver->setDisplayType(Playback);
            m_pSensorDataObserver->setMultipleShowEnabled(false);
            //
            showPlaybackBoard(true);
            //
            m_pBtnShowStyle->setText("Show Multiple Windows");

            QList<QPushButton*> button = this->findChildren<QPushButton *>("Enter Loop Mode");
            if (m_pCeleX5->isLoopModeEnabled())
            {
                switchMode(button[0], true, true);
            }
            else
            {
                m_pCeleX5->setLoopModeEnabled(false);
                switchMode(button[0], false, true);
                m_pCbBoxFixedMode->setCurrentIndex((int)m_pCeleX5->getSensorFixedMode());
            }
            //onReadBinTimer();
            //onUpdatePlayInfo();
            m_pReadBinTimer->start(READ_BIN_TIME);
            m_pUpdatePlayInfoTimer->start(UPDATE_PLAY_INFO_TIME);

            m_pCeleX5->pauseSensor(); //puase sensor: the sensor will not output data when playback
        }
    }
    else
    {
        pButton->setText("Playback");
        setButtonNormal(pButton);
        //
        m_pSensorDataObserver->setDisplayType(Realtime);
        m_pBtnShowStyle->show();
        showPlaybackBoard(false);
        //
        m_pCeleX5->restartSensor(); //restart sensor: the sensor will output data agatin
        m_pCeleX5->setIsPlayBack(false);
        m_pCeleX5->play();
        if (!m_pCeleX5->isLoopModeEnabled())
        {
            m_pCbBoxFixedMode->setCurrentIndex(m_emSensorFixedMode);
            m_pCeleX5->setSensorFixedMode(m_emSensorFixedMode);
        }
        else
        {
            m_pCeleX5->setLoopModeEnabled(true);
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Picture_Mode, 1);
            m_pCeleX5->setSensorLoopMode(CeleX5::Event_Address_Only_Mode, 2);
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Optical_Flow_S_Mode, 3);
        }
        m_bPlaybackPaused = false;
        m_pBtnPlayPause->setStyleSheet("QPushButton {background-color: #002F6F; background-image: url(:/images/player_pause.png); "
                                       "border-style: outset; border-radius: 10px; border-color: #002F6F;} "
                                       "QPushButton:pressed {background: #992F6F; background-image: url(:/images/player_play.png); }");
    }
}

QComboBox *CeleX5Widget::createModeComboBox(QString text, QRect rect, QWidget *parent, bool bLoop, int loopNum)
{
    QString style1 = "QComboBox {font: 18px Calibri; color: white; border: 2px solid darkgrey; "
                     "border-radius: 5px; background: green;}";
    QString style2 = "QComboBox:editable {background: green;}";

    QComboBox* pComboBoxMode = new QComboBox(parent);
    pComboBoxMode->setGeometry(rect);
    pComboBoxMode->show();
    pComboBoxMode->setStyleSheet(style1 + style2);
    QStringList modeList;
    if (bLoop)
    {
        if (loopNum == 1)
            modeList << "Full_Picture Mode";
        else if (loopNum == 2)
            modeList << "Event_Address_Only Mode";
        else if (loopNum == 3)
            modeList /*<< "Full_Picture Mode"*/<< "Full_Optical_Flow_S Mode" << "Full_Optical_Flow_M Mode" << "Event_Address_Only Mode";
    }
    else
    {
        modeList << "Event_Address_Only Mode" << "Event_Optical_Flow Mode" << "Event_Intensity Mode"
                 << "Full_Picture Mode" << "Full_Optical_Flow_S Mode"/* << "Full_Optical_Flow_Test Mode"*/;
    }

    for (int i = 0; i < modeList.size(); i++)
    {
        pComboBoxMode->insertItem(i, text+" - "+modeList.at(i));
    }
    pComboBoxMode->hide();
    connect(pComboBoxMode, SIGNAL(currentIndexChanged(QString)), this, SLOT(onSensorModeChanged(QString)));

    return pComboBoxMode;
}

void CeleX5Widget::createButtons(QGridLayout* layout)
{
    QStringList btnNameList;
    btnNameList.push_back("RESET");
    btnNameList.push_back("Generate FPN");
    btnNameList.push_back("Change FPN");
    btnNameList.push_back("Start Recording Bin");
    btnNameList.push_back("Start Recording Images");
    btnNameList.push_back("Playback");
    btnNameList.push_back("Enter Loop Mode");
    btnNameList.push_back("Configurations");
    btnNameList.push_back("Enable Auto ISP");
    //btnNameList.push_back("More Parameters ...");
    //btnNameList.push_back("Test: Save Pic");
    btnNameList.push_back("Rotate_LR");
    btnNameList.push_back("Rotate_UD");
    btnNameList.push_back("ConvertBin2Video");
    btnNameList.push_back("ConvertBin2CSV");
    btnNameList.push_back("Version");
    //btnNameList.push_back("Save Parameters");
    //btnNameList.push_back("Enable Anti-Flashlight");
    btnNameList.push_back("Advanced Setting");

    m_pButtonGroup = new QButtonGroup;
    int index = 0;
    for (int i = 0; i < btnNameList.count(); ++i)
    {
        QPushButton* pButton = createButton(btnNameList.at(i), QRect(20, 20, 100, 36), this);
        pButton->setObjectName(btnNameList.at(i));
        pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F;}");
        m_pButtonGroup->addButton(pButton, i);
        if ("Change FPN" == btnNameList.at(i) ||
            "Rotate_UD" == btnNameList.at(i) ||
            "ConvertBin2CSV" == btnNameList.at(i) ||
            "Start Recording Images" == btnNameList.at(i))
        {
            layout->addWidget(pButton, 1, index - 1);
        }
        else
        {
            layout->addWidget(pButton, 0, index);
            index++;
        }
    }
    connect(m_pButtonGroup, SIGNAL(buttonReleased(QAbstractButton*)), this, SLOT(onButtonClicked(QAbstractButton*)));
}

void CeleX5Widget::changeFPN()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open a FPN file", QCoreApplication::applicationDirPath(), "FPN Files (*.txt)");
    if (filePath.isEmpty())
        return;

    m_pCeleX5->setFpnFile(filePath.toStdString());
}

void CeleX5Widget::record(QPushButton* pButton)
{
    if ("Start Recording Bin" == pButton->text())
    {
        pButton->setText("Stop Recording Bin");
        setButtonEnable(pButton);
        //
        const QDateTime now = QDateTime::currentDateTime();
        const QString timestamp = now.toString(QLatin1String("yyyyMMdd_hhmmsszzz"));

        QString qstrBinName;
        if (CeleX5::CeleX5_OpalKelly == m_emDeviceType)
        {
            qstrBinName = QCoreApplication::applicationDirPath() + "/ParaData_" + timestamp;
        }
        else
        {
            qstrBinName = QCoreApplication::applicationDirPath() + "/MipiData_" + timestamp;
        }
        QStringList modeList;
        modeList << "_E_" << "_EO_" << "_EI_" << "_F_" << "_FO1_" << "_FO2_" << "_FO3_" << "_FO4_";
        if (m_pCeleX5->isLoopModeEnabled())
            qstrBinName += "_Loop_";
        else
            qstrBinName += modeList.at(int(m_pCeleX5->getSensorFixedMode()));

        qstrBinName += QString::number(m_pCeleX5->getClockRate());
        qstrBinName += "M.bin"; //MHz
        std::string filePath = qstrBinName.toStdString();
        m_pCeleX5->startRecording(filePath);

        m_pRecordDataTimer->start(m_uiSaveBinDuration*60*1000);
    }
    else
    {
        pButton->setText("Start Recording Bin");
        setButtonNormal(pButton);
        m_pCeleX5->stopRecording();
        m_pRecordDataTimer->stop();
    }
}

void CeleX5Widget::recordImages(QPushButton *pButton)
{
    if ("Start Recording Images" == pButton->text())
    {
        pButton->setText("Stop Recording Images");
        setButtonEnable(pButton);
        m_pSensorDataObserver->setRecordingImgState(true);

        const QDateTime now = QDateTime::currentDateTime();
        const QString timestamp = now.toString(QLatin1String("yyyyMMdd_hhmmss"));

        m_pSensorDataObserver->setRecordingTime(timestamp);

        QDir dir;
        dir.cd(QCoreApplication::applicationDirPath());
        if (!dir.exists("image_fullpic"))
        {
            dir.mkdir("image_fullpic");
        }
        if (!dir.exists("image_event_count"))
        {
            dir.mkdir("image_event_count");
        }
    }
    else
    {
        pButton->setText("Start Recording Images");
        setButtonNormal(pButton);
        m_pSensorDataObserver->setRecordingImgState(false);
    }
}

void CeleX5Widget::recordVideo(QPushButton* pButton)
{
    if ("Start Recording Video" == pButton->text())
    {
        pButton->setText("Stop Recording Video");
        setButtonEnable(pButton);

        const QDateTime now = QDateTime::currentDateTime();
        const QString timestamp = now.toString(QLatin1String("yyyyMMdd_hhmmsszzz"));

        QString qstrVideoName;
        if (CeleX5::CeleX5_OpalKelly == m_emDeviceType)
        {
            qstrVideoName = QCoreApplication::applicationDirPath() + "/ParaData_" + timestamp;
        }
        else
        {
            qstrVideoName = QCoreApplication::applicationDirPath() + "/MipiData_" + timestamp;
        }
        QStringList modeList;
        modeList << "_E_" << "_EO_" << "_EI_" << "_F_" << "_FO1_" << "_FO2_" << "_FO3_" << "_FO4_";
        qstrVideoName += modeList.at(int(m_pCeleX5->getSensorFixedMode()));

        qstrVideoName += QString::number(m_pCeleX5->getClockRate());
        qstrVideoName += "M.avi"; //MHz
        std::string filePath = qstrVideoName.toStdString();
        m_writer1.open(filePath, -1/*CV_FOURCC('X', 'V', 'I', 'D')*/, 60.0, cv::Size(1280, 800), false);

    }
    else
    {
        pButton->setText("Start Recording Video");
        setButtonNormal(pButton);
        m_writer1.release();
    }
}

void CeleX5Widget::switchMode(QPushButton* pButton, bool isLoopMode, bool bPlayback)
{
    if (isLoopMode)
    {
        m_pSensorDataObserver->setLoopModeEnabled(true);
        if (!bPlayback)
        {
            m_pCeleX5->setLoopModeEnabled(true);
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Picture_Mode, 1);
            m_pCeleX5->setSensorLoopMode(CeleX5::Event_Address_Only_Mode, 2);
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Optical_Flow_S_Mode, 3);
        }
        m_pCbBoxLoopAMode->show();
        m_pCbBoxLoopBMode->show();
        m_pCbBoxLoopCMode->show();
        //
        m_pCbBoxLoopEventType->show();
        m_pCbBoxImageType->hide();
        m_pCbBoxFixedMode->hide();
        m_pBtnShowStyle->hide();
        pButton->setText("Enter Fixed Mode");
        //
        //Loop A
        m_pCbBoxLoopAMode->setCurrentText("LoopA - Full_Picture Mode");
        //Loop B
        m_pCbBoxLoopBMode->setCurrentText("LoopB - Event_Address_Only Mode");
        //Loop C
        m_pCbBoxLoopCMode->setCurrentText("LoopC - Full_Optical_Flow_S Mode");
    }
    else
    {
        m_pSensorDataObserver->setLoopModeEnabled(false);
        if (!bPlayback)
        {
            m_pCeleX5->setLoopModeEnabled(false);
        }
        m_pCbBoxLoopAMode->hide();
        m_pCbBoxLoopBMode->hide();
        m_pCbBoxLoopCMode->hide();
        //
        m_pCbBoxLoopEventType->hide();
        m_pCbBoxImageType->show();
        m_pCbBoxFixedMode->show();
        if (m_pSensorDataObserver->getDisplayType() == Realtime)
        {
            m_pBtnShowStyle->show();
            m_pCeleX5->setSensorFixedMode(m_emSensorFixedMode);
        }
        pButton->setText("Enter Loop Mode");
    }
    m_pSensorDataObserver->update();
}

void CeleX5Widget::showCFG()
{
    if (!m_pCFGWidget)
    {
        m_pCFGWidget = new QWidget;
        m_pCFGWidget->setWindowTitle("Configuration Settings");
        m_pCFGWidget->setGeometry(300, 50, 1300, 850);

        QString style1 = "QGroupBox {"
                         "border: 2px solid #990000;"
                         "font: 20px Calibri; color: #990000;"
                         "border-radius: 9px;"
                         "margin-top: 0.5em;"
                         "background: rgba(50,0,0,10);"
                         "}";
        QString style2 = "QGroupBox::title {"
                         "subcontrol-origin: margin;"
                         "left: 10px;"
                         "padding: 0 3px 0 3px;"
                         "}";
        int groupWidth = 610;

        //----- Group 1 -----
        QGroupBox* speedGroup = new QGroupBox("Sensor Speed Parameters: ", m_pCFGWidget);
        speedGroup->setGeometry(10, 20, groupWidth, 110);
        speedGroup->setStyleSheet(style1 + style2);

        //----- Group 2 -----
        QGroupBox* picGroup = new QGroupBox("Sensor Control Parameters: ", m_pCFGWidget);
        picGroup->setGeometry(10, 150, groupWidth, 250);
        picGroup->setStyleSheet(style1 + style2);
        QStringList cfgList;
        //cfgList << "Clock" << "Brightness" << "Contrast" << "Threshold" << "ISO";
        cfgList << "Clock" << "Brightness" << "Threshold" << "ISO";
        int min[5]   = {20,  0,    50,  1};
        int max[5]   = {100, 1023, 511, 6};
        int value[5] = {100, 100,  171, 3};
        int step[5]  = {10,  1,    1,   1};
        int top[5]   = {50,  180,  250, 320};
        for (int i = 0; i < cfgList.size(); i++)
        {
            if ("ISO" == cfgList.at(i))
            {
                value[i] = m_pCeleX5->getISOLevel();
                max[i] = m_pCeleX5->getISOLevelCount();
            }
            else if ("Brightness" == cfgList.at(i))
            {
                value[i] = m_pCeleX5->getBrightness();
            }
            CfgSlider* pSlider = new CfgSlider(m_pCFGWidget, min[i], max[i], step[i], value[i], this);
            pSlider->setGeometry(10, top[i], 600, 70);
            pSlider->setBiasType(QString(cfgList.at(i)).toStdString());
            pSlider->setDisplayName(cfgList.at(i));
            pSlider->setObjectName(cfgList.at(i));
        }

        //----- Group 3 -----
        QGroupBox* loopGroup = new QGroupBox("Loop Mode Duration: ", m_pCFGWidget);
        loopGroup->setGeometry(670, 20, groupWidth, 380);
        loopGroup->setStyleSheet(style1 + style2);
        QStringList cfgList2;
        cfgList2 << "Event Duration" << "FullPic Num" << "S FO Pic Num"  << "M FO Pic Num";
        int min2[4] = {0};
        int max2[4] = {1023, 255, 255, 255};
        int value2[4] = {20, 1, 1, 3};
        for (int i = 0; i < cfgList2.size(); i++)
        {
            CfgSlider* pSlider = new CfgSlider(m_pCFGWidget, min2[i], max2[i], 1, value2[i], this);
            pSlider->setGeometry(670, 50+i*80, 600, 70);
            pSlider->setBiasType(QString(cfgList2.at(i)).toStdString());
            pSlider->setDisplayName(cfgList2.at(i));
            pSlider->setObjectName(cfgList2.at(i));
        }

        //----- Group 4 -----
        QGroupBox* autoISPGroup = new QGroupBox("Auto ISP Control Parameters: ", m_pCFGWidget);
        autoISPGroup->setGeometry(10, 440, 1270, 360);
        autoISPGroup->setStyleSheet(style1 + style2);

        //        m_pBtnGroup=new QButtonGroup(m_pCFGWidget);
        //        QRadioButton *level1 =new QRadioButton(tr("level1"),m_pCFGWidget);
        //        QRadioButton *level2=new QRadioButton(tr("level2"),m_pCFGWidget);
        //        QRadioButton *level3=new QRadioButton(tr("level3"),m_pCFGWidget);
        //        QRadioButton *level4=new QRadioButton(tr("level4"),m_pCFGWidget);

        //        level1->setGeometry(50,420,1270, 360);
        //        level2->setGeometry(150,420,1270, 360);
        //        level3->setGeometry(250,420,1270, 360);
        //        level4->setGeometry(350,420,1270, 360);

        //        m_pBtnGroup->addButton(level1,0);
        //        m_pBtnGroup->addButton(level2,1);
        //        m_pBtnGroup->addButton(level3,2);
        //        m_pBtnGroup->addButton(level4,3);

        //        level1->setChecked(1);

        //        connect(m_pBtnGroup, SIGNAL(buttonClicked(int)), this, SLOT(onRadioButtonClicked()));

        QStringList cfgList4;
        cfgList4 << "Brightness 1" << "Brightness 2" << "Brightness 3" << "Brightness 4"
                 << "BRT Threshold 1" << "BRT Threshold 2" << "BRT Threshold 3";
        int min4[7] = {0};
        int max4[7] = {1023, 1023, 1023, 1023, 4095, 4095, 4095};
        int value4[7] = {100, 110, 120, 130, 60, 500, 2500};
        int left4[7] = {10, 10, 10, 10, 670, 670, 670};
        int top4[7] = {470, 550, 630, 710, 470, 550, 630};
        for (int i = 0; i < cfgList4.size(); i++)
        {
            CfgSlider* pSlider = new CfgSlider(m_pCFGWidget, min4[i], max4[i], 1, value4[i], this);
            pSlider->setGeometry(left4[i], top4[i], 600, 70);
            pSlider->setBiasType(QString(cfgList4.at(i)).toStdString());
            pSlider->setDisplayName(cfgList4.at(i));
            pSlider->setObjectName(cfgList4.at(i));
        }
        m_pCFGWidget->setFocus();
    }
    QList<CfgSlider*> slider1 = m_pCFGWidget->findChildren<CfgSlider *>("Clock");
    if (slider1.size() > 0)
        slider1[0]->updateValue(m_pCeleX5->getClockRate());

    m_pCFGWidget->raise();
    m_pCFGWidget->show();
    if (m_pCFGWidget->isMinimized())
        m_pCFGWidget->showNormal();
}

void CeleX5Widget::showAdvancedSetting()
{
    if (!m_pAdSettingWidget)
    {
        m_pAdSettingWidget = new QWidget;
        m_pAdSettingWidget->setWindowTitle("Advanced Settings");
        m_pAdSettingWidget->setGeometry(300, 50, 800, 600);

        QString style1 = "QGroupBox {"
                         "border: 2px solid #990000;"
                         "font: 20px Calibri; color: #990000;"
                         "border-radius: 9px;"
                         "margin-top: 0.5em;"
                         "background: rgba(50,0,0,10);"
                         "}";
        QString style2 = "QGroupBox::title {"
                         "subcontrol-origin: margin;"
                         "left: 10px;"
                         "padding: 0 3px 0 3px;"
                         "}";

        QGroupBox* recordGroup = new QGroupBox("Data Record && Playback Parameters: ", m_pAdSettingWidget);
        recordGroup->setGeometry(50, 20, 700, 330);
        recordGroup->setStyleSheet(style1 + style2);
        //
        QLabel* pLabel = new QLabel(tr("Whether to display the images while recording"), m_pAdSettingWidget);
        pLabel->setGeometry(100, 50, 600, 50);
        pLabel->setStyleSheet("QLabel {background: transparent; font: 20px Calibri; }");

        QRadioButton *pRadioBtn = new QRadioButton(tr(" open"), m_pAdSettingWidget);
        pRadioBtn->setGeometry(120, 90, 600, 50);
        pRadioBtn->setStyleSheet("QRadioButton {background: transparent; color: #990000; "
                                 "font: 20px Calibri; }");
        pRadioBtn->setChecked(1);
        pRadioBtn->show();
        connect(pRadioBtn, SIGNAL(toggled(bool)), this, SLOT(onShowImagesSwitch(bool)));
        pRadioBtn->setObjectName("Display Switch");
        //
        QStringList cfgList1;
        cfgList1 << "BinFile Time Duration(min)" << "       SavePic Count Interval";
        QStringList cfgObj1;
        cfgObj1 << "Time Duration" << "Count Interval";
        int min1[4] = {1, 0};
        int max1[4] = {20, 10};
        int value1[4] = {5, 0};
        for (int i = 0; i < cfgList1.size(); i++)
        {
            CfgSlider* pSlider = new CfgSlider(m_pAdSettingWidget, min1[i], max1[i], 1, value1[i], this);
            pSlider->setGeometry(90, 180+i*90, 600, 70);
            pSlider->setBiasType(QString(cfgObj1.at(i)).toStdString());
            pSlider->setDisplayName(cfgList1.at(i));
            pSlider->setObjectName(cfgObj1.at(i));
        }

        QGroupBox* otherGroup = new QGroupBox("Other Parameters: ", m_pAdSettingWidget);
        otherGroup->setGeometry(50, 400, 700, 150);
        otherGroup->setStyleSheet(style1 + style2);
        //
        QStringList cfgList2;
        cfgList2 << "       Resolution Parameter";
        QStringList cfgObj2;
        cfgObj2 << "Resolution";
        int min2[1] = {0};
        int max2[1] = {255};
        int value2[1] = {0};
        for (int i = 0; i < cfgList2.size(); i++)
        {
            CfgSlider* pSlider = new CfgSlider(m_pAdSettingWidget, min2[i], max2[i], 1, value2[i], this);
            pSlider->setGeometry(90, 450+i*100, 600, 70);
            pSlider->setBiasType(QString(cfgObj2.at(i)).toStdString());
            pSlider->setDisplayName(cfgList2.at(i));
            pSlider->setObjectName(cfgObj2.at(i));
        }

        m_pAdSettingWidget->setFocus();
    }
    m_pAdSettingWidget->raise();
    m_pAdSettingWidget->show();
    if (m_pAdSettingWidget->isMinimized())
        m_pAdSettingWidget->showNormal();
}

void CeleX5Widget::setSliderMaxValue(QWidget *parent, QString objName, int value)
{
    for (int i = 0; i < parent->children().size(); ++i)
    {
        CfgSlider* pWidget = (CfgSlider*)parent->children().at(i);
        if (pWidget->objectName() == objName)
        {
            pWidget->setMaximum(value);
            return;
        }
    }
}

int CeleX5Widget::getSliderMax(QWidget *parent, QString objName)
{
    for (int i = 0; i < parent->children().size(); ++i)
    {
        CfgSlider* pWidget = (CfgSlider*)parent->children().at(i);
        if (pWidget->objectName() == objName)
        {
            return pWidget->maximum();
        }
    }
    return 0;
}

void CeleX5Widget::showPlaybackBoard(bool show)
{
    if (!m_pPlaybackBg)
    {
        m_pPlaybackBg = new QWidget(this);
        m_pPlaybackBg->setStyleSheet("background-color: lightgray; ");
        m_pPlaybackBg->setGeometry(1350, 500, 550, 300);

        CfgSlider* pSlider2 = new CfgSlider(m_pPlaybackBg, 0, 100000, 1, 0, this);
        pSlider2->setGeometry(0, 30, 500, 70);
        pSlider2->setBiasType("Package Count");
        pSlider2->setDisplayName("   Package Count");
        pSlider2->setObjectName("Package Count");

        int left = 140;
        int top = 130;
        int spacing = 20;
        QPushButton* m_pBtnReplay = new QPushButton("Replay", m_pPlaybackBg);
        m_pBtnReplay->setGeometry(left, top, 80, 30);
        m_pBtnReplay->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                                    "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                    "font: 20px Calibri; }"
                                    "QPushButton:pressed {background: #992F6F;}");
        connect(m_pBtnReplay, SIGNAL(released()), this, SLOT(onBtnReplayRelease()));

        left += m_pBtnReplay->width()+spacing;
        m_pBtnPlayPause = new QPushButton(m_pPlaybackBg);
        m_pBtnPlayPause->setGeometry(left, top, 80, 30);
        m_pBtnPlayPause->setStyleSheet("QPushButton {background-color: #002F6F; background-image: url(:/images/player_pause.png); "
                                       "border-style: outset; border-radius: 10px; border-color: #002F6F;} "
                                       "QPushButton:pressed {background: #992F6F; background-image: url(:/images/player_play.png); }");
        connect(m_pBtnPlayPause, SIGNAL(released()), this, SLOT(onBtnPlayPauseReleased()));

        left += m_pBtnPlayPause->width()+spacing;
        m_pBtnSavePic = new QPushButton("Start Saving Pic", m_pPlaybackBg);
        m_pBtnSavePic->setGeometry(left, top, 150, 30);
        m_pBtnSavePic->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                                     "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                     "font: 20px Calibri; }"
                                     "QPushButton:pressed {background: #992F6F;}");
        connect(m_pBtnSavePic, SIGNAL(released()), this, SLOT(onBtnSavePicReleased()));

        m_pBtnSavePicEx = new QPushButton("Start Saving Pic (Replay)", m_pPlaybackBg);
        m_pBtnSavePicEx->setGeometry(140, 200, 220, 30);
        m_pBtnSavePicEx->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                                     "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                     "font: 20px Calibri; }"
                                     "QPushButton:pressed {background: #992F6F;}");
        connect(m_pBtnSavePicEx, SIGNAL(released()), this, SLOT(onBtnSavePicExReleased()));
    }
    if (show)
        m_pPlaybackBg->show();
    else
        m_pPlaybackBg->hide();
}

void CeleX5Widget::convertBin2Video(QPushButton* pButton)
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open a bin file", QCoreApplication::applicationDirPath(), "Bin Files (*.bin)");
    if (filePath.isEmpty())
        return;

    showPlaybackBoard(true);
    m_pSensorDataObserver->setDisplayType(ConvertBin2Video);
    pButton->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                           "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                           "font: 20px Calibri; }"
                           "QPushButton:pressed {background: #992F6F; }");
    std::string path = filePath.left(filePath.size() - 4).toStdString();
#ifdef _WIN32
    path += ".mkv";
#else
    path += ".mp4";
#endif
    if (m_pCeleX5->openBinFile(filePath.toStdString()))
    {
        QList<QPushButton*> button=this->findChildren<QPushButton *>("Enter Loop Mode");

        if(m_pCeleX5->isLoopModeEnabled())
        {
            switchMode(button[0], true, true);
        }
        else
        {
            switchMode(button[0], false, true);
            m_pCbBoxFixedMode->setCurrentIndex((int)m_pCeleX5->getSensorFixedMode());
        }
        m_pReadBinTimer->start(READ_BIN_TIME);
        m_pUpdatePlayInfoTimer->start(UPDATE_PLAY_INFO_TIME);
        m_pVideoStream->avWriterInit(path.c_str());
    }
}

void CeleX5Widget::convertBin2CSV(QPushButton *pButton)
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open a bin file", QCoreApplication::applicationDirPath(), "Bin Files (*.bin)");
    if (filePath.isEmpty())
        return;

    showPlaybackBoard(true);
    m_pSensorDataObserver->setDisplayType(ConvertBin2CSV);
    pButton->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                           "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                           "font: 20px Calibri; }"
                           "QPushButton:pressed {background: #992F6F; }");
    std::string path = filePath.left(filePath.size() - 4).toStdString();
    path += ".csv";
    if (m_pCeleX5->openBinFile(filePath.toStdString()))
    {
        QList<QPushButton*> button=this->findChildren<QPushButton *>("Enter Loop Mode");

        if(m_pCeleX5->isLoopModeEnabled())
        {
            switchMode(button[0], true, true);
        }
        else
        {
            switchMode(button[0], false, true);
            m_pCbBoxFixedMode->setCurrentIndex((int)m_pCeleX5->getSensorFixedMode());
        }
        m_pReadBinTimer->start(READ_BIN_TIME);
        m_pUpdatePlayInfoTimer->start(UPDATE_PLAY_INFO_TIME);
        g_ofWriteMat.open(path.c_str());
    }
}

void CeleX5Widget::setCurrentPackageNo(int value)
{
    for (int i = 0; i < m_pPlaybackBg->children().size(); ++i)
    {
        CfgSlider* pWidget = (CfgSlider*)m_pPlaybackBg->children().at(i);
        if (pWidget->objectName() == "Package Count")
        {
            pWidget->updateValue(value);
            break;
        }
    }
}

void CeleX5Widget::onBtnRotateLRReleased(QPushButton* pButton)
{
    if (m_pCeleX5->getRotateType() >= 2)
    {
        m_pCeleX5->setRotateType(-2);
        pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F;}");
    }
    else
    {
        m_pCeleX5->setRotateType(2);
        pButton->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F; }");
    }
}

void CeleX5Widget::onBtnRotateUDReleased(QPushButton* pButton)
{
    if (m_pCeleX5->getRotateType()%2 == 1)
    {
        m_pCeleX5->setRotateType(-1);
        pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F;}");
    }
    else
    {
        m_pCeleX5->setRotateType(1);
        pButton->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F; }");
    }
}

void CeleX5Widget::onButtonClicked(QAbstractButton *button)
{
    //cout << "MainWindow::onButtonClicked: " << button->objectName().toStdString() << endl;
    if ("RESET" == button->objectName())
    {
        m_pCeleX5->reset();
    }
    else if ("Generate FPN" == button->objectName())
    {
        m_pCeleX5->generateFPN("");
    }
    else if ("Change FPN" == button->objectName())
    {
        changeFPN();
    }
    else if ("Start Recording Bin" == button->objectName())
    {
        record((QPushButton*)button);
    }
    else if ("Start Recording Images" == button->objectName())
    {
        recordImages((QPushButton*)button);
    }
    else if("Start Recording Video" == button->objectName())
    {
        recordVideo((QPushButton*)button);
    }
    else if ("Playback" == button->objectName())
    {
        playback((QPushButton*)button);
    }
    else if ("Enter Loop Mode" == button->objectName())
    {
        switchMode((QPushButton*)button, !m_pCeleX5->isLoopModeEnabled(), false);
    }
    else if ("Configurations" == button->objectName())
    {
        showCFG();
    }
    else if ("Enable Auto ISP" == button->objectName())
    {
        if (CeleX5::Full_Picture_Mode == m_pCeleX5->getSensorFixedMode() ||
                m_pCeleX5->isLoopModeEnabled())
        {
            if ("Enable Auto ISP" == button->text())
            {
                button->setText("Disable Auto ISP");
                m_pCeleX5->setAutoISPEnabled(true);
                setButtonEnable((QPushButton*)button);
            }
            else
            {
                button->setText("Enable Auto ISP");
                m_pCeleX5->setAutoISPEnabled(false);
                setButtonNormal((QPushButton*)button);
            }
        }
    }
    else if ("More Parameters ..." == button->objectName())
    {
        showMoreParameters(5);
    }
    else if ("Test: Save Pic" == button->objectName())
    {
        m_pCeleX5->saveFullPicRawData();
    }
    else if ("Rotate_LR" == button->objectName())
    {
        onBtnRotateLRReleased((QPushButton*)button);
    }
    else if ("Rotate_UD" == button->objectName())
    {
        onBtnRotateUDReleased((QPushButton*)button);
    }
    else if ("ConvertBin2Video" == button->objectName())
    {
        convertBin2Video((QPushButton*)button);
    }
    else if ("ConvertBin2CSV" == button->objectName())
    {
        convertBin2CSV((QPushButton*)button);
    }
    else if ("Enable Anti-Flashlight" == button->objectName())
    {
        m_pCeleX5->setAntiFlashlightEnabled(true);
    }
    else if ("Version" == button->objectName())
    {
        //m_pCeleDriver->writeSerialNumber("CX5-MP-0001-HXQ");
        //Sleep(1000);
        //std::string serial_number = m_pCeleX5->getSerialNumber();
        if (NULL == m_pVersionWidget)
        {
            m_pVersionWidget = new QWidget;
            m_pVersionWidget->setGeometry(600, 100, 500, 300);

            QLabel* pLabelSNum = new QLabel(m_pVersionWidget);
            pLabelSNum->setGeometry(10, 10, 400, 30);
            pLabelSNum->setText("Device Serial Number:   " + QString::fromStdString(m_pCeleX5->getSerialNumber()));
            pLabelSNum->setStyleSheet("QLabel { color: black; font: 20px Calibri; }");

            QLabel* pLabelFVer = new QLabel(m_pVersionWidget);
            //pLabelFVer->setGeometry(43, 10+40, 400, 30);
            pLabelFVer->setGeometry(10, 10+40, 400, 30);
            pLabelFVer->setText("Firmware Version:   " + QString::fromStdString(m_pCeleX5->getFirmwareVersion()));
            pLabelFVer->setStyleSheet("QLabel { color: black; font: 20px Calibri; }");

            QLabel* pLabelFDate = new QLabel(m_pVersionWidget);
            //pLabelFDate->setGeometry(66, 10+40*2, 400, 30);
            pLabelFDate->setGeometry(10, 10+40*2, 400, 30);
            pLabelFDate->setText("Firmware Date:   " + QString::fromStdString(m_pCeleX5->getFirmwareDate()));
            pLabelFDate->setStyleSheet("QLabel { color: black; font: 20px Calibri; }");

            QLabel* pLabelAPIVer = new QLabel(m_pVersionWidget);
            //pLabelAPIVer->setGeometry(94, 10+40*3, 400, 30);
            pLabelAPIVer->setGeometry(10, 10+40*3, 400, 30);
            pLabelAPIVer->setText("API Version:   1.3");
            pLabelAPIVer->setStyleSheet("QLabel { color: black; font: 20px Calibri; }");
        }
        m_pVersionWidget->show();
    }
    else if ("Advanced Setting" == button->objectName())
    {
        showAdvancedSetting();
    }
}

void CeleX5Widget::onRadioButtonClicked()
{
    cout<<m_pBtnGroup->checkedId()<<endl;
    switch(m_pBtnGroup->checkedId())
    {
    case 0:
        cout<<"level1---"<<endl;
        break;
    case 1:
        cout<<"level2---"<<endl;

        break;
    case 2:
        cout<<"level3---"<<endl;

        break;
    case 3:
        cout<<"level4---"<<endl;

        break;
    }
}

void CeleX5Widget::onValueChanged(uint32_t value, CfgSlider *slider)
{
    cout << "TestSensorWidget::onValueChanged: " << slider->getBiasType() << ", " << value << endl;
    if ("Clock" == slider->getBiasType())
    {
        m_pCeleX5->setClockRate(value);
        m_pSensorDataObserver->setFullFrameFPS(value);
    }
    else if ("Brightness" == slider->getBiasType())
    {
        m_pCeleX5->setBrightness(value);
    }
    else if ("Threshold" == slider->getBiasType())
    {
        m_pCeleX5->setThreshold(value);
    }
    else if ("ISO" == slider->getBiasType())
    {
        m_pCeleX5->setISOLevel(value); //the api will change fpn file according to the ISO level

        /*QString filePath = QCoreApplication::applicationDirPath() + "/FPN_" + QString::number(value) + ".txt";
        if (filePath.isEmpty())
            return;
        m_pCeleX5->setFpnFile(filePath.toStdString());*/
    }
    else if ("Event Duration" == slider->getBiasType())
    {
        m_pCeleX5->setEventDuration(value);
    }
    else if ("FullPic Num" == slider->getBiasType())
    {
        m_pCeleX5->setPictureNumber(value, CeleX5::Full_Picture_Mode);
    }
    else if ("S FO Pic Num" == slider->getBiasType())
    {
        m_pCeleX5->setPictureNumber(value, CeleX5::Full_Optical_Flow_S_Mode);
    }
    else if ("M FO Pic Num" == slider->getBiasType())
    {
        m_pCeleX5->setPictureNumber(value, CeleX5::Full_Optical_Flow_M_Mode);
    }
    else if ("Display FPS" == slider->getBiasType())
    {
        m_pSensorDataObserver->setDisplayFPS(value);
    }
    else if ("Package Count" == slider->getBiasType())
    {
        m_pCeleX5->setCurrentPackageNo(value);
        m_pReadBinTimer->start(READ_BIN_TIME);
        m_pUpdatePlayInfoTimer->start(UPDATE_PLAY_INFO_TIME);
    }
    else if ("Event Count Step" == slider->getBiasType())
    {
        m_pCeleX5->setEventCountStepSize(value);
    }
    else if ("Brightness 1" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 1);
    }
    else if ("Brightness 2" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 2);
    }
    else if ("Brightness 3" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 3);
    }
    else if ("Brightness 4" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 4);
    }
    else if ("BRT Threshold 1" == slider->getBiasType())
    {
        m_pCeleX5->setISPThreshold(value, 1);
    }
    else if ("BRT Threshold 2" == slider->getBiasType())
    {
        m_pCeleX5->setISPThreshold(value, 2);
    }
    else if ("BRT Threshold 3" == slider->getBiasType())
    {
        m_pCeleX5->setISPThreshold(value, 3);
    }
    else if ("Frame Time" == slider->getBiasType())
    {
        m_pCeleX5->setEventFrameTime(value);
    }
    else if ("Row Cycle Count" == slider->getBiasType())
    {
        m_pCeleX5->setEventShowMethod(EventShowByRowCycle, value);
    }
    else if ("Event Count" == slider->getBiasType())
    {
        m_pCeleX5->setEventShowMethod(EventShowByCount, value);
    }
    else if ("Time Duration" == slider->getBiasType())
    {
        m_uiSaveBinDuration = value;
    }
    else if ("Count Interval" == slider->getBiasType())
    {
        m_pSensorDataObserver->setPicIntervalCount(value);
    }
    else if ("Resolution" == slider->getBiasType())
    {
        m_pCeleX5->setRowDisabled(value);
    }
    else if ("Event Start Pos" == slider->getBiasType())
    {
        m_pCeleX5->setEventFrameStartPos(value);
    }
}

void CeleX5Widget::onPipeoutDataTimer()
{
    m_pCeleX5->pipeOutFPGAData();
}

void CeleX5Widget::onReadBinTimer()
{
    //cout << __FUNCTION__ << endl;
    if (m_emDeviceType == CeleX5::CeleX5_OpalKelly)
    {
        if (m_pCeleX5->readPlayBackData()) //if read finished
        {
            //m_pReadBinTimer->start(READ_BIN_TIME);
            m_pReadBinTimer->stop();
        }
    }
    else if (m_emDeviceType == CeleX5::CeleX5_MIPI)
    {
        if (m_pCeleX5->readBinFileData())
        {
            //m_pReadBinTimer->start(READ_BIN_TIME);
            m_pReadBinTimer->stop();
        }
    }
}

void CeleX5Widget::onRecordDataTimer()
{
    if (!m_pRecordDataTimer->isActive())
        return;

    m_pCeleX5->stopRecording();
    //
    const QDateTime now = QDateTime::currentDateTime();
    const QString timestamp = now.toString(QLatin1String("yyyyMMdd_hhmmsszzz"));
    QString qstrBinName;
    if (CeleX5::CeleX5_OpalKelly == m_emDeviceType)
    {
        qstrBinName = QCoreApplication::applicationDirPath() + "/ParaData_" + timestamp;
    }
    else
    {
        qstrBinName = QCoreApplication::applicationDirPath() + "/MipiData_" + timestamp;
    }
    QStringList modeList;
    modeList << "_E_" << "_EO_" << "_EI_" << "_F_" << "_FO1_" << "_FO2_" << "_FO3_" << "_FO4_";
    if (m_pCeleX5->isLoopModeEnabled())
        qstrBinName += "_Loop_";
    else
        qstrBinName += modeList.at(int(m_pCeleX5->getSensorFixedMode()));

    qstrBinName += QString::number(m_pCeleX5->getClockRate());
    qstrBinName += "M.bin"; //MHz
    std::string filePath = qstrBinName.toStdString();
    m_pCeleX5->startRecording(filePath);
}

void CeleX5Widget::onUpdatePlayInfo()
{
    if (m_pCeleX5->getPlaybackState() == PlayFinished)
    {
        cout << "------------- Playback Finished! -------------" << endl;
        if (m_pSensorDataObserver->getDisplayType() == ConvertBin2Video)
        {
            m_pVideoStream->avWriterRelease();
            QList<QPushButton*> button=this->findChildren<QPushButton *>("ConvertBin2Video");
            button[0]->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                                     "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                     "font: 20px Calibri; }"
                                     "QPushButton:pressed {background: #992F6F;}");
            QMessageBox::information(NULL, "convertBin2Video", "Convert Bin to Video completely!", QMessageBox::Yes, QMessageBox::Yes);
            m_pSensorDataObserver->setDisplayType(Realtime);
        }
        else if (m_pSensorDataObserver->getDisplayType() == ConvertBin2CSV)
        {
            g_ofWriteMat.close();
            QList<QPushButton*> button = this->findChildren<QPushButton *>("ConvertBin2CSV");
            button[0]->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                                     "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                     "font: 20px Calibri; }"
                                     "QPushButton:pressed {background: #992F6F;}");
            QMessageBox::information(NULL, "convertBin2CSV", "Convert Bin to CSV completely!", QMessageBox::Yes, QMessageBox::Yes);
            m_pSensorDataObserver->setDisplayType(Realtime);
        }

        m_pUpdatePlayInfoTimer->stop();
    }
    //cout << "--------- CurrentPackageNo = " << m_pCeleX5->getCurrentPackageNo() << endl;
    int value;
    if (m_pCeleX5->getTotalPackageCount() > 0xFFFFFF)
        value = 0xFFFFFF;
    else
        value = m_pCeleX5->getTotalPackageCount();
    setSliderMaxValue(m_pPlaybackBg, "Package Count", value);
    setCurrentPackageNo(m_pCeleX5->getCurrentPackageNo());
}

void CeleX5Widget::onBtnPlayPauseReleased()
{
    if (m_bPlaybackPaused)
    {
        m_bPlaybackPaused = false;
        m_pBtnPlayPause->setStyleSheet("QPushButton {background-color: #002F6F; background-image: url(:/images/player_pause.png); "
                                       "border-style: outset; border-radius: 10px; border-color: #002F6F;} "
                                       "QPushButton:pressed {background: #992F6F; background-image: url(:/images/player_play.png); }");
        m_pCeleX5->play();
    }
    else
    {
        m_bPlaybackPaused = true;
        m_pBtnPlayPause->setStyleSheet("QPushButton {background-color: #992F6F; background-image: url(:/images/player_play.png); "
                                       "border-style: outset;  border-radius: 10px; border-color: #992F6F;} "
                                       "QPushButton:pressed {background: #992F6F; background-image: url(:/images/player_play.png); }");
        m_pCeleX5->pause();
    }
}

void CeleX5Widget::onBtnReplayRelease()
{
    m_pReadBinTimer->stop();
    setCurrentPackageNo(0);
    m_pCeleX5->replay();
    m_pReadBinTimer->start(READ_BIN_TIME);
    m_pUpdatePlayInfoTimer->start(UPDATE_PLAY_INFO_TIME);
}

void CeleX5Widget::onBtnSavePicReleased()
{
    if (m_pSensorDataObserver->isSavingBmp())
    {
        m_pSensorDataObserver->setSaveBmp(false);
        m_pBtnSavePic->setText("Start Saving Pic");
        m_pBtnSavePic->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                                     "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                     "font: 20px Calibri; }"
                                     "QPushButton:pressed {background: #992F6F;}");
    }
    else
    {
        m_pSensorDataObserver->setSaveBmp(true);
        m_pBtnSavePic->setText("Stop Saving Pic");
        m_pBtnSavePic->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                                     "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                     "font: 20px Calibri; }"
                                     "QPushButton:pressed {background: #992F6F; }");
    }
}

void CeleX5Widget::onBtnSavePicExReleased()
{
    if (m_pSensorDataObserver->isSavingBmp())
    {
        m_pSensorDataObserver->setSaveBmp(false);
        m_pBtnSavePicEx->setText("Start Saving Pic (Replay)");
        m_pBtnSavePicEx->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                                       "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                       "font: 20px Calibri; }"
                                       "QPushButton:pressed {background: #992F6F;}");
    }
    else
    {
        onBtnReplayRelease();
        m_pSensorDataObserver->setSaveBmp(true);
        m_pBtnSavePicEx->setText("Stop Saving Pic (Replay)");
        m_pBtnSavePicEx->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                                       "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                                       "font: 20px Calibri; }"
                                       "QPushButton:pressed {background: #992F6F; }");
    }
}

void CeleX5Widget::onSensorModeChanged(QString text)
{
    cout << text.toStdString() << endl;
    if (m_pCeleX5->isLoopModeEnabled())
    {
        int loopNum = 0;
        if (text.contains("LoopA"))
            loopNum = 1;
        else if (text.contains("LoopB"))
            loopNum = 2;
        else if (text.contains("LoopC"))
            loopNum = 3;

        QString mode = text.mid(8);
        //cout << loopNum << ", " << mode.toStdString() << endl;
        if (mode == "Event_Address_Only Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Event_Address_Only_Mode, loopNum);
        else if (mode == "Event_Optical_Flow Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Event_Optical_Flow_Mode, loopNum);
        else if (mode == "Event_Intensity Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Event_Intensity_Mode, loopNum);
        else if (mode == "Full_Picture Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Picture_Mode, loopNum);
        else if (mode == "Full_Optical_Flow_S Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Optical_Flow_S_Mode, loopNum);
        else if (mode == "Full_Optical_Flow_M Mode")
            m_pCeleX5->setSensorLoopMode(CeleX5::Full_Optical_Flow_M_Mode, loopNum);
    }
    else
    {
        QString mode = text.mid(8);
        //cout << mode.toStdString() << endl;
        if (mode == "Event_Address_Only Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Event_Address_Only_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Event Binary Pic");
            m_pCbBoxImageType->insertItem(1, "Event Denoised Binary Pic");
            m_pCbBoxImageType->insertItem(2, "Event Count Pic");
            m_pCbBoxImageType->insertItem(3, "Event Denoised Count Pic");
            m_pCbBoxImageType->setCurrentIndex(0);
            m_pSensorDataObserver->setMultipleShowEnabled(false);
            m_pBtnShowStyle->setText("Show Multiple Windows");
        }
        else if (mode == "Event_Optical_Flow Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Event_Optical_Flow_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Event OpticalFlow Pic");
            m_pCbBoxImageType->insertItem(1, "Event Binary Pic");
        }
        else if (mode == "Event_Intensity Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Event_Intensity_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Event Binary Pic");
            m_pCbBoxImageType->insertItem(1, "Event Gray Pic");
            m_pCbBoxImageType->insertItem(2, "Event Accumulated Pic");
            m_pCbBoxImageType->insertItem(3, "Event Superimposed Pic");
            m_pCbBoxImageType->insertItem(4, "Event Count Pic");
            m_pCbBoxImageType->setCurrentIndex(1);
        }
        else if (mode == "Full_Picture Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Full_Picture_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full Pic");

            m_pSensorDataObserver->setMultipleShowEnabled(false);
            m_pBtnShowStyle->setText("Show Multiple Windows");
        }
        else if (mode == "Full_Optical_Flow_S Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_S_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full OpticalFlow Pic");
            m_pCbBoxImageType->insertItem(1, "Full OpticalFlow Speed Pic");
            m_pCbBoxImageType->insertItem(2, "Full OpticalFlow Direction Pic");
            m_pCbBoxImageType->setCurrentIndex(0);

            m_pSensorDataObserver->setMultipleShowEnabled(true);
            m_pBtnShowStyle->setText("Show Single Windows");
        }
        else if (mode == "Full_Optical_Flow_M Mode")
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_M_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full OpticalFlow Pic");
            m_pCbBoxImageType->insertItem(1, "Full OpticalFlow Speed Pic");
            m_pCbBoxImageType->insertItem(2, "Full OpticalFlow Direction Pic");
            m_pCbBoxImageType->setCurrentIndex(0);
        }
        else if (mode == "Full_Optical_Flow_Test Mode")
        {
            //m_pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_Test_Mode);
            m_pCbBoxImageType->clear();
            m_pCbBoxImageType->insertItem(0, "Full OpticalFlow Pic");
            m_pCbBoxImageType->insertItem(1, "Full OpticalFlow Speed Pic");
            m_pCbBoxImageType->insertItem(2, "Full OpticalFlow Direction Pic");
            m_pCbBoxImageType->setCurrentIndex(0);
        }
        if (m_pSensorDataObserver->getDisplayType() == Realtime)
        {
            m_emSensorFixedMode = m_pCeleX5->getSensorFixedMode();
        }
    }
}

void CeleX5Widget::onImageTypeChanged(int index)
{
    cout << "CeleX5Widget::onImageTypeChanged: " << index << endl;
    m_pSensorDataObserver->setPictureMode(index);
}

void CeleX5Widget::onLoopEventTypeChanged(int index)
{
    cout << "CeleX5Widget::onLoopEventTypeChanged: " << index << endl;
    m_pSensorDataObserver->setPictureMode(index);
}

void CeleX5Widget::onShowMultipleWindows()
{
    if ("Show Multiple Windows" == m_pBtnShowStyle->text())
    {
        m_pSensorDataObserver->setMultipleShowEnabled(true);
        m_pBtnShowStyle->setText("Show Single Windows");
    }
    else
    {
        m_pSensorDataObserver->setMultipleShowEnabled(false);
        m_pBtnShowStyle->setText("Show Multiple Windows");
    }
}

void CeleX5Widget::onEventShowTypeChanged(int index)
{
    cout << __FUNCTION__ << ": " << index << endl;
    if (0 == index) //show by time
    {
        m_pFrameSlider->show();
        m_pEventCountSlider->hide();
        m_pRowCycleSlider->hide();
        m_pCeleX5->setEventShowMethod(EventShowByTime, m_pFrameSlider->getValue());
    }
    else if (1 == index) //show by event count
    {
        m_pFrameSlider->hide();
        m_pEventCountSlider->show();
        m_pRowCycleSlider->hide();
        m_pCeleX5->setEventShowMethod(EventShowByCount, m_pEventCountSlider->getValue());
    }
    else if (2 == index)
    {
        m_pFrameSlider->hide();
        m_pEventCountSlider->hide();
        m_pRowCycleSlider->show();
        m_pCeleX5->setEventShowMethod(EventShowByRowCycle, m_pRowCycleSlider->getValue());
    }
}

void CeleX5Widget::onShowImagesSwitch(bool state)
{
    //cout << "CeleX5Widget::onShowImagesSwitch: state = " << state << endl;
    m_pCeleX5->setShowImagesEnabled(state);
    QList<QRadioButton*> radio1 = m_pAdSettingWidget->findChildren<QRadioButton *>("Display Switch");
    if (state)
    {
        if (radio1.size() > 0)
        {
            radio1[0]->setText(" open");
            radio1[0]->setStyleSheet("QRadioButton {background: transparent; color: #990000; "
                                     "font: 20px Calibri; }");
        }
    }
    else
    {
        if (radio1.size() > 0)
        {
            radio1[0]->setText(" close");
            radio1[0]->setStyleSheet("QRadioButton {background: transparent; color: gray; "
                                     "font: 20px Calibri; }");
        }
    }

}

QPushButton *CeleX5Widget::createButton(QString text, QRect rect, QWidget *parent)
{
    QPushButton* pButton = new QPushButton(text, parent);
    pButton->setGeometry(rect);

    pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                           "border-style: outset; border-width: 2px; border-radius: 10px; "
                           "font: 20px Calibri; }"
                           "QPushButton:pressed {background: #992F6F; }"
                           "QPushButton:disabled {background: #777777; color: lightgray;}");
    return pButton;
}

SliderWidget *CeleX5Widget::createSlider(CeleX5::CfgInfo cfgInfo, int value, QRect rect, QWidget *parent, QWidget *widgetSlot)
{
    SliderWidget* pSlider = new SliderWidget(parent, cfgInfo.min, cfgInfo.max, cfgInfo.step, value, widgetSlot, false);
    pSlider->setGeometry(rect);
    pSlider->setBiasType(cfgInfo.name);
    pSlider->setDisplayName(QString::fromStdString(cfgInfo.name));
    pSlider->setBiasAddr(cfgInfo.high_addr, cfgInfo.middle_addr, cfgInfo.low_addr);
    pSlider->setObjectName(QString::fromStdString(cfgInfo.name));
    pSlider->setDisabled(true);
    if (value < 0)
        pSlider->setLineEditText("--");
    return pSlider;
}

void CeleX5Widget::setButtonEnable(QPushButton *pButton)
{
    pButton->setStyleSheet("QPushButton {background: #008800; color: white; "
                           "border-style: outset; border-width: 2px; border-radius: 10px; "
                           "font: 20px Calibri; }"
                           "QPushButton:pressed {background: #992F6F; }"
                           "QPushButton:disabled {background: #777777; color: lightgray;}");
}

void CeleX5Widget::setButtonNormal(QPushButton *pButton)
{
    pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                           "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                           "font: 20px Calibri; }"
                           "QPushButton:pressed {background: #992F6F;}");
}

void CeleX5Widget::showMoreParameters(int index)
{
    if (!m_pCeleX5Cfg)
    {
        m_pCeleX5Cfg = new CeleX5Cfg(m_pCeleX5);
        //m_pCeleX5Cfg->setTestWidget(this);
    }
    m_pCeleX5Cfg->setCurrentIndex(index);
    m_pCeleX5Cfg->raise();
    m_pCeleX5Cfg->show();
}

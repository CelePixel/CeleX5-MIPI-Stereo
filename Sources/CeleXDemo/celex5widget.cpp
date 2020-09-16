#include "celex5widget.h"
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
//
QString            g_qsBinFileName;
long               g_lFrameCount_M = 0;
long               g_lFrameCount_S = 0;
int                g_iDeviceIndex = 0;

SensorDataObserver::SensorDataObserver(CX5SensorDataServer *sensorData, QWidget *parent)
    : QWidget(parent)
    , m_imageMode1(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageMode2(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageMode3(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_imageMode4(CELEX5_COL, CELEX5_ROW, QImage::Format_RGB888)
    , m_pCeleX5(NULL)
    , m_bLoopModeEnabled(false)
    , m_emDisplayPicType{Event_Binary_Pic, Event_Binary_Pic}
    , m_uiTemperature(0)
    , m_iFPS(30)
    , m_iPlaybackFPS(1000)
    , m_uiRealFullFrameFPS(0)
    , m_bSavePics(false)
    , m_emDisplayType(Realtime)
    , m_bMultiSensor(true)
{
    //m_pSensorData = sensorData;
    //m_pSensorData->registerData(this, CeleX5DataManager::CeleX_Frame_Data);

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
    //m_pSensorData->unregisterData(this, CeleX5DataManager::CeleX_Frame_Data);
    m_pCeleX5->getSensorDataServer(0)->unregisterData(this, CeleX5DataManager::CeleX_Frame_Data);
    m_pCeleX5->getSensorDataServer(1)->unregisterData(this, CeleX5DataManager::CeleX_Frame_Data);
    for (int i = 0; i < 4; i++)
    {
        delete[] m_pBuffer[i];
        m_pBuffer[i] = NULL;
    }
}

void SensorDataObserver::onFrameDataUpdated(CeleX5ProcessedData* pSensorData)
{
    if (m_emDisplayType == Realtime)
    {
        return;
    }
    //cout << __FUNCTION__ << endl;
    g_iDeviceIndex = pSensorData->getDeviceIndex();
    while (pSensorData->getDeviceIndex() == g_iDeviceIndex)
    {
        if (m_emDisplayType == Realtime)
        {
            return;
        }
#ifdef _WIN32
        Sleep(1);
#else
        usleep(10);
#endif
    }
    m_uiRealFullFrameFPS = pSensorData->getFullFrameFPS();

    int device_index = pSensorData->getDeviceIndex();
    //cout << __FUNCTION__ << ": device_index = " << device_index << endl;

    updatePicBuffer(device_index, pSensorData->getSensorMode(), m_pBuffer[device_index], m_emDisplayPicType[device_index]);
    updateImage(m_pBuffer[device_index], device_index+1, m_emDisplayPicType[device_index]);

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
        if(pSensorData->getSensorMode() == CeleX5::Event_Address_Only_Mode)
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
                g_ofWriteMat << vecEvent[i].row << ',' << vecEvent[i].col << ',' << vecEvent[i].adc
                             << ',' << vecEvent[i].polarity << ',' <<  vecEvent[i].t << endl;
            }
        }
        else if (pSensorData->getSensorMode() == CeleX5::Event_Optical_Flow_Mode)
        {
            for (int i = 0; i < dataSize; i++)
            {
                g_ofWriteMat << vecEvent[i].row << ',' << vecEvent[i].col << ',' << vecEvent[i].adc
                             << ',' <<  vecEvent[i].t << endl;
            }
        }
    }
    if (m_bSavePics)
        savePics(pSensorData->getDeviceIndex());

    if (m_emDisplayType == Playback)
    {
        if (1000 / m_iPlaybackFPS > 1)
        {
#ifdef _WIN32
            Sleep(1000 / m_iPlaybackFPS);
#else
            usleep(1000 / m_iPlaybackFPS);
#endif
        }
    }
}

void SensorDataObserver::setCeleX5(CeleX5 *pCeleX5)
{
    m_pCeleX5 = pCeleX5;
    m_pCeleX5->getSensorDataServer(0)->registerData(this, CeleX5DataManager::CeleX_Frame_Data);
    m_pCeleX5->getSensorDataServer(1)->registerData(this, CeleX5DataManager::CeleX_Frame_Data);
}

void SensorDataObserver::setLoopModeEnabled(bool enable)
{
    m_bLoopModeEnabled = enable;
}

void SensorDataObserver::setDisplayPicType(int sensor_index, QString type)
{
    if (type == "Full Pic")
    {
        m_emDisplayPicType[sensor_index] = Full_Pic;
    }
    else if (type == "Event Binary Pic")
    {
        m_emDisplayPicType[sensor_index] = Event_Binary_Pic;
    }
    else if (type == "Event Denoised Binary Pic")
    {
        m_emDisplayPicType[sensor_index] = Event_Denoised_Binary_Pic;
    }
    else if (type == "Event Count Pic")
    {
        m_emDisplayPicType[sensor_index] = Event_Count_Pic;
    }
    else if (type == "Event Gray Pic")
    {
        m_emDisplayPicType[sensor_index] = Event_Gray_Pic;
    }
    else if (type == "Event Accumulated Pic")
    {
        m_emDisplayPicType[sensor_index] = Event_Accumulated_Pic;
    }
    else if (type == "Event Superimposed Pic")
    {
        m_emDisplayPicType[sensor_index] = Event_superimposed_Pic;
    }
    else if (type == "Event OpticalFlow Pic")
    {
        m_emDisplayPicType[sensor_index] = Event_OpticalFlow_Pic;
    }
    else if (type == "Full OpticalFlow Pic")
    {
        m_emDisplayPicType[sensor_index] = Full_OpticalFlow_Pic;
    }
    else if (type == "Full OpticalFlow Speed Pic")
    {
        m_emDisplayPicType[sensor_index] = Full_OpticalFlow_Speed_Pic;
    }
    else if (type == "Full OpticalFlow Direction Pic")
    {
        m_emDisplayPicType[sensor_index] = Full_OpticalFlow_Direction_Pic;
    }
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

void SensorDataObserver::setDisplayType(DisplayType type)
{
    g_lFrameCount_M = 0;
    g_lFrameCount_S = 0;
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

void SensorDataObserver::updatePicBuffer(int sensor_index, CeleX5::CeleX5Mode mode, unsigned char *pBuffer, DisplayPicType picType)
{
    if (NULL == pBuffer)
        return;

    if (mode == CeleX5::Event_Address_Only_Mode)
    {
        if (Event_Binary_Pic == picType)
        {
            m_pCeleX5->getEventPicBuffer(pBuffer, CeleX5::EventBinaryPic, sensor_index);
        }
        else if (Event_Denoised_Binary_Pic == picType)
        {
            m_pCeleX5->getEventPicBuffer(pBuffer, CeleX5::EventDenoisedBinaryPic, sensor_index);
        }
        else if (Event_Count_Pic == picType)
        {
            m_pCeleX5->getEventPicBuffer(pBuffer, CeleX5::EventCountPic, sensor_index);
        }
    }
    else if (mode == CeleX5::Event_Optical_Flow_Mode)
    {
        if (Event_OpticalFlow_Pic == picType)
        {
            m_pCeleX5->getOpticalFlowPicBuffer(pBuffer, CeleX5::Full_Optical_Flow_Pic, sensor_index);
        }
    }
    else if (mode == CeleX5::Event_Intensity_Mode)
    {
        if (Event_Binary_Pic == picType)
        {
            m_pCeleX5->getEventPicBuffer(pBuffer, CeleX5::EventBinaryPic, sensor_index);
        }
        else if (Event_Gray_Pic == picType)
        {
            m_pCeleX5->getEventPicBuffer(pBuffer, CeleX5::EventGrayPic, sensor_index);
        }
        else if (Event_Accumulated_Pic == picType)
        {
            m_pCeleX5->getEventPicBuffer(pBuffer, CeleX5::EventAccumulatedPic, sensor_index);
        }
        else if (Event_superimposed_Pic == picType)
        {
            m_pCeleX5->getEventPicBuffer(pBuffer, CeleX5::EventSuperimposedPic, sensor_index);
        }
    }
    else if (mode == CeleX5::Full_Picture_Mode)
    {
        if (Full_Pic == picType)
        {
            m_pCeleX5->getFullPicBuffer(pBuffer, sensor_index);
        }
    }
    else
    {
        if (Full_OpticalFlow_Pic == picType)
        {
            m_pCeleX5->getOpticalFlowPicBuffer(pBuffer, CeleX5::Full_Optical_Flow_Pic, sensor_index);
        }
        else if (Full_OpticalFlow_Speed_Pic == picType)
        {
            m_pCeleX5->getOpticalFlowPicBuffer(pBuffer, CeleX5::Full_Optical_Flow_Speed_Pic, sensor_index);
        }
        else if (Full_OpticalFlow_Direction_Pic == picType)
        {
            m_pCeleX5->getOpticalFlowPicBuffer(pBuffer, CeleX5::Full_Optical_Flow_Direction_Pic, sensor_index);
        }
    }
}

void SensorDataObserver::updateImage(unsigned char *pBuffer, int loopNum, DisplayPicType picType)
{
#ifdef _LOG_TIME_CONSUMING_
    struct timeval tv_begin, tv_end;
    gettimeofday(&tv_begin, NULL);
#endif    
    if (NULL == pBuffer)
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
            value = pBuffer[i*CELEX5_COL+j];
            if (Event_OpticalFlow_Pic == picType ||
                    Full_OpticalFlow_Pic == picType) //optical-flow frame
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
            else if (Full_OpticalFlow_Speed_Pic == picType) //optical-flow speed
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
            else if (Full_OpticalFlow_Direction_Pic == picType) //optical-flow direction
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
            else if (Event_superimposed_Pic == picType) //event superimposed pic
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

void SensorDataObserver::savePics(int device_index)
{
    QString image_path;
    QDir dir;
    dir.cd(QCoreApplication::applicationDirPath());
    image_path = QCoreApplication::applicationDirPath();
    if (!dir.exists("images"))
    {
        dir.mkdir("images");
    }
    dir.cd("images");
    image_path += "/images/";
    if (!dir.exists(g_qsBinFileName))
    {
        dir.mkdir(g_qsBinFileName);
    }
    dir.cd(g_qsBinFileName);
    image_path += g_qsBinFileName;
    if (!dir.exists("image_M"))
    {
        dir.mkdir("image_M");
    }
    if (!dir.exists("image_S"))
    {
        dir.mkdir("image_S");
    }
    if (device_index == 0)
    {
        QString name_M = image_path + "/image_M/Pic";
        name_M += QString::number(g_lFrameCount_M);
        name_M += ".jpg";
        char m[256] = {0};
        memcpy(m, name_M.toStdString().c_str(), name_M.size());
        m_imageMode1.save(m, "JPG");
        ++g_lFrameCount_M;
    }
    else
    {
        QString name_S = image_path + "/image_S/Pic";

        name_S+= QString::number(g_lFrameCount_S);
        name_S += ".jpg";
        char s[256] = {0};
        memcpy(s, name_S.toStdString().c_str(), name_S.size());
        m_imageMode2.save(s, "JPG");
        ++g_lFrameCount_S;
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
        painter.drawPixmap(0, 0, QPixmap::fromImage(m_imageMode1).scaled(896, 560));
        painter.fillRect(QRect(0, 0, 200, 30), QBrush(Qt::blue));
        painter.drawText(QRect(0, 0, 200, 30), Qt::AlignVCenter, " Master Sensor Pic");

        painter.drawPixmap(940, 0, QPixmap::fromImage(m_imageMode2).scaled(896, 560));
        painter.fillRect(QRect(940, 0, 200, 30), QBrush(Qt::blue));
        painter.drawText(QRect(940, 0, 200, 30), Qt::AlignVCenter, " Slave Sensor Pic");

        for (int i = 0; i < MAX_SENSOR_NUM; i++)
        {
            int mode = m_pCeleX5->getSensorFixedMode(i);
            if (mode == CeleX5::Full_Picture_Mode ||
                mode == CeleX5::Full_Optical_Flow_S_Mode)
            {
                painter.fillRect(QRect(i*940+765, 0, 130, 30), QBrush(Qt::blue));
                painter.drawText(QRect(i*940+765, 0, 130, 30), Qt::AlignVCenter, " FPS: " + QString::number(m_uiRealFullFrameFPS) + "/" + QString::number(m_pCeleX5->getClockRate(0)));
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
    for (int i = 0; i < MAX_SENSOR_NUM; i++)
    {
        CeleX5::CeleX5Mode mode = m_pCeleX5->getSensorFixedMode(i);
        updatePicBuffer(i, mode, m_pBuffer[i], m_emDisplayPicType[i]);
        updateImage(m_pBuffer[i], i+1, m_emDisplayPicType[i]);
    }

    m_uiRealFullFrameFPS = m_pCeleX5->getFullFrameFPS();
}

CeleX5Widget::CeleX5Widget(QWidget *parent)
    : QWidget(parent)
    , m_pCFGWidget(NULL)
    , m_pCeleX5Cfg(NULL)
    , m_pSettingsWidget(NULL)
    , m_pPlaybackBg(NULL)
    , m_emDeviceType(CeleX5::CeleX5_MIPI)
    , m_bPlaybackPaused(false)
    , m_emSensorFixedMode{CeleX5::Event_Address_Only_Mode, CeleX5::Event_Address_Only_Mode}
{
    m_pCeleX5 = new CeleX5;
    m_pCeleX5->openSensor(m_emDeviceType);
    m_pCeleX5->getCeleX5Cfg();
    m_mapCfgDefault = m_pCeleX5->getCeleX5Cfg();

    m_pReadBinTimer = new QTimer(this);
    m_pReadBinTimer->setSingleShot(true);
    connect(m_pReadBinTimer, SIGNAL(timeout()), this, SLOT(onReadBinTimer()));

    m_pUpdatePlayInfoTimer = new QTimer(this);
    connect(m_pUpdatePlayInfoTimer, SIGNAL(timeout()), this, SLOT(onUpdatePlayInfo()));

    QGridLayout* layoutBtn = new QGridLayout;
    //layoutBtn->setContentsMargins(0, 0, 0, 880);
    createButtons(layoutBtn);

    m_pSensorDataObserver = new SensorDataObserver(m_pCeleX5->getSensorDataServer(0), this);
    m_pSensorDataObserver->show();
    //m_pSensorDataObserver->setGeometry(40, 130, 1280, 1000);
    m_pSensorDataObserver->setCeleX5(m_pCeleX5);

    QString style1 = "QComboBox {font: 18px Calibri; color: #FFFF00; border: 2px solid darkgrey; "
                     "border-radius: 5px; background: green;}";
    QString style2 = "QComboBox:editable {background: green;}";

    //--- create comboBox to select sensor mode
    //Fixed Mode
    m_pCbBoxFixedMode_M = createModeComboBox("M - Fixed", QRect(40, 115, 300, 30), this, false, 0);
    m_pCbBoxFixedMode_M->setCurrentText("Fixed - Event_Address_Only Mode");
    m_pCbBoxFixedMode_M->show();
    connect(m_pCbBoxFixedMode_M, SIGNAL(currentIndexChanged(QString)), this, SLOT(onSensorModeChanged_M(QString)));

    m_pCbBoxFixedMode_S = createModeComboBox("S - Fixed", QRect(40+940, 115, 300, 30), this, false, 0);
    m_pCbBoxFixedMode_S->setCurrentText("Fixed - Event_Address_Only Mode");
    m_pCbBoxFixedMode_S->show();
    connect(m_pCbBoxFixedMode_S, SIGNAL(currentIndexChanged(QString)), this, SLOT(onSensorModeChanged_S(QString)));

    //--- create comboBox to select image type
    int x[2] = {685, 685+940};
    for (int i = 0; i < MAX_SENSOR_NUM; i++)
    {
        m_pCbBoxImageType[i] = new QComboBox(this);
        m_pCbBoxImageType[i]->setGeometry(x[i], 115, 250, 30);
        m_pCbBoxImageType[i]->show();
        m_pCbBoxImageType[i]->setStyleSheet(style1 + style2);
        m_pCbBoxImageType[i]->insertItem(0, "Event Binary Pic");
        m_pCbBoxImageType[i]->insertItem(1, "Event Denoised Binary Pic");
        m_pCbBoxImageType[i]->insertItem(2, "Event Count Pic");
        m_pCbBoxImageType[i]->setCurrentIndex(0);
    }
    connect(m_pCbBoxImageType[0], SIGNAL(currentIndexChanged(QString)), this, SLOT(onImageTypeChanged_M(QString)));
    connect(m_pCbBoxImageType[1], SIGNAL(currentIndexChanged(QString)), this, SLOT(onImageTypeChanged_S(QString)));

    QHBoxLayout* layoutSensorImage = new QHBoxLayout;
    layoutSensorImage->setContentsMargins(30, 0, 0, 0);
    layoutSensorImage->addWidget(m_pSensorDataObserver);

    QVBoxLayout* pMainLayout = new QVBoxLayout;
    pMainLayout->addLayout(layoutBtn);
    pMainLayout->addSpacing(70);
    //pMainLayout->addLayout(layoutComboBox);
    pMainLayout->addLayout(layoutSensorImage);
    this->setLayout(pMainLayout);

    //--- create comboBox to select image type
    int topEventShowType = 750;
    int topLeft = 40;
    QComboBox* pEventShowType = new QComboBox(this);
    pEventShowType->setGeometry(topLeft, topEventShowType, 250, 30);
    pEventShowType->show();
    pEventShowType->setStyleSheet(style1 + style2);
    pEventShowType->insertItem(0, "Event Show By Time");
    pEventShowType->insertItem(1, "Event Show By Count");
    pEventShowType->insertItem(2, "Event Show By Row Cycle");
    pEventShowType->setCurrentIndex(0);
    connect(pEventShowType, SIGNAL(currentIndexChanged(int)), this, SLOT(onEventShowTypeChanged(int)));

    topEventShowType = 800;
    m_pRowCycleSlider = new CfgSlider(this, 1, 100, 1, 6, this);
    m_pRowCycleSlider->setGeometry(topLeft, topEventShowType, 500, 70);
    m_pRowCycleSlider->setBiasType("Row Cycle Count");
    m_pRowCycleSlider->setDisplayName("Row Cycle Count");
    m_pRowCycleSlider->setObjectName("Row Cycle Count");
    m_pRowCycleSlider->hide();

    m_pEventCountSlider = new CfgSlider(this, 10000, 1000000, 1, 100000, this);
    m_pEventCountSlider->setGeometry(topLeft, topEventShowType, 500, 70);
    m_pEventCountSlider->setBiasType("Event Count");
    m_pEventCountSlider->setDisplayName("Event Count");
    m_pEventCountSlider->setObjectName("Event Count");
    m_pEventCountSlider->hide();

    m_pFrameSlider = new CfgSlider(this, 100, 1000000, 1, 30000, this);
    m_pFrameSlider->setGeometry(topLeft, topEventShowType, 500, 70);
    m_pFrameSlider->setBiasType("Frame Time");
    m_pFrameSlider->setDisplayName("Frame Time (us)");
    m_pFrameSlider->setObjectName("Frame Time");

    m_pFPSSlider = new CfgSlider(this, 1, 1000, 1, 30, this);
    m_pFPSSlider->setGeometry(topLeft, topEventShowType+100, 500, 70);
    m_pFPSSlider->setBiasType("Display FPS");
    m_pFPSSlider->setDisplayName("Display FPS (f/s)");
    m_pFPSSlider->setObjectName("Display FPS");
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
}

QStringList g_qsBinFilePathList;
void CeleX5Widget::playback(QPushButton *pButton)
{
    if ("Playback" == pButton->text())
    {
        g_qsBinFilePathList = QFileDialog::getOpenFileNames(this, "Open a bin file", QCoreApplication::applicationDirPath(), "Bin Files (*.bin)");
        //qDebug() << __FUNCTION__ << filePathList << endl;
        bool bPlaybackSuccess = false;
        for (int i = 0; i < g_qsBinFilePathList.size(); i++)
        {
            QStringList nameList = g_qsBinFilePathList[0].split("/");
            g_qsBinFileName = nameList[nameList.size()-1];
            g_qsBinFileName = g_qsBinFileName.left(g_qsBinFileName.size()-6);
            //qDebug() << __FUNCTION__ << g_qsBinFileName << endl;
            QString filePath = g_qsBinFilePathList[i];
            if (filePath.isEmpty())
                return;
            if (m_pCeleX5->openBinFile(filePath.toStdString(), i))
            {
                bPlaybackSuccess = true;
            }
        }
        if (bPlaybackSuccess)
        {
            pButton->setText("Exit Playback");
            setButtonEnable(pButton);
            showPlaybackBoard(true);

            m_pSensorDataObserver->setDisplayType(Playback);
            m_pReadBinTimer->start(READ_BIN_TIME);
            m_pUpdatePlayInfoTimer->start(UPDATE_PLAY_INFO_TIME);

            m_pCbBoxFixedMode_S->setCurrentIndex((int)m_pCeleX5->getSensorFixedMode(1));
            m_pCbBoxFixedMode_M->setCurrentIndex((int)m_pCeleX5->getSensorFixedMode(0));

            //m_pCeleX5->pauseSensor(); //puase sensor: the sensor will not output data when playback
            m_pCeleX5->alignBinFileData();
            m_pCeleX5->setPlaybackEnabled(true);
        }
    }
    else
    {
        m_pCbBoxFixedMode_S->setCurrentIndex((int)m_emSensorFixedMode[1]);
        m_pCbBoxFixedMode_M->setCurrentIndex((int)m_emSensorFixedMode[0]);

        cout << (int)m_emSensorFixedMode[1] << ", " << m_emSensorFixedMode[0] << endl;

        pButton->setText("Playback");
        setButtonNormal(pButton);
        m_pSensorDataObserver->setDisplayType(Realtime);

        showPlaybackBoard(false);
        //m_pCeleX5->restartSensor(); //restart sensor: the sensor will output data agatin
        m_pCeleX5->setPlaybackEnabled(false);
        m_pCeleX5->play();
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
            modeList << "Full_Picture Mode"<< "Event_Address_Only Mode";
        else if (loopNum == 2)
            modeList << "Event_Address_Only Mode"/*<< "Full_Picture Mode"*/;
        else if (loopNum == 3)
            modeList /*<< "Full_Picture Mode"*/<< "Full_Optical_Flow_S Mode" << "Full_Optical_Flow_M Mode"/* << "Event_Address_Only Mode"*/;
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
    //connect(pComboBoxMode, SIGNAL(currentIndexChanged(QString)), this, SLOT(onSensorModeChanged(QString)));

    return pComboBoxMode;
}

void CeleX5Widget::createButtons(QGridLayout* layout)
{
    QStringList btnNameList;
    btnNameList.push_back("RESET");
    btnNameList.push_back("Generate FPN (M)");
    btnNameList.push_back("Change FPN (M)");
    btnNameList.push_back("Generate FPN (S)");
    btnNameList.push_back("Change FPN (S)");
    btnNameList.push_back("Start Recording Bin");
    btnNameList.push_back("Playback");
    //btnNameList.push_back("Enter Loop Mode");
    btnNameList.push_back("Configurations");
    btnNameList.push_back("Enable Auto ISP");
    //btnNameList.push_back("More Parameters ...");
    //btnNameList.push_back("Test: Save Pic");
    btnNameList.push_back("Rotate_LR");
    btnNameList.push_back("Rotate_UD");
    btnNameList.push_back("ConvertBin2Video");
    btnNameList.push_back("ConvertBin2CSV");
    //btnNameList.push_back("Save Parameters");

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
        if ("Change FPN (M)" == btnNameList.at(i) ||
            "Change FPN (S)" == btnNameList.at(i))
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

void CeleX5Widget::changeFPN(int device_index)
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open a FPN file", QCoreApplication::applicationDirPath(), "FPN Files (*.txt)");
    if (filePath.isEmpty())
        return;

    m_pCeleX5->setFpnFile(filePath.toStdString(), device_index);
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

        QString qstrBinName_M, qstrBinName_S;

        qstrBinName_M = qstrBinName_S = QCoreApplication::applicationDirPath() + "/MipiData_" + timestamp;

        QStringList modeList;
        modeList << "_E_" << "_EO_" << "_EI_" << "_F_" << "_FO1_" << "_FO2_" << "_FO3_" << "_FO4_";

        qstrBinName_M += modeList.at(int(m_pCeleX5->getSensorFixedMode(0)));
        qstrBinName_S += modeList.at(int(m_pCeleX5->getSensorFixedMode(1)));

        qstrBinName_M += QString::number(m_pCeleX5->getClockRate(0));
        qstrBinName_S += QString::number(m_pCeleX5->getClockRate(1));

        QString qstrBinNameM, qstrBinNameS;

        qstrBinNameM = qstrBinName_M + "M" + "_M.bin"; //MHz
        qstrBinNameS = qstrBinName_S + "M" + "_S.bin"; //MHz
        m_pCeleX5->startRecording(qstrBinNameM.toStdString(), 0);
        m_pCeleX5->startRecording(qstrBinNameS.toStdString(), 1);
    }
    else
    {
        pButton->setText("Start Recording Bin");
        setButtonNormal(pButton);
        m_pCeleX5->stopRecording();
    }
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

void CeleX5Widget::showCFG()
{
    if (!m_pCFGWidget)
    {
        m_pCFGWidget = new QWidget;
        m_pCFGWidget->setWindowTitle("Configuration Settings");
        m_pCFGWidget->setGeometry(300, 50, /*1300*/750, /*850*/500);

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
        int max[5]   = {160, 1023, 511, 6};
        int value[5] = {100, 100,  171, 4};
        int step[5]  = {10,  1,    1,   1};
        int top[5]   = {50,  180,  250, 320};
        for (int i = 0; i < cfgList.size(); i++)
        {
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
    m_pCFGWidget->raise();
    m_pCFGWidget->show();
    if (m_pCFGWidget->isMinimized())
        m_pCFGWidget->showNormal();
}

void CeleX5Widget::showSettingsWidget()
{
    if (!m_pSettingsWidget)
    {
        m_pSettingsWidget = new SettingsWidget(m_pCeleX5);
    }
    m_pSettingsWidget->setCurrentIndex(0);
    m_pSettingsWidget->raise();
    m_pSettingsWidget->show();
}

void CeleX5Widget::showPlaybackBoard(bool show)
{
    if (!m_pPlaybackBg)
    {
        m_pPlaybackBg = new QWidget(this);
        m_pPlaybackBg->setStyleSheet("background-color: lightgray; ");
        m_pPlaybackBg->setGeometry(980, 750, 900, 250);

        CfgSlider* pSlider2 = new CfgSlider(m_pPlaybackBg, 0, 100000, 1, 0, this);
        pSlider2->setGeometry(70, 30, 500, 70);
        pSlider2->setBiasType("Package Count");
        pSlider2->setDisplayName("   Package Count");
        pSlider2->setObjectName("Package Count");

        int left = 220;
        int top = 130;
        int spacing = 20;
        QString btnStyle = "QPushButton {background: #002F6F; color: white; "
                           "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                           "font: 20px Calibri; }"
                           "QPushButton:pressed {background: #992F6F;}";
        m_pBtnReplay = new QPushButton("Replay", m_pPlaybackBg);
        m_pBtnReplay->setGeometry(left, top, 80, 30);
        m_pBtnReplay->setStyleSheet(btnStyle);
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
        m_pBtnSavePic->setStyleSheet(btnStyle);
        connect(m_pBtnSavePic, SIGNAL(released()), this, SLOT(onBtnSavePicReleased()));

        m_pBtnSavePicEx = new QPushButton("Start Saving Pic (Replay)", m_pPlaybackBg);
        m_pBtnSavePicEx->setGeometry(left, 180, 220, 30);
        m_pBtnSavePicEx->setStyleSheet(btnStyle);
        connect(m_pBtnSavePicEx, SIGNAL(released()), this, SLOT(onBtnSavePicExReleased()));
    }
    if (show)
        m_pPlaybackBg->show();
    else
        m_pPlaybackBg->hide();

    if (show)
        m_pPlaybackBg->show();
    else
        m_pPlaybackBg->hide();
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
        m_pCeleX5->pauseSensor(); //puase sensor: the sensor will not output data when playback

        m_pCbBoxFixedMode_M->setCurrentIndex((int)m_pCeleX5->getSensorFixedMode(0));
        onReadBinTimer();
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
        m_pCeleX5->pauseSensor(); //puase sensor: the sensor will not output data when playback
        m_pCbBoxFixedMode_M->setCurrentIndex((int)m_pCeleX5->getSensorFixedMode(0));

        onReadBinTimer();
        m_pUpdatePlayInfoTimer->start(1000);

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

void CeleX5Widget::setSensorFixedMode(int device_index, QString mode)
{
    cout << __FUNCTION__ << ": device_index = " << device_index << ", mode = " << mode.toStdString() << endl;
    if (mode == "Event_Address_Only Mode")
    {
        if (Realtime == m_pSensorDataObserver->getDisplayType())
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Event_Address_Only_Mode, device_index);
            m_emSensorFixedMode[device_index] = CeleX5::Event_Address_Only_Mode;
        }

        m_pCbBoxImageType[device_index]->clear();
        m_pCbBoxImageType[device_index]->insertItem(0, "Event Binary Pic");
        m_pCbBoxImageType[device_index]->insertItem(1, "Event Denoised Binary Pic");
        m_pCbBoxImageType[device_index]->insertItem(2, "Event Count Pic");
        m_pCbBoxImageType[device_index]->setCurrentIndex(0);
    }
    else if (mode == "Event_Optical_Flow Mode")
    {
        if (Realtime == m_pSensorDataObserver->getDisplayType())
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Event_Optical_Flow_Mode, device_index);
            m_emSensorFixedMode[device_index] = CeleX5::Event_Optical_Flow_Mode;
        }

        m_pCbBoxImageType[device_index]->clear();
        m_pCbBoxImageType[device_index]->insertItem(0, "Event OpticalFlow Pic");
    }
    else if (mode == "Event_Intensity Mode")
    {
        if (Realtime == m_pSensorDataObserver->getDisplayType())
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Event_Intensity_Mode, device_index);
            m_emSensorFixedMode[device_index] = CeleX5::Event_Intensity_Mode;
        }

        m_pCbBoxImageType[device_index]->clear();
        m_pCbBoxImageType[device_index]->insertItem(0, "Event Binary Pic");
        m_pCbBoxImageType[device_index]->insertItem(1, "Event Gray Pic");
        m_pCbBoxImageType[device_index]->insertItem(2, "Event Accumulated Pic");
        m_pCbBoxImageType[device_index]->insertItem(3, "Event Superimposed Pic");
        m_pCbBoxImageType[device_index]->setCurrentIndex(1);
    }
    else if (mode == "Full_Picture Mode")
    {
        if (Realtime == m_pSensorDataObserver->getDisplayType())
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Full_Picture_Mode, device_index);
            m_emSensorFixedMode[device_index] = CeleX5::Full_Picture_Mode;
        }

        m_pCbBoxImageType[device_index]->clear();
        m_pCbBoxImageType[device_index]->insertItem(0, "Full Pic");
    }
    else if (mode == "Full_Optical_Flow_S Mode")
    {
        if (Realtime == m_pSensorDataObserver->getDisplayType())
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_S_Mode, device_index);
            m_emSensorFixedMode[device_index] = CeleX5::Full_Optical_Flow_S_Mode;
        }

        m_pCbBoxImageType[device_index]->clear();
        m_pCbBoxImageType[device_index]->insertItem(0, "Full OpticalFlow Pic");
        m_pCbBoxImageType[device_index]->insertItem(1, "Full OpticalFlow Speed Pic");
        m_pCbBoxImageType[device_index]->insertItem(2, "Full OpticalFlow Direction Pic");
        m_pCbBoxImageType[device_index]->setCurrentIndex(0);
    }
    else if (mode == "Full_Optical_Flow_M Mode")
    {
        if (Realtime == m_pSensorDataObserver->getDisplayType())
        {
            m_pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_M_Mode, device_index);
            m_emSensorFixedMode[device_index] = CeleX5::Full_Optical_Flow_M_Mode;
        }

        m_pCbBoxImageType[device_index]->clear();
        m_pCbBoxImageType[device_index]->insertItem(0, "Full OpticalFlow Pic");
        m_pCbBoxImageType[device_index]->insertItem(1, "Full OpticalFlow Speed Pic");
        m_pCbBoxImageType[device_index]->insertItem(2, "Full OpticalFlow Direction Pic");
        m_pCbBoxImageType[device_index]->setCurrentIndex(0);
    }
}

void CeleX5Widget::setRotateLRType(QPushButton* pButton)
{
    if (m_pCeleX5->getRotateType() >= 2)
    {
        m_pCeleX5->setRotateType(-2, -1);
        pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F;}");
    }
    else
    {
        m_pCeleX5->setRotateType(2, -1);
        pButton->setStyleSheet("QPushButton {background: #992F6F; color: yellow; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F; }");
    }
}

void CeleX5Widget::setRotateUDType(QPushButton* pButton)
{
    cout << __FUNCTION__ << ": " << m_pCeleX5->getRotateType() << endl;
    if (m_pCeleX5->getRotateType()%2 == 1)
    {
        m_pCeleX5->setRotateType(-1, -1);
        pButton->setStyleSheet("QPushButton {background: #002F6F; color: white; "
                               "border-style: outset; border-width: 2px; border-radius: 10px; border-color: #002F6F; "
                               "font: 20px Calibri; }"
                               "QPushButton:pressed {background: #992F6F;}");
    }
    else
    {
        m_pCeleX5->setRotateType(1, -1);
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
    else if ("Generate FPN (M)" == button->objectName())
    {
        m_pCeleX5->generateFPN("", 0);
    }
    else if ("Generate FPN (S)" == button->objectName())
    {
        m_pCeleX5->generateFPN("", 1);
    }
    else if ("Change FPN (M)" == button->objectName())
    {
        changeFPN(0);
    }
    else if ("Change FPN (S)" == button->objectName())
    {
        changeFPN(1);
    }
    else if ("Start Recording Bin" == button->objectName())
    {
        record((QPushButton*)button);
    }
    else if ("Playback" == button->objectName())
    {
        playback((QPushButton*)button);
    }
    else if ("Enter Loop Mode" == button->objectName())
    {
        //switchMode((QPushButton*)button, !m_pCeleX5->isLoopModeEnabled());
    }
    else if ("Configurations" == button->objectName())
    {
        //showCFG();
        showSettingsWidget();
    }
    else if ("Enable Auto ISP" == button->objectName())
    {
        if (CeleX5::Full_Picture_Mode == m_pCeleX5->getSensorFixedMode(0) ||
                CeleX5::Full_Picture_Mode == m_pCeleX5->getSensorFixedMode(1))
        {
            if ("Enable Auto ISP" == button->text())
            {
                button->setText("Disable Auto ISP");
                m_pCeleX5->setAutoISPEnabled(true, 0);
                m_pCeleX5->setAutoISPEnabled(true, 1);
                setButtonEnable((QPushButton*)button);
            }
            else
            {
                button->setText("Enable Auto ISP");
                m_pCeleX5->setAutoISPEnabled(false, 0);
                m_pCeleX5->setAutoISPEnabled(false, 1);
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
        //m_pCeleX5->beginSaveFullPic("");
    }
    else if ("Rotate_LR" == button->objectName())
    {
        setRotateLRType((QPushButton*)button);
    }
    else if ("Rotate_UD" == button->objectName())
    {
        setRotateUDType((QPushButton*)button);
    }
    else if ("ConvertBin2Video" == button->objectName())
    {
        convertBin2Video((QPushButton*)button);
    }
    else if ("ConvertBin2CSV" == button->objectName())
    {
        convertBin2CSV((QPushButton*)button);
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
        m_pCeleX5->setClockRate(value, 0);
        m_pCeleX5->setClockRate(value, 1);
    }
    else if ("Brightness" == slider->getBiasType())
    {
        m_pCeleX5->setBrightness(value, 0);
        m_pCeleX5->setBrightness(value, 1);
    }
    else if ("Threshold" == slider->getBiasType())
    {
        m_pCeleX5->setThreshold(value, 0);
        m_pCeleX5->setThreshold(value, 1);
    }
    else if ("ISO" == slider->getBiasType())
    {
        m_pCeleX5->setISOLevel(value, 0);
        m_pCeleX5->setISOLevel(value, 1);
    }
    else if ("Display FPS" == slider->getBiasType())
    {
        m_pSensorDataObserver->setDisplayFPS(value);
    }
    else if ("DataCount" == slider->getBiasType())
    {
        m_pCeleX5->setCurrentPackageNo(value);
        m_pReadBinTimer->start(READ_BIN_TIME);
        m_pUpdatePlayInfoTimer->start(1000);
    }
    else if ("Brightness 1" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 1, 0);
        m_pCeleX5->setISPBrightness(value, 1, 1);
    }
    else if ("Brightness 2" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 2, 0);
        m_pCeleX5->setISPBrightness(value, 2, 1);
    }
    else if ("Brightness 3" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 3, 0);
        m_pCeleX5->setISPBrightness(value, 3, 1);
    }
    else if ("Brightness 4" == slider->getBiasType())
    {
        m_pCeleX5->setISPBrightness(value, 4, 0);
        m_pCeleX5->setISPBrightness(value, 4, 1);
    }
    else if ("BRT Threshold 1" == slider->getBiasType())
    {
        m_pCeleX5->setISPThreshold(value, 1, 0);
        m_pCeleX5->setISPThreshold(value, 1, 1);
    }
    else if ("BRT Threshold 2" == slider->getBiasType())
    {
        m_pCeleX5->setISPThreshold(value, 2, 0);
        m_pCeleX5->setISPThreshold(value, 2, 1);
    }
    else if ("BRT Threshold 3" == slider->getBiasType())
    {
        m_pCeleX5->setISPThreshold(value, 3, 0);
        m_pCeleX5->setISPThreshold(value, 3, 1);
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
}

void CeleX5Widget::onReadBinTimer()
{
    //cout << __FUNCTION__ << endl;
    if (m_emDeviceType == CeleX5::CeleX5_MIPI)
    {
        //        if (m_pCeleX5->readBinFileData(0) && m_pCeleX5->readBinFileData(1))
        //        {
        //            m_pReadBinTimer->stop();
        //        }
        //        if (!m_pCeleX5->readBinFileData(0))
        //        {

        //        }
        if (m_pSensorDataObserver->getDisplayType() == ConvertBin2Video ||
            m_pSensorDataObserver->getDisplayType() == ConvertBin2CSV)
        {
            m_pCeleX5->readBinFileData(0);
        }
        else
        {
            m_pCeleX5->readBinFileData(0);
            m_pCeleX5->readBinFileData(1);
        }
        m_pReadBinTimer->start(READ_BIN_TIME);
    }
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
//    m_pReadBinTimer->stop();
//    setCurrentPackageNo(0);
//    m_pCeleX5->replay();
//    m_pReadBinTimer->start(READ_BIN_TIME);
//    m_pUpdatePlayInfoTimer->start(UPDATE_PLAY_INFO_TIME);
//    m_pCeleX5->alignBinFileData();

    m_pSensorDataObserver->setDisplayType(Realtime);
    m_pCeleX5->play();
#ifdef _WIN32
        Sleep(500);
#else
        usleep(500000);
#endif

    //g_qsBinFilePathList = QFileDialog::getOpenFileNames(this, "Open a bin file", QCoreApplication::applicationDirPath(), "Bin Files (*.bin)");
    //qDebug() << __FUNCTION__ << g_qsBinFilePathList << endl;
    bool bPlaybackSuccess = false;
    for (int i = 0; i < g_qsBinFilePathList.size(); i++)
    {
        QStringList nameList = g_qsBinFilePathList[0].split("/");
        g_qsBinFileName = nameList[nameList.size()-1];
        g_qsBinFileName = g_qsBinFileName.left(g_qsBinFileName.size()-6);
        //qDebug() << __FUNCTION__ << g_qsBinFileName << endl;
        QString filePath = g_qsBinFilePathList[i];
        if (filePath.isEmpty())
            return;
        if (m_pCeleX5->openBinFile(filePath.toStdString(), i))
        {
            bPlaybackSuccess = true;
        }
    }
    if (bPlaybackSuccess)
    {
        m_pSensorDataObserver->setDisplayType(Playback);
        m_pReadBinTimer->start(READ_BIN_TIME);
        m_pUpdatePlayInfoTimer->start(UPDATE_PLAY_INFO_TIME);

        m_pCeleX5->alignBinFileData();
    }
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
        g_lFrameCount_M = 0;
        g_lFrameCount_S = 0;
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
    QString mode = text.mid(8);
    setSensorFixedMode(0, mode);
}

void CeleX5Widget::onSensorModeChanged_M(QString mode)
{
    cout << __FUNCTION__ << ": " << mode.toStdString() << endl;
    setSensorFixedMode(0, mode.mid(12));
}

void CeleX5Widget::onSensorModeChanged_S(QString mode)
{
    cout << __FUNCTION__ << ": " << mode.toStdString() << endl;
    setSensorFixedMode(1, mode.mid(12));
}

void CeleX5Widget::onImageTypeChanged_M(QString type)
{
    cout << __FUNCTION__ << ": " << type.toStdString() << endl;
    m_pSensorDataObserver->setDisplayPicType(0, type);
}

void CeleX5Widget::onImageTypeChanged_S(QString type)
{
    cout << __FUNCTION__ << ": " << type.toStdString() << endl;
    m_pSensorDataObserver->setDisplayPicType(1, type);
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

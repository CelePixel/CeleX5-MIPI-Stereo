#ifndef CELEX5WIDGET_H
#define CELEX5WIDGET_H

#include "sliderwidget.h"
#include "cfgslider.h"
#include "celex5cfg.h"
#include "settingswidget.h"
#include "doubleslider.h"
#include "./include/celex5/celex5.h"
#include "./include/celex5/celex5datamanager.h"
#include "videostream.h"
#include <QTime>
#include <QMessageBox>
#include <QRadioButton>

#ifdef _WIN32
#include <windows.h>
#else
#include<unistd.h>
#endif

using namespace std;

#pragma execution_character_set("utf-8")

enum DisplayType {
    Realtime = 0,
    Playback = 1,
    ConvertBin2Video = 2,
    ConvertBin2CSV = 3
};

enum DisplayPicType {
    Full_Pic = 0,
    Event_Binary_Pic = 1,
    Event_Denoised_Binary_Pic = 2,
    Event_Count_Pic = 3,
    Event_Gray_Pic = 4,
    Event_Accumulated_Pic = 5,
    Event_superimposed_Pic = 6,
    Event_OpticalFlow_Pic = 7,
    Full_OpticalFlow_Pic = 8,
    Full_OpticalFlow_Speed_Pic = 9,
    Full_OpticalFlow_Direction_Pic = 10,
};

class QLabel;
class MainWindow;
class QComboBox;
class SensorDataObserver : public QWidget, public CeleX5DataManager
{
    Q_OBJECT
public:
    SensorDataObserver(CX5SensorDataServer* sensorData, QWidget *parent);
    ~SensorDataObserver();
    virtual void onFrameDataUpdated(CeleX5ProcessedData* pSensorData); //overrides Observer operation
    void setCeleX5(CeleX5* pCeleX5);
    void setLoopModeEnabled(bool enable);

    void setDisplayPicType(int sensor_index, QString type);

    void setDisplayFPS(int count);
    void setDisplayType(DisplayType type);
    DisplayType getDisplayType(){ return m_emDisplayType; }
    void setSaveBmp(bool save);
    bool isSavingBmp();

private:
    void updatePicBuffer(int sensor_index, CeleX5::CeleX5Mode mode, unsigned char* pBuffer, DisplayPicType picType);
    void updateImage(unsigned char* pBuffer, int loopNum, DisplayPicType picType);
    void savePics(int device_index);

protected:
    void paintEvent(QPaintEvent *event);

protected slots:
    void onUpdateImage();

private:
    QImage         m_imageMode1;
    QImage         m_imageMode2;
    QImage         m_imageMode3;
    QImage         m_imageMode4;

    CX5SensorDataServer*    m_pSensorData;
    CeleX5*                 m_pCeleX5;
    bool                    m_bLoopModeEnabled;
    DisplayPicType          m_emDisplayPicType[MAX_SENSOR_NUM];
    uint16_t                m_uiTemperature;
    uint16_t                m_uiRealFullFrameFPS;
    int                     m_iFPS;
    int                     m_iPlaybackFPS;
    uchar*                  m_pBuffer[4];
    QTimer*                 m_pUpdateTimer;
    DisplayType             m_emDisplayType;
    bool                    m_bSavePics;
    bool                    m_bMultiSensor;
};

class QGridLayout;
class QAbstractButton;
class QPushButton;
class QButtonGroup;
class CeleX5Widget : public QWidget
{
    Q_OBJECT
public:
    explicit CeleX5Widget(QWidget *parent = 0);
    ~CeleX5Widget();
    void closeEvent(QCloseEvent *event);

private:
    void playback(QPushButton* pButton);
    QComboBox *createModeComboBox(QString text, QRect rect, QWidget* parent, bool bLoop, int loopNum);
    void createButtons(QGridLayout* layout);
    void changeFPN(int device_index);
    void record(QPushButton* pButton);
    void switchMode(QPushButton* pButton, bool isLoopMode);
    //
    void showMoreParameters(int index);
    void showCFG();
    void showSettingsWidget();
    void showPlaybackBoard(bool show);
    //
    void setSliderMaxValue(QWidget* parent, QString objName, int value);
    int  getSliderMax(QWidget* parent, QString objName);
    //
    QPushButton* createButton(QString text, QRect rect, QWidget *parent);
    SliderWidget* createSlider(CeleX5::CfgInfo cfgInfo, int value, QRect rect, QWidget *parent, QWidget *widgetSlot);

    void setButtonEnable(QPushButton* pButton);
    void setButtonNormal(QPushButton* pButton);

    void setRotateLRType(QPushButton* pButton);
    void setRotateUDType(QPushButton* pButton);

    void convertBin2Video(QPushButton* pButton);
    void convertBin2CSV(QPushButton* pButton);
    void setCurrentPackageNo(int value);
    //
    void setSensorFixedMode(int device_index, QString mode);

signals:

protected slots:
    void onButtonClicked(QAbstractButton* button);
    void onRadioButtonClicked();
    //
    void onValueChanged(uint32_t value, CfgSlider* slider);
    //
    void onReadBinTimer();
    void onUpdatePlayInfo();

    //---------- playback ----------
    void onBtnPlayPauseReleased();
    void onBtnReplayRelease();
    void onBtnSavePicReleased();
    void onBtnSavePicExReleased();
    //
    void onSensorModeChanged(QString text);
    void onSensorModeChanged_M(QString mode);
    void onSensorModeChanged_S(QString mode);
    //
    void onImageTypeChanged_M(QString type);
    void onImageTypeChanged_S(QString type);
    //
    void onEventShowTypeChanged(int index);

private:
    CeleX5*             m_pCeleX5;
    QWidget*            m_pCFGWidget;
    QButtonGroup*       m_pButtonGroup;
    //
    QComboBox*          m_pCbBoxFixedMode_M;
    QComboBox*          m_pCbBoxFixedMode_S;
    //
    QComboBox*          m_pCbBoxImageType[MAX_SENSOR_NUM];
    //
    CfgSlider*          m_pFPSSlider;
    CfgSlider*          m_pFrameSlider;
    CfgSlider*          m_pEventCountSlider;
    CfgSlider*          m_pRowCycleSlider;   
    QButtonGroup*       m_pBtnGroup;
    //
    CeleX5Cfg*          m_pCeleX5Cfg;
    SettingsWidget*     m_pSettingsWidget;

    //---------- playback ----------
    QWidget*            m_pPlaybackBg;
    QPushButton*        m_pBtnPlayPause;
    QPushButton*        m_pBtnReplay;
    QPushButton*        m_pBtnSavePic;
    QPushButton*        m_pBtnSavePicEx;   
    QTimer*             m_pReadBinTimer;
    QTimer*             m_pUpdatePlayInfoTimer;
    bool                m_bPlaybackPaused;

    SensorDataObserver* m_pSensorDataObserver;
    map<string, vector<CeleX5::CfgInfo>> m_mapCfgDefault;

    CeleX5::DeviceType  m_emDeviceType;
    CeleX5::CeleX5Mode  m_emSensorFixedMode[2];
};

#endif // CELEX5WIDGET_H

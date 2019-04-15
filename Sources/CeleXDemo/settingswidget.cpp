#include "settingswidget.h"
#include "sliderwidget.h"
#include "cfgslider.h"
#include <QTabWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QRadioButton>
#include <iostream>
#include <QDebug>

using namespace std;

SettingsWidget::SettingsWidget(CeleX5 *pCeleX5, QWidget *parent)
    : QWidget(parent)
    , m_pCeleX5(pCeleX5)
{
    this->setWindowTitle("Configurations");
    this->setGeometry(10, 40, 1300, 520);
    this->setStyleSheet("background-color: lightgray; ");

    m_pTabWidget = new QTabWidget;
    m_pTabWidget->setStyleSheet("QTabWidget::pane {border-width: 1px; border-color: rgb(48, 104, 151);\
                                                   border-style: outset; background-color: rgb(132, 171, 208);} \
                                QTabBar::tab {min-height: 30px; font: 18px Calibri;} \
                                QTabBar::tab:selected{border-color: white; background: #002F6F; color: white;}");

    QStringList cfgList;
    cfgList << "Basic Controls" << "Device Info" << "Others";
    for (int i = 0; i < cfgList.size(); i++)
    {
        QWidget *widget = new QWidget();
        QString tapName = "       " + cfgList[i] + "       ";
        m_pTabWidget->addTab(widget, tapName);
        if (i == 0)
        {
            createTapWidget1(widget);
        }
        else if (i == 1)
        {
            createTapWidget2(widget);
        }
        else if (i == 2)
        {
            createTapWidget3(widget);;
        }
    }
    QHBoxLayout *layout = new QHBoxLayout();
    layout->addWidget(m_pTabWidget);

    this->setLayout(layout);
}

void SettingsWidget::setCurrentIndex(int index)
{
    for (int i = 0; i < m_csrTypeList.size(); i++)
        updateCfgParameters(i);
    m_pTabWidget->setCurrentIndex(index);
}

void SettingsWidget::updateCfgParameters(int index)
{
    QObjectList objList = m_pTabWidget->widget(index)->children();
    for (int i = 0; i < objList.size(); ++i)
    {
        SliderWidget* pSlider = (SliderWidget*)objList.at(i);
        QString csrType = m_csrTypeList.at(index);
        CeleX5::CfgInfo cfgInfo = m_pCeleX5->getCfgInfoByName(csrType.toStdString(), pSlider->getBiasType(), false);
        pSlider->updateValue(cfgInfo.value);
        cout << "CeleX5Cfg::updateCfgParameters: " << pSlider->getBiasType() << " = " << cfgInfo.value << endl;
    }
}

void SettingsWidget::createTapWidget1(QWidget *widget)
{
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
    int left = 30, groupWidth = 580;

    //----- Group 1 -----
    QGroupBox* speedGroup = new QGroupBox("Master Parameters: ", widget);
    speedGroup->setGeometry(left, 20, groupWidth, 420);
    speedGroup->setStyleSheet(style1 + style2);

    QStringList cfgList;
    cfgList << "Clock" << "Brightness" << "Threshold" << "ISO";
    int min[5]   = {20,  0,    50,  1};
    int max[5]   = {100, 1023, 511, 4};
    int value[5] = {100, 100,  171, 2};
    int step[5]  = {10,  1,    1,   1};
    int top[5]   = {50,  160,  240, 320};
    for (int i = 0; i < cfgList.size(); i++)
    {
        CfgSlider* pSlider = new CfgSlider(widget, min[i], max[i], step[i], value[i], this);
        pSlider->setGeometry(left, top[i], 550, 70);
        pSlider->setBiasType(QString(cfgList.at(i)).toStdString()+"_M");
        pSlider->setDisplayName(cfgList.at(i));
        pSlider->setObjectName(cfgList.at(i)+"_M");
    }

    //----- Group 2 -----
    left = 670;
    QGroupBox* slaveGroup = new QGroupBox("Slave Parameters: ", widget);
    slaveGroup->setGeometry(left, 20, groupWidth, 420);
    slaveGroup->setStyleSheet(style1 + style2);
    for (int i = 0; i < cfgList.size(); i++)
    {
        CfgSlider* pSlider = new CfgSlider(widget, min[i], max[i], step[i], value[i], this);
        pSlider->setGeometry(left, top[i], 550, 70);
        pSlider->setBiasType(QString(cfgList.at(i)).toStdString()+"_S");
        pSlider->setDisplayName(cfgList.at(i));
        pSlider->setObjectName(cfgList.at(i)+"_S");
    }
}

void SettingsWidget::createTapWidget2(QWidget *widget)
{
    int left = 65, top = 50, groupWidth = 500, groupHeight = 280;
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
    //----- Master Group -----
    QGroupBox* masterGroup = new QGroupBox("Master Sensor: ", widget);
    masterGroup->setGeometry(left-20, top-30, groupWidth+50, groupHeight);
    masterGroup->setStyleSheet(style1 + style2);
    //--- device serial number ---
    QLabel* pLabelSNum_1 = new QLabel(widget);
    pLabelSNum_1->setGeometry(left, top, groupWidth, 30);
    pLabelSNum_1->setText("Device Serial Number:   " + QString::fromStdString(m_pCeleX5->getSerialNumber(0)));
    pLabelSNum_1->setStyleSheet("QLabel { color: black; font: 18px Calibri; }");

    //--- fireware version ---
    QLabel* pLabelFVer_1 = new QLabel(widget);
    pLabelFVer_1->setGeometry(left, top+50, groupWidth, 30);
    pLabelFVer_1->setText("Firmware Version:   " + QString::fromStdString(m_pCeleX5->getFirmwareVersion(0)));
    pLabelFVer_1->setStyleSheet("QLabel { color: black; font: 18px Calibri; }");

    //--- fireware data ---
    QLabel* pLabelFDate_1 = new QLabel(widget);
    pLabelFDate_1->setGeometry(left, top+50*2, groupWidth, 30);
    pLabelFDate_1->setText("Firmware Date:   " + QString::fromStdString(m_pCeleX5->getFirmwareDate(0)));
    pLabelFDate_1->setStyleSheet("QLabel { color: black; font: 18px Calibri; }");

    //--- sensor type ---
    QLabel* pLabelType_1 = new QLabel(widget);
    pLabelType_1->setGeometry(left, top+50*3, groupWidth, 30);
    pLabelType_1->setText("Sensor Type:");
    pLabelType_1->setStyleSheet("QLabel { color: black; font: 18px Calibri; }");
    //
    QString combo_style_1 = "QComboBox {font: 18px Calibri; color: white; border: 2px solid darkgrey; "
                            "border-radius: 5px; background: #0033aa;}";
    QString combo_style_2 = "QComboBox:editable {background: #0033aa;}";
    QComboBox* pComboBoxType_1 = new QComboBox(widget);
    pComboBoxType_1->setGeometry(left+120, top+50*3, 120, 30);
    pComboBoxType_1->setStyleSheet(combo_style_1 + combo_style_2);
    pComboBoxType_1->insertItem(0, " Master");
    pComboBoxType_1->insertItem(1, " Slave");
    if (m_pCeleX5->getSensorAttribute(0) == CeleX5::Slave_Sensor)
        pComboBoxType_1->setCurrentIndex(1);
    connect(pComboBoxType_1, SIGNAL(currentIndexChanged(int)), this, SLOT(onSensorTypeChanged_1(int)));

    //--- API version ---
    QLabel* pLabelAPIVer_1 = new QLabel(widget);
    pLabelAPIVer_1->setGeometry(left, top+50*4, groupWidth, 30);
    pLabelAPIVer_1->setText("API Version:   1.0");
    pLabelAPIVer_1->setStyleSheet("QLabel { color: black; font: 18px Calibri; }");

    //----- Slave Group -----
    left = 700;
    QGroupBox* slaveGroup = new QGroupBox("Slave Sensor: ", widget);
    slaveGroup->setGeometry(left-20, top-30, groupWidth+50, groupHeight);
    slaveGroup->setStyleSheet(style1 + style2);

    //--- device serial number ---
    QLabel* pLabelSNum_2 = new QLabel(widget);
    pLabelSNum_2->setGeometry(left, top, groupWidth, 30);
    pLabelSNum_2->setText("Device Serial Number:   " + QString::fromStdString(m_pCeleX5->getSerialNumber(1)));
    pLabelSNum_2->setStyleSheet("QLabel { color: black; font: 18px Calibri; }");

    //--- fireware version ---
    QLabel* pLabelFVer_2 = new QLabel(widget);
    pLabelFVer_2->setGeometry(left, top+50, groupWidth, 30);
    pLabelFVer_2->setText("Firmware Version:   " + QString::fromStdString(m_pCeleX5->getFirmwareVersion(1)));
    pLabelFVer_2->setStyleSheet("QLabel { color: black; font: 18px Calibri; }");

    //--- fireware data ---
    QLabel* pLabelFDate_2 = new QLabel(widget);
    pLabelFDate_2->setGeometry(left, top+50*2, groupWidth, 30);
    pLabelFDate_2->setText("Firmware Date:   " + QString::fromStdString(m_pCeleX5->getFirmwareDate(1)));
    pLabelFDate_2->setStyleSheet("QLabel { color: black; font: 18px Calibri; }");

    //--- sensor type ---
    QLabel* pLabelType_2 = new QLabel(widget);
    pLabelType_2->setGeometry(left, top+50*3, groupWidth, 30);
    pLabelType_2->setText("Sensor Type:");
    pLabelType_2->setStyleSheet("QLabel { color: black; font: 18px Calibri; }");
    QComboBox* pComboBoxType_2 = new QComboBox(widget);
    pComboBoxType_2->setGeometry(left+120, top+50*3, 120, 30);
    pComboBoxType_2->setStyleSheet(combo_style_1 + combo_style_2);
    pComboBoxType_2->insertItem(0, " Master");
    pComboBoxType_2->insertItem(1, " Slave");
    if (m_pCeleX5->getSensorAttribute(1) == CeleX5::Slave_Sensor)
        pComboBoxType_2->setCurrentIndex(1);
    connect(pComboBoxType_2, SIGNAL(currentIndexChanged(int)), this, SLOT(onSensorTypeChanged_2(int)));

    //--- API version ---
    QLabel* pLabelAPIVer_2 = new QLabel(widget);
    pLabelAPIVer_2->setGeometry(left, top+50*4, groupWidth, 30);
    pLabelAPIVer_2->setText("API Version:   1.0");
    pLabelAPIVer_2->setStyleSheet("QLabel { color: black; font: 18px Calibri; }");
}

void SettingsWidget::createTapWidget3(QWidget *widget)
{
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

    QGroupBox* recordGroup = new QGroupBox("Data Record && Playback Parameters: ", widget);
    recordGroup->setGeometry(50, 20, 700, 150);
    recordGroup->setStyleSheet(style1 + style2);
    //
    QLabel* pLabel = new QLabel(tr("Whether to display the images while recording"), widget);
    pLabel->setGeometry(100, 50, 600, 50);
    pLabel->setStyleSheet("QLabel {background: transparent; font: 20px Calibri; }");

    QRadioButton *pRadioBtn = new QRadioButton(tr(" open"), widget);
    pRadioBtn->setGeometry(120, 90, 600, 50);
    pRadioBtn->setStyleSheet("QRadioButton {background: transparent; color: #990000; "
                             "font: 20px Calibri; }");
    pRadioBtn->setChecked(1);
    pRadioBtn->show();
    connect(pRadioBtn, SIGNAL(toggled(bool)), this, SLOT(onShowImagesSwitch(bool)));
    pRadioBtn->setObjectName("Display Switch");
    //
    QGroupBox* otherGroup = new QGroupBox("Other Parameters: ", widget);
    otherGroup->setGeometry(50, 220, 700, 150);
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
        CfgSlider* pSlider = new CfgSlider(widget, min2[i], max2[i], 1, value2[i], this);
        pSlider->setGeometry(90, 250+i*100, 600, 70);
        pSlider->setBiasType(QString(cfgObj2.at(i)).toStdString());
        pSlider->setDisplayName(cfgList2.at(i));
        pSlider->setObjectName(cfgObj2.at(i));
    }

//    widget->setFocus();
//    widget->raise();
//    widget->show();
//    if (widget->isMinimized())
    //        widget->showNormal();
}

void SettingsWidget::onValueChanged(uint32_t value, CfgSlider *slider)
{
    cout << "SettingsWidget::onValueChanged: " << slider->getBiasType() << ", " << value << endl;
    if ("Clock_M" == slider->getBiasType())
    {
        m_pCeleX5->setClockRate(value, 0);
    }
    else if ("Brightness_M" == slider->getBiasType())
    {
        m_pCeleX5->setBrightness(value, 0);
    }
    else if ("Threshold_M" == slider->getBiasType())
    {
        m_pCeleX5->setThreshold(value, 0);
    }
    else if ("ISO_M" == slider->getBiasType())
    {
        m_pCeleX5->setISOLevel(value, 0);
    }
    else if ("Clock_S" == slider->getBiasType())
    {
        m_pCeleX5->setClockRate(value, 1);
    }
    else if ("Brightness_S" == slider->getBiasType())
    {
        m_pCeleX5->setBrightness(value, 1);
    }
    else if ("Threshold_S" == slider->getBiasType())
    {
        m_pCeleX5->setThreshold(value, 1);
    }
    else if ("ISO_S" == slider->getBiasType())
    {
        m_pCeleX5->setISOLevel(value, 1);
    }
    else if ("Resolution" == slider->getBiasType())
    {
        m_pCeleX5->setRowDisabled(value);
    }
}

void SettingsWidget::onSensorTypeChanged_1(int index)
{
    m_pCeleX5->setSensorAttribute(CeleX5::SensorAttribute(index), 0);
}

void SettingsWidget::onSensorTypeChanged_2(int index)
{
    m_pCeleX5->setSensorAttribute(CeleX5::SensorAttribute(index), 1);
}

void SettingsWidget::onShowImagesSwitch(bool state)
{
    //cout << "CeleX5Widget::onShowImagesSwitch: state = " << state << endl;
    m_pCeleX5->setShowImagesEnabled(state, -1);
    QList<QRadioButton*> radio1 = this->findChildren<QRadioButton *>("Display Switch");
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

#ifndef SETTINGSWIDGET_H
#define SETTINGSWIDGET_H

#include <QWidget>
#include "./include/celex5/celex5.h"
#include "cfgslider.h"

class QTabWidget;
class TestWidget;
class SettingsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsWidget(CeleX5* pCeleX5, QWidget *parent = 0);
    void setCurrentIndex(int index);

private:
    void updateCfgParameters(int index);
    void createTapWidget1(QWidget* widget);
    void createTapWidget2(QWidget* widget);
    void createTapWidget3(QWidget* widget);

signals:
    void valueChanged(string cmdName, int value);

public slots:
    void onValueChanged(uint32_t value, CfgSlider* slider);
    void onSensorTypeChanged_1(int index);
    void onSensorTypeChanged_2(int index);
    void onShowImagesSwitch(bool state);

private:
    CeleX5*            m_pCeleX5;
    QTabWidget*        m_pTabWidget;
    QStringList        m_csrTypeList;
};

#endif // SETTINGSWIDGET_H

﻿#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QCloseEvent>

MainWindow::MainWindow(int argc, char *argv[], QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_pCX5Widget(NULL)
{
    ui->setupUi(this);
    this->move(10, 10);

    bool bCeleX5Device = true;

    if (bCeleX5Device)
    {
        this->setWindowTitle("CeleX5-Demo");
        if (!m_pCX5Widget)
        {
            m_pCX5Widget = new CeleX5Widget(this);
            m_pCX5Widget->setGeometry(0, 0, 1860, 1000);
        }
        m_pCX5Widget->show();
        setCentralWidget(m_pCX5Widget);
    }
    this->showMaximized();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    cout << "MainWindow::closeEvent" << endl;
    if (m_pCX5Widget)
        m_pCX5Widget->closeEvent(event);
}

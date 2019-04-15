/*
* Copyright (c) 2017-2018  CelePixel Technology Co. Ltd.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "../include/celex5/celex5.h"
#include "../driver/CeleDriver.h"
#include "../configproc/hhsequencemgr.h"
#include "../configproc/hhwireincommand.h"
#include "../base/xbase.h"
#include "../eventproc/dataprocessthread.h"
#include "../eventproc/datareaderthread.h"
#include "datarecorder.h"
#include <iostream>
#include <cstring>
#include <thread>
#ifdef _WIN32
#else
#include <unistd.h>
#endif

#ifdef _WIN32
static HANDLE s_hEventHandle = nullptr;
#endif

#define CSR_COL_GAIN          45

#define CSR_BIAS_ADVL_I_H     30
#define CSR_BIAS_ADVL_I_L     31
#define CSR_BIAS_ADVH_I_H     26
#define CSR_BIAS_ADVH_I_L     27

#define CSR_BIAS_ADVCL_I_H    38
#define CSR_BIAS_ADVCL_I_L    39
#define CSR_BIAS_ADVCH_I_H    34
#define CSR_BIAS_ADVCH_I_L    35

CeleX5::CeleX5()
	: m_uiClockRate{ 100, 100 }
	, m_uiLastClockRate{ 100, 100 }
	, m_iEventDataFormat{ 2, 2 }
	, m_pDataToRead(NULL)
	, m_uiPackageCount{ 0, 0 }
	, m_bFirstReadFinished(false)
	, m_pReadBuffer(NULL)
	, m_emDeviceType(CeleX5::Unknown_Devive)
	, m_pCeleDriver(NULL)
	, m_uiPackageCounter{ 0, 0 }
	, m_uiPackageTDiff{ 0, 0 }
	, m_uiPackageBeginT{ 0, 0 }
	, m_bAutoISPEnabled{ false, false }
	, m_uiBrightness{ 100, 100 }
	, m_uiThreshold{ 171, 171 }
	, m_arrayISPThreshold{ 60, 500, 2500 }
	, m_arrayBrightness{ 100, 110, 120, 130 }
	, m_uiAutoISPRefreshTime(80)
	, m_uiISOLevel{ 2, 2 }
	, m_uiOpticalFlowFrameTime(30)
{
	m_pSequenceMgr = new HHSequenceMgr;

	if (NULL == m_pCeleDriver)
	{
		m_pCeleDriver = new CeleDriver;
	}
	XBase base;
	std::string filePath = base.getApplicationDirPath();

	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		m_pDataRecorder[i] = new DataRecorder;

		//create data process thread
		m_pDataProcessThread[i] = new DataProcessThreadEx("CeleX5Thread");
		m_pDataProcessThread[i]->setCeleX(this, i);
	}
	//auto n = thread::hardware_concurrency();//cpu core count = 8
}

CeleX5::~CeleX5()
{
	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		if (m_pDataProcessThread[i])
		{
			if (m_pDataProcessThread[i]->isRunning())
				m_pDataProcessThread[i]->terminate();
			delete m_pDataProcessThread[i];
			m_pDataProcessThread[i] = NULL;
		}
	}
	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		if (m_pCeleDriver)
		{
			m_pCeleDriver->clearData(0);
			m_pCeleDriver->Close(0);
			delete m_pCeleDriver;
			m_pCeleDriver = NULL;
		}
	}
	//
	if (m_pSequenceMgr) delete m_pSequenceMgr;
	//
	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		if (m_pDataRecorder[i])
		{
			delete m_pDataRecorder[i];
			m_pDataRecorder[i] = NULL;
		}
	}
	if (m_pReadBuffer) {
		delete[] m_pReadBuffer;
		m_pReadBuffer = NULL;
	}
}

bool CeleX5::openSensor(DeviceType type)
{
	//cout << "-------- device count = " << m_pCeleDriver[0]->getDeviceCount();
	m_emDeviceType = type;
	m_pDataProcessThread[0]->setDeviceType(type);
	m_pDataProcessThread[1]->setDeviceType(type);
	if (CeleX5::CeleX5_OpalKelly == type)
	{
		//To Do
	}
	else if (CeleX5::CeleX5_MIPI == type)
	{
		m_pSequenceMgr->parseCeleX5Cfg(FILE_CELEX5_CFG_MIPI);
		m_mapCfgModified = m_mapCfgDefaults = getCeleX5Cfg();
		
		for (int i = 0; i < m_pCeleDriver->getDeviceCount(); i++) //just for two sensors
		{
			if (!m_pCeleDriver->openUSB(i))
				return false;
			cout << "------------- Serial Number: " << getSerialNumber(i) << endl;
			cout << "------------- Firmware Version: " << getFirmwareVersion(i) << endl;
			cout << "------------- Firmware Date: " << getFirmwareDate(i) << endl;

			m_pDataProcessThread[i]->getDataProcessor5()->setISOLevel(m_uiISOLevel[i]);

			if (!configureSettings(type, i))
				return false;
			clearData(i);
			m_pDataProcessThread[i]->start();
		}
		//cout << "------------- Attribute 0: " << m_pCeleDriver->getAttribute(0) << endl;
		//cout << "------------- Attribute 1: " << m_pCeleDriver->getAttribute(1) << endl;
		if (m_pCeleDriver->getAttribute(0) == "S" && m_pCeleDriver->getAttribute(1) == "M")
		{
			m_pCeleDriver->exchangeDevices();
		}
		//cout << "------------- Attribute 0: " << m_pCeleDriver->getAttribute(0) << endl;
		//cout << "------------- Attribute 1: " << m_pCeleDriver->getAttribute(1) << endl;
	}
	return true;
}

bool CeleX5::isSensorReady(int device_index)
{
	return true;
}

void CeleX5::getMIPIData(vector<uint8_t> &buffer, std::time_t& time_stamp_end, vector<IMURawData>& imu_data, int device_index)
{
	if (CeleX5::CeleX5_MIPI != m_emDeviceType)
	{
		return;
	}
	vector<IMU_Raw_Data> imu_raw_data;
	if (m_pCeleDriver->getSensorData(buffer, time_stamp_end, imu_raw_data, device_index))
	{
		imu_data = vector<IMURawData>(imu_raw_data.size());
		for (int i = 0; i < imu_raw_data.size(); i++)
		{
			//cout << "--------------"<<imu_raw_data[i].time_stamp << endl;
			memcpy(imu_data[i].imu_data, imu_raw_data[i].imu_data, sizeof(imu_raw_data[i].imu_data));
			imu_data[i].time_stamp = imu_raw_data[i].time_stamp;
			//imu_data[i] = ((struct IMURawData*)&imu_raw_data[i]);
		}
		//parseIMUData(imu_data);
		//record sensor data
		if (m_pDataRecorder[device_index]->isRecording())
		{
			m_pDataRecorder[device_index]->writeData(buffer, time_stamp_end, imu_data);
		}

		if (getSensorFixedMode(device_index) > 2)
		{
			m_uiPackageCounter[device_index]++;
#ifdef _WIN32
			uint32_t t2 = GetTickCount();
#else
			uint32_t t2 = clock() / 1000;
#endif
			m_uiPackageTDiff[device_index] += (t2 - m_uiPackageBeginT[device_index]);
			m_uiPackageBeginT[device_index] = t2;
			if (m_uiPackageTDiff[device_index] > 1000)
			{
				//cout << "--- package count = " << counter << endl;
				m_pDataProcessThread[device_index]->getDataProcessor5()->getProcessedData()->setFullFrameFPS(m_uiPackageCountPS[device_index]);
				m_uiPackageTDiff[device_index] = 0;
				m_uiPackageCountPS[device_index] = m_uiPackageCounter[device_index];
				m_uiPackageCounter[device_index] = 0;
			}
		}
	}
}

CX5SensorDataServer* CeleX5::getSensorDataServer(int device_index)
{
	return m_pDataProcessThread[device_index]->getDataProcessor5()->getSensorDataServer();
}

CeleX5::DeviceType CeleX5::getDeviceType()
{
	return m_emDeviceType;
}

std::string CeleX5::getSerialNumber(int device_index)
{
	//cout << "------------- Serial Number: " << m_pCeleDriver->getSerialNumber() << endl;
	return m_pCeleDriver->getSerialNumber(device_index);
}

std::string CeleX5::getFirmwareVersion(int device_index)
{
	//cout << "------------- Firmware Version: " << m_pCeleDriver->getFirmwareVersion() << endl;
	return m_pCeleDriver->getFirmwareVersion(device_index);
}

std::string CeleX5::getFirmwareDate(int device_index)
{
	//cout << "------------- Firmware Date: " << m_pCeleDriver->getFirmwareDate() << endl;
	return m_pCeleDriver->getFirmwareDate(device_index);
}

bool CeleX5::setFpnFile(const std::string& fpnFile, int device_index)
{
	return m_pDataProcessThread[device_index]->getDataProcessor5()->setFpnFile(fpnFile);
}

void CeleX5::generateFPN(std::string fpnFile, int device_index)
{
	m_pDataProcessThread[device_index]->getDataProcessor5()->generateFPN(fpnFile, device_index);
}

// Set the Sensor operation mode in fixed mode
// address = 53, width = [2:0]
void CeleX5::setSensorFixedMode(CeleX5Mode mode, int device_index)
{
	//-------------- for change clock --------------
	if (CeleX5::Event_Intensity_Mode == mode || CeleX5::Event_Optical_Flow_Mode == mode)
	{
		if (m_uiClockRate[device_index] > 70)
		{
			m_uiLastClockRate[device_index] = m_uiClockRate[device_index];
			setClockRate(70, device_index);
		}
	}
	else
	{
		if (m_uiLastClockRate[device_index] != m_uiClockRate[device_index])
			setClockRate(m_uiLastClockRate[device_index], device_index);
	}
	//-------------- for change clock --------------	
	m_pDataProcessThread[device_index]->clearData();

	if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		m_pCeleDriver->clearData(device_index);
	}

	//Disable ALS read and write, must be the first operation
	setALSEnabled(false, device_index);

	//Enter CFG Mode
	wireIn(93, 0, 0xFF, device_index);
	wireIn(90, 1, 0xFF, device_index);

	//Write Fixed Sensor Mode
	wireIn(53, static_cast<uint32_t>(mode), 0xFF, device_index);

	//Disable brightness adjustment (auto isp), always load sensor core parameters from profile0
	wireIn(221, 0, 0xFF, device_index); //AUTOISP_BRT_EN
	wireIn(223, 0, 0xFF, device_index); //AUTOISP_TRIGGER
	wireIn(220, 0, 0xFF, device_index); //AUTOISP_PROFILE_ADDR, Write core parameters to profile0
	writeRegister(233, -1, 232, 1500, device_index); //AUTOISP_BRT_VALUE, Set initial brightness value 1500
	writeRegister(22, -1, 23, m_uiBrightness[device_index], device_index); //BIAS_BRT_I, Override the brightness value in profile0, avoid conflict with AUTOISP profile0

	if (CeleX5::Event_Intensity_Mode == mode || CeleX5::Event_Optical_Flow_Mode == mode)
	{
		m_iEventDataFormat[device_index] = 1;
		wireIn(73, m_iEventDataFormat[device_index], 0xFF, device_index); //EVENT_PACKET_SELECT
		m_pDataProcessThread[device_index]->getDataProcessor5()->setMIPIDataFormat(m_iEventDataFormat[device_index]);
		/*
		(1) CSR_114 / CSR_115 = 96
		(2) CSR_74 = 254
		(3) CSR_76 / CSR_77 = 1280

		(4) CSR_79 / CSR_80 = 400
		(5) CSR_82 / CSR_83 = 800
		(6) CSR_84 / CSR_85 = 462
		(7) CSR_86 / CSR_87 = 1200
		*/
		writeRegister(79, -1, 80, 200, device_index); //has bug
		writeRegister(82, -1, 83, 800, device_index);
		writeRegister(84, -1, 85, 462, device_index);
		writeRegister(86, -1, 87, 1200, device_index);
	}
	else
	{
		m_iEventDataFormat[device_index] = 2;
		wireIn(73, m_iEventDataFormat[device_index], 0xFF, device_index); //EVENT_PACKET_SELECT
		m_pDataProcessThread[device_index]->getDataProcessor5()->setMIPIDataFormat(m_iEventDataFormat[device_index]);
		/*
		(1) CSR_114 / CSR_115 = 96
		(2) CSR_74 = 254
		(3) CSR_76 / CSR_77 = 1280

		(4) CSR_79 / CSR_80 = 200
		(5) CSR_82 / CSR_83 = 720
		(6) CSR_84 / CSR_85 = 680
		(7) CSR_86 / CSR_87 = 1300
		*/
		writeRegister(79, -1, 80, 200, device_index);
		writeRegister(82, -1, 83, 720, device_index);
		writeRegister(84, -1, 85, 680, device_index);
		writeRegister(86, -1, 87, 1300, device_index);
	}
	//Enter Start Mode
	wireIn(90, 0, 0xFF, device_index);
	wireIn(93, 1, 0xFF, device_index);

	m_pDataProcessThread[device_index]->getDataProcessor5()->setSensorFixedMode(mode);
}

CeleX5::CeleX5Mode CeleX5::getSensorFixedMode(int device_index)
{
	return m_pDataProcessThread[device_index]->getDataProcessor5()->getSensorFixedMode();
}

// related to fps (main clock frequency), hardware parameter
uint32_t CeleX5::getFullPicFrameTime(int device_index)
{
	return 1000 / m_uiClockRate[device_index];
}

//software parameter
void CeleX5::setEventFrameTime(uint32_t value, int device_index)
{
	if (-1 == device_index)
	{
		for (int i = 0; i < MAX_SENSOR_NUM; i++)
		{
			m_pDataProcessThread[i]->getDataProcessor5()->setEventFrameTime(value, m_uiClockRate[i]);
		}
	}
	else
	{
		m_pDataProcessThread[device_index]->getDataProcessor5()->setEventFrameTime(value, m_uiClockRate[device_index]);
	}
}
uint32_t CeleX5::getEventFrameTime(int device_index)
{
	return m_pDataProcessThread[device_index]->getDataProcessor5()->getEventFrameTime();
}

//hardware parameter
void CeleX5::setOpticalFlowFrameTime(uint32_t value)
{
	if (value <= 10 || value >= 180)
	{
		cout << __FUNCTION__ << ": value is out of range!" << endl;
		return;
	}
	m_uiOpticalFlowFrameTime = value;
	//
	enterCFGMode();
	wireIn(169, (value - 10) * 3 / 2, 0xFF);
	enterStartMode();
}
uint32_t CeleX5::getOpticalFlowFrameTime()
{
	return m_uiOpticalFlowFrameTime;
}

void CeleX5::setEventShowMethod(EventShowType type, int value, int device_index)
{
	if (-1 == device_index)
	{
		for (int i = 0; i < MAX_SENSOR_NUM; i++)
			m_pDataProcessThread[i]->getDataProcessor5()->setEventShowMethod(type, value);
		if (type == EventShowByTime)
			setEventFrameTime(value, -1);
	}
	else
	{
		m_pDataProcessThread[device_index]->getDataProcessor5()->setEventShowMethod(type, value);
		if (type == EventShowByTime)
			setEventFrameTime(value, device_index);
	}
}
EventShowType CeleX5::getEventShowMethod(int device_index)
{
	return m_pDataProcessThread[device_index]->getDataProcessor5()->getEventShowMethod();
}

void CeleX5::setRotateType(int type, int device_index)
{
	if (-1 == device_index)
	{
		for (int i = 0; i < MAX_SENSOR_NUM; i++)
			m_pDataProcessThread[i]->getDataProcessor5()->setRotateType(type);
	}
	else
	{
		m_pDataProcessThread[device_index]->getDataProcessor5()->setRotateType(type);
	}
}
int CeleX5::getRotateType(int device_index)
{
	return m_pDataProcessThread[device_index]->getDataProcessor5()->getRotateType();
}

void CeleX5::setEventCountStepSize(uint32_t size, int device_index)
{
	m_pDataProcessThread[device_index]->getDataProcessor5()->setEventCountStep(size);
}
uint32_t CeleX5::getEventCountStepSize(int device_index)
{
	return m_pDataProcessThread[device_index]->getDataProcessor5()->getEventCountStep();
}

uint32_t CeleX5::getFullFrameFPS()
{
	return m_uiPackageCountPS[0];
}

void CeleX5::setRowDisabled(uint8_t rowMask, int device_index)
{
	if (-1 == device_index)
	{
		for (int i = 0; i < MAX_SENSOR_NUM; i++)
		{
			enterCFGMode(i);
			wireIn(44, rowMask, 0xFF, i);
			enterStartMode(i);
		}
	}
	else
	{
		enterCFGMode(device_index);
		wireIn(44, rowMask, 0xFF, device_index);
		enterStartMode(device_index);
	}
}

void CeleX5::setShowImagesEnabled(bool enable, int device_index)
{
	
	if (-1 == device_index)
	{
		for (int i = 0; i < MAX_SENSOR_NUM; i++)
			m_pDataProcessThread[i]->setShowImagesEnabled(enable);
	}
	else
	{
		m_pDataProcessThread[device_index]->setShowImagesEnabled(enable);
	}
}

// BIAS_EVT_VL : 341 address(2/3)
// BIAS_EVT_DC : 512 address(4/5)
// BIAS_EVT_VH : 683 address(6/7)
void CeleX5::setThreshold(uint32_t value, int device_index)
{
	m_uiThreshold[device_index] = value;

	enterCFGMode(device_index);

	int EVT_VL = 512 - value;
	if (EVT_VL < 0)
		EVT_VL = 0;
	writeRegister(2, -1, 3, EVT_VL, device_index);

	int EVT_VH = 512 + value;
	if (EVT_VH > 1023)
		EVT_VH = 1023;
	writeRegister(6, -1, 7, EVT_VH, device_index);

	enterStartMode(device_index);
}

uint32_t CeleX5::getThreshold(int device_index)
{
	return m_uiThreshold[device_index];
}

// Set brightness
// <BIAS_BRT_I>
// high byte address = 22, width = [1:0]
// low byte address = 23, width = [7:0]
void CeleX5::setBrightness(uint32_t value, int device_index)
{
	m_uiBrightness[device_index] = value;

	enterCFGMode(device_index);
	writeRegister(22, -1, 23, value, device_index);
	enterStartMode(device_index);
}

uint32_t CeleX5::getBrightness(int device_index)
{
	return m_uiBrightness[device_index];
}

uint32_t CeleX5::getClockRate(int device_index)
{
	return m_uiClockRate[device_index];
}

void CeleX5::setClockRate(uint32_t value, int device_index)
{
	if (value > 100 || value < 20)
		return;
	m_uiClockRate[device_index] = value;
	if (CeleX5::CeleX5_OpalKelly == m_emDeviceType)
	{
		enterCFGMode(device_index);

		// Disable PLL
		wireIn(150, 0, 0xFF, device_index);
		// Write PLL Clock Parameter
		wireIn(159, value * 3 / 5, 0xFF, device_index);
		// Enable PLL
		wireIn(150, 1, 0xFF, device_index);

		enterStartMode(device_index);
	}
	else if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		enterCFGMode(device_index);

		// Disable PLL
		wireIn(150, 0, 0xFF, device_index);
		int clock[15] = { 20,  30,  40,  50,  60,  70,  80,  90, 100, 110, 120, 130, 140, 150, 160 };

		int PLL_DIV_N[15] = { 12,  18,  12,  15,  18,  21,  12,  18,  15,  22,  18,  26,  21,  30, 24 };
		int PLL_DIV_L[15] = { 2,   3,   2,   2,   2,   2,   2,   3,   2,   3,   2,   3,   2,   3,  2 };
		int PLL_FOUT_DIV1[15] = { 3,   2,   1,   1,   1,   1,   0,   0,   0,   0,   0,   0,   0,   0,  0 };
		int PLL_FOUT_DIV2[15] = { 3,   2,   3,   3,   3,   3,   3,   2,   3,   3,   3,   3,   3,   3,  3 };

		int MIPI_PLL_DIV_I[15] = { 3,   2,   3,   3,   2,   2,   3,   2,   3,   2,   2,   2,   2,   2,  1 };
		int MIPI_PLL_DIV_N[15] = { 120, 120, 120, 96,  120, 102, 120, 120, 96,  130, 120, 110, 102, 96, 120 };

		int index = value / 10 - 2;

		cout << "CeleX5::setClockRate: " << clock[index] << " MHz" << endl;

		// Write PLL Clock Parameter
		writeRegister(159, -1, -1, PLL_DIV_N[index], device_index);
		writeRegister(160, -1, -1, PLL_DIV_L[index], device_index);
		writeRegister(151, -1, -1, PLL_FOUT_DIV1[index], device_index);
		writeRegister(152, -1, -1, PLL_FOUT_DIV2[index], device_index);

		// Enable PLL
		wireIn(150, 1, 0xFF, device_index);

		disableMIPI(device_index);
		writeRegister(113, -1, -1, MIPI_PLL_DIV_I[index], device_index);
		writeRegister(115, -1, 114, MIPI_PLL_DIV_N[index], device_index);
		enableMIPI(device_index);

		enterStartMode(device_index);

		uint32_t frame_time = m_pDataProcessThread[device_index]->getDataProcessor5()->getEventFrameTime();
		m_pDataProcessThread[device_index]->getDataProcessor5()->setEventFrameTime(frame_time, m_uiClockRate[device_index]);
	}
}

void CeleX5::setISOLevel(uint32_t value, int device_index)
{
	int index = value - 1;
	if (index < 0)
		index = 0;
	if (index > 5)
		index = 5;

	m_uiISOLevel[device_index] = index + 1;
	m_pDataProcessThread[device_index]->getDataProcessor5()->setISOLevel(m_uiISOLevel[device_index]);

	int col_gain[6] = { 2, 2, 2, 2, 1, 1 };
	int bias_advl_i[6] = { 470, 410, 350, 290, 410, 380 };
	int bias_advh_i[6] = { 710, 770, 830, 890, 770, 800 };
	int bias_advcl_i[6] = { 560, 545, 530, 515, 545, 540 };
	int bias_advch_i[6] = { 620, 635, 650, 665, 635, 640 };
	int bias_vcm_i[6] = { 590, 590, 590, 590, 590, 590 };

	/*int col_gain[4] = { 1, 1, 1, 1 };
	int bias_advl_i[4] = { 428, 384, 336, 284 };
	int bias_advh_i[4] = { 636, 680, 728, 780 };
	int bias_advcl_i[4] = { 506, 495, 483, 470 };
	int bias_advch_i[4] = { 558, 569, 581, 594 };
	int bias_vcm_i[4] = { 532, 532, 532, 532 };*/

	enterCFGMode(device_index);

	writeRegister(CSR_COL_GAIN, -1, -1, col_gain[index], device_index);
	writeRegister(CSR_BIAS_ADVL_I_H, -1, CSR_BIAS_ADVL_I_L, bias_advl_i[index], device_index);
	writeRegister(CSR_BIAS_ADVH_I_H, -1, CSR_BIAS_ADVH_I_L, bias_advh_i[index], device_index);
	writeRegister(CSR_BIAS_ADVCL_I_H, -1, CSR_BIAS_ADVCL_I_L, bias_advcl_i[index], device_index);
	writeRegister(CSR_BIAS_ADVCH_I_H, -1, CSR_BIAS_ADVCH_I_L, bias_advch_i[index], device_index);

	writeRegister(42, -1, 43, bias_vcm_i[index], device_index);

	enterStartMode(device_index);
}

uint32_t CeleX5::getISOLevel(int device_index)
{
	return m_uiISOLevel[device_index];
}

void CeleX5::setSensorAttribute(SensorAttribute attribute, int device_index)
{
	if (attribute == CeleX5::Master_Sensor)
		m_pCeleDriver->writeAttribute("M", device_index);
	else if (attribute == CeleX5::Slave_Sensor)
		m_pCeleDriver->writeAttribute("S", device_index);
}

CeleX5::SensorAttribute CeleX5::getSensorAttribute(int device_index)
{
	if (m_pCeleDriver->getAttribute(device_index) == "S")
		return CeleX5::Slave_Sensor;
	else
		return CeleX5::Master_Sensor;
}

//0: format 0; 1: format 1; 2: format 2
void CeleX5::setEventDataFormat(int format, int device_index)
{
	m_iEventDataFormat[device_index] = format;
	wireIn(73, m_iEventDataFormat[device_index], 0xFF, device_index); //EVENT_PACKET_SELECT
	m_pDataProcessThread[device_index]->getDataProcessor5()->setMIPIDataFormat(m_iEventDataFormat[device_index]);
}

int CeleX5::getEventDataFormat(int device_index)
{
	return m_iEventDataFormat[device_index];
}

void CeleX5::reset(int device_index)
{
	if (m_emDeviceType == CeleX5::CeleX5_OpalKelly)
	{
		//To Do
	}
	else if (m_emDeviceType == CeleX5::CeleX5_MIPI)
	{
		enterCFGMode(device_index);
		m_pCeleDriver->clearData();
		enterStartMode(device_index);
	}
}
void CeleX5::pauseSensor()
{
	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		enterCFGMode(i);
	}
}

void CeleX5::restartSensor()
{
	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		enterStartMode(i);
	}
}

// FLICKER_DETECT_EN: CSR_183
// Flicker detection enable select: 1:enable / 0:disable
void CeleX5::setAntiFlashlightEnabled(bool enabled, int device_index)
{
	enterCFGMode(device_index);
	if (enabled)
		writeRegister(183, -1, -1, 1, device_index);
	else
		writeRegister(183, -1, -1, 0, device_index);
	enterStartMode(device_index);
}

void CeleX5::setAutoISPEnabled(bool enable, int device_index)
{
	m_bAutoISPEnabled[device_index] = enable;
	if (enable)
	{
		enterCFGMode(device_index);

		wireIn(221, 1, 0xFF, device_index); //AUTOISP_BRT_EN, enable auto ISP
		wireIn(223, 0, 0xFF, device_index); //AUTOISP_TRIGGER

		wireIn(220, 0, 0xFF, device_index); //AUTOISP_PROFILE_ADDR, Write core parameters to profile0
		writeRegister(233, -1, 232, 1500, device_index); //AUTOISP_BRT_VALUE, Set initial brightness value 1500
		writeRegister(22, -1, 23, 80, device_index); //BIAS_BRT_I, Override the brightness value in profile0, avoid conflict with AUTOISP profile0		

		enterStartMode(device_index);

		setALSEnabled(true, device_index);
	}
	else
	{
		setALSEnabled(false, device_index); //Disable ALS read and write

		enterCFGMode(device_index);

		//Disable brightness adjustment (auto isp), always load sensor core parameters from profile0
		wireIn(221, 0, 0xFF, device_index); //AUTOISP_BRT_EN, disable auto ISP
		wireIn(223, 0, 0xFF, device_index); //AUTOISP_TRIGGER

		wireIn(220, 0, 0xFF, device_index); //AUTOISP_PROFILE_ADDR, Write core parameters to profile0
		writeRegister(233, -1, 232, 1500, device_index); //AUTOISP_BRT_VALUE, Set initial brightness value 1500
		writeRegister(22, -1, 23, 140, device_index); //BIAS_BRT_I, Override the brightness value in profile0, avoid conflict with AUTOISP profile0	
		enterStartMode(device_index);
	}
}

bool CeleX5::isAutoISPEnabled(int device_index)
{
	return m_bAutoISPEnabled[device_index];
}

void CeleX5::setALSEnabled(bool enable, int device_index)
{
	if (enable)
		m_pCeleDriver->i2c_set(254, 0, device_index);
	else
		m_pCeleDriver->i2c_set(254, 2, device_index);
}

void CeleX5::setISPThreshold(uint32_t value, int num, int device_index)
{
	m_arrayISPThreshold[num - 1] = value;
	if (num == 1)
		writeRegister(235, -1, 234, m_arrayISPThreshold[0], device_index); //AUTOISP_BRT_THRES1
	else if (num == 2)
		writeRegister(237, -1, 236, m_arrayISPThreshold[1], device_index); //AUTOISP_BRT_THRES2
	else if (num == 3)
		writeRegister(239, -1, 238, m_arrayISPThreshold[2], device_index); //AUTOISP_BRT_THRES3
}

void CeleX5::setISPBrightness(uint32_t value, int num, int device_index)
{
	m_arrayBrightness[num - 1] = value;
	wireIn(220, num - 1, 0xFF, device_index); //AUTOISP_PROFILE_ADDR
	writeRegister(22, -1, 23, m_arrayBrightness[num - 1], device_index);
}

void CeleX5::getFullPicBuffer(unsigned char* buffer, int device_index)
{
	m_pDataProcessThread[device_index]->getDataProcessor5()->getFullPicBuffer(buffer);
}
cv::Mat CeleX5::getFullPicMat(int device_index)
{
	/*CeleX5ProcessedData* pSensorData = m_pDataProcessThread[device_index]->getDataProcessor5()->getProcessedData();
	if (pSensorData)
	{
		return cv::Mat(cv::Size(CELEX5_COL, CELEX5_ROW), CV_8UC1, pSensorData->getFullPicBuffer());
	}
	return cv::Mat();*/

	cv::Mat matPic(CELEX5_ROW, CELEX5_COL, CV_8UC1);
	m_pDataProcessThread[device_index]->getDataProcessor5()->getFullPicBuffer(matPic.data);
	return matPic;
}

void CeleX5::getEventPicBuffer(unsigned char* buffer, emEventPicType type, int device_index)
{
	m_pDataProcessThread[device_index]->getDataProcessor5()->getEventPicBuffer(buffer, type);
}
cv::Mat CeleX5::getEventPicMat(emEventPicType type, int device_index)
{
	/*CeleX5ProcessedData* pSensorData = m_pDataProcessThread[device_index]->getDataProcessor5()->getProcessedData();
	if (pSensorData)
	{
		return cv::Mat(cv::Size(CELEX5_COL, CELEX5_ROW), CV_8UC1, pSensorData->getEventPicBuffer(type));
	}
	return cv::Mat();*/

	cv::Mat matPic(CELEX5_ROW, CELEX5_COL, CV_8UC1);
	m_pDataProcessThread[device_index]->getDataProcessor5()->getEventPicBuffer(matPic.data, type);
	return matPic;
}

void CeleX5::getOpticalFlowPicBuffer(unsigned char* buffer, emFullPicType type, int device_index)
{
	m_pDataProcessThread[device_index]->getDataProcessor5()->getOpticalFlowPicBuffer(buffer, type);
}
cv::Mat CeleX5::getOpticalFlowPicMat(emFullPicType type, int device_index)
{
	/*CeleX5ProcessedData* pSensorData = m_pDataProcessThread[device_index]->getDataProcessor5()->getProcessedData();
	if (pSensorData)
	{
		return cv::Mat(cv::Size(CELEX5_COL, CELEX5_ROW), CV_8UC1, pSensorData->getOpticalFlowPicBuffer(type));
	}
	return cv::Mat();*/

	cv::Mat matPic(CELEX5_ROW, CELEX5_COL, CV_8UC1);
	m_pDataProcessThread[device_index]->getDataProcessor5()->getOpticalFlowPicBuffer(matPic.data, type);
	return matPic;
}

bool CeleX5::getEventDataVector(std::vector<EventData> &vector, int device_index)
{
	return m_pDataProcessThread[device_index]->getDataProcessor5()->getEventDataVector(vector);
}
bool CeleX5::getEventDataVector(std::vector<EventData> &vector, uint64_t& frameNo, int device_index)
{
	return m_pDataProcessThread[device_index]->getDataProcessor5()->getEventDataVector(vector, frameNo);
}

int CeleX5::getIMUData(std::vector<IMUData>& data, int device_index)
{
	return m_pDataProcessThread[device_index]->getDataProcessor5()->getIMUData(data);
}

void CeleX5::startRecording(std::string filePath, int device_index)
{
	m_strFirmwareVer[device_index] = getFirmwareVersion(device_index);
	m_pDataProcessThread[device_index]->setRecordState(true);
	m_pDataRecorder[device_index]->startRecording(filePath);
}

void CeleX5::stopRecording(int device_index)
{
	if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		if (-1 == device_index)
		{
			BinFileAttributes header;
			for (int i = 0; i < MAX_SENSOR_NUM; i++)
			{
				header.data_type = 2;
				header.loopA_mode = m_pDataProcessThread[i]->getDataProcessor5()->getSensorFixedMode();
				header.loopB_mode = 255;
				header.loopC_mode = 255;

				header.event_data_format = m_iEventDataFormat[i];
				m_pDataRecorder[i]->stopRecording(&header);
				m_pDataProcessThread[i]->setRecordState(false);
			}
		}
		else
		{
			BinFileAttributes header;
			header.data_type = 2;
			header.loopA_mode = m_pDataProcessThread[device_index]->getDataProcessor5()->getSensorFixedMode();
			header.loopB_mode = 255;
			header.loopC_mode = 255;

			header.event_data_format = m_iEventDataFormat[device_index];
			m_pDataRecorder[device_index]->stopRecording(&header);
			m_pDataProcessThread[device_index]->setRecordState(false);
		}
	}
}

map<string, vector<CeleX5::CfgInfo>> CeleX5::getCeleX5Cfg()
{
	map<string, vector<HHCommandBase*>> mapCfg = m_pSequenceMgr->getCeleX5Cfg();
	//
	map<string, vector<CeleX5::CfgInfo>> mapCfg1;
	for (auto itr = mapCfg.begin(); itr != mapCfg.end(); itr++)
	{
		//cout << "CelexSensorDLL::getCeleX5Cfg: " << itr->first << endl;
		vector<HHCommandBase*> pCmdList = itr->second;
		vector<CeleX5::CfgInfo> vecCfg;
		for (auto itr1 = pCmdList.begin(); itr1 != pCmdList.end(); itr1++)
		{
			WireinCommandEx* pCmd = (WireinCommandEx*)(*itr1);
			//cout << "----- Register Name: " << pCmd->name() << endl;
			CeleX5::CfgInfo cfgInfo;
			cfgInfo.name = pCmd->name();
			cfgInfo.min = pCmd->minValue();
			cfgInfo.max = pCmd->maxValue();
			cfgInfo.value = pCmd->value();
			cfgInfo.high_addr = pCmd->highAddr();
			cfgInfo.middle_addr = pCmd->middleAddr();
			cfgInfo.low_addr = pCmd->lowAddr();
			vecCfg.push_back(cfgInfo);
		}
		mapCfg1[itr->first] = vecCfg;
	}
	return mapCfg1;
}

map<string, vector<CeleX5::CfgInfo>> CeleX5::getCeleX5CfgModified()
{
	return m_mapCfgModified;
}

void CeleX5::writeRegister(int16_t addressH, int16_t addressM, int16_t addressL, uint32_t value, int device_index)
{
	if (addressL == -1)
	{
		wireIn(addressH, value, 0xFF, device_index);
	}
	else
	{
		if (addressM == -1)
		{
			uint32_t valueH = value >> 8;
			uint32_t valueL = 0xFF & value;
			wireIn(addressH, valueH, 0xFF, device_index);
			wireIn(addressL, valueL, 0xFF, device_index);
		}
	}
}

CeleX5::CfgInfo CeleX5::getCfgInfoByName(string csrType, string name, bool bDefault)
{
	map<string, vector<CfgInfo>> mapCfg;
	if (bDefault)
		mapCfg = m_mapCfgDefaults;
	else
		mapCfg = m_mapCfgModified;
	CeleX5::CfgInfo cfgInfo;
	for (auto itr = mapCfg.begin(); itr != mapCfg.end(); itr++)
	{
		string tapName = itr->first;
		if (csrType == tapName)
		{
			vector<CfgInfo> vecCfg = itr->second;
			int index = 0;
			for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
			{
				if ((*itr1).name == name)
				{
					cfgInfo = (*itr1);
					return cfgInfo;
				}
				index++;
			}
			break;
		}
	}
	return cfgInfo;
}

void CeleX5::writeCSRDefaults(string csrType, int device_index)
{
	cout << "CeleX5::writeCSRDefaults: " << csrType << endl;
	m_mapCfgModified[csrType] = m_mapCfgDefaults[csrType];
	for (auto itr = m_mapCfgDefaults.begin(); itr != m_mapCfgDefaults.end(); itr++)
	{
		//cout << "group name: " << itr->first << endl;
		string tapName = itr->first;
		if (csrType == tapName)
		{
			vector<CeleX5::CfgInfo> vecCfg = itr->second;
			for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
			{
				CeleX5::CfgInfo cfgInfo = (*itr1);
				writeRegister(cfgInfo, device_index);
			}
			break;
		}
	}
}

void CeleX5::modifyCSRParameter(string csrType, string cmdName, uint32_t value)
{
	CeleX5::CfgInfo cfgInfo;
	for (auto itr = m_mapCfgModified.begin(); itr != m_mapCfgModified.end(); itr++)
	{
		string tapName = itr->first;
		if (csrType.empty())
		{
			vector<CfgInfo> vecCfg = itr->second;
			int index = 0;
			for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
			{
				if ((*itr1).name == cmdName)
				{
					cfgInfo = (*itr1);
					cout << "CeleX5::modifyCSRParameter: Old value = " << cfgInfo.value << endl;
					//modify the value in m_pMapCfgModified
					cfgInfo.value = value;
					vecCfg[index] = cfgInfo;
					m_mapCfgModified[tapName] = vecCfg;
					cout << "CeleX5::modifyCSRParameter: New value = " << cfgInfo.value << endl;
					break;
				}
				index++;
			}
		}
		else
		{
			if (csrType == tapName)
			{
				vector<CfgInfo> vecCfg = itr->second;
				int index = 0;
				for (auto itr1 = vecCfg.begin(); itr1 != vecCfg.end(); itr1++)
				{
					if ((*itr1).name == cmdName)
					{
						cfgInfo = (*itr1);
						cout << "CeleX5::modifyCSRParameter: Old value = " << cfgInfo.value << endl;
						//modify the value in m_pMapCfgModified
						cfgInfo.value = value;
						vecCfg[index] = cfgInfo;
						m_mapCfgModified[tapName] = vecCfg;
						cout << "CeleX5::modifyCSRParameter: New value = " << cfgInfo.value << endl;
						break;
					}
					index++;
				}
				break;
			}
		}
	}
}

bool CeleX5::configureSettings(CeleX5::DeviceType type, int device_index)
{
	if (CeleX5::CeleX5_OpalKelly == type)
	{
		//--------------- Step1 ---------------
		wireIn(94, 1, 0xFF, device_index);

		//--------------- Step2 ---------------
		//Disable PLL
		wireIn(150, 0, 0xFF, device_index);
		//Load PLL Parameters
		writeCSRDefaults("PLL_Parameters", device_index);
		//Enable PLL
		wireIn(150, 1, 0xFF, device_index);

		//--------------- Step3 ---------------
		enterCFGMode(device_index);

		//Load Other Parameters
		wireIn(92, 12, 0xFF, device_index); //

		//Load Sensor Core Parameters
		wireIn(220, 0, 0xFF, device_index); //AUTOISP_PROFILE_ADDR
		writeCSRDefaults("Sensor_Core_Parameters", device_index);

		//wireIn(91, 11, 0xFF);
		//wireIn(18, 151, 0xFF); //23

		//Load Sensor_Operation_Mode_Control_Parameters
		wireIn(53, 0, 0xFF, device_index); //SENSOR_MODE_1
		wireIn(54, 3, 0xFF, device_index); //SENSOR_MODE_2
		wireIn(55, 4, 0xFF, device_index); //SENSOR_MODE_3

		wireIn(57, 60, 0xFF, device_index); //EVENT_DURATION, low byte
		wireIn(58, 0, 0xFF, device_index); //EVENT_DURATION, high byte

		wireIn(64, 0, 0xFF, device_index); //SENSOR_MODE_SELECT, fixed mode

		enterStartMode(device_index);
	}
	else if (CeleX5::CeleX5_MIPI == type)
	{
		setALSEnabled(false, device_index);
		if (m_pCeleDriver)
			m_pCeleDriver->openStream(device_index);

		//--------------- Step1 ---------------
		wireIn(94, 0, 0xFF, device_index); //PADDR_EN

		//--------------- Step2: Load PLL Parameters ---------------
		//Disable PLL
		cout << "--- Disable PLL ---" << endl;
		wireIn(150, 0, 0xFF, device_index); //PLL_PD_B
		//Load PLL Parameters
		cout << endl << "--- Load PLL Parameters ---" << endl;
		writeCSRDefaults("PLL_Parameters", device_index);
		//Enable PLL
		cout << "--- Enable PLL ---" << endl;
		wireIn(150, 1, 0xFF, device_index); //PLL_PD_B

		//--------------- Step3: Load MIPI Parameters ---------------
		cout << endl << "--- Disable MIPI ---" << endl;
		disableMIPI(device_index);

		cout << endl << "--- Load MIPI Parameters ---" << endl;
		writeCSRDefaults("MIPI_Parameters", device_index);
		//writeRegister(115, -1, 114, 120); //MIPI_PLL_DIV_N

		//Enable MIPI
		cout << endl << "--- Enable MIPI ---" << endl;
		enableMIPI(device_index);

		//--------------- Step4: ---------------
		cout << endl << "--- Enter CFG Mode ---" << endl;
		enterCFGMode(device_index);

		//----- Load Sensor Core Parameters -----
		//if (m_bAutoISPEnabled)
		{
			//--------------- for auto isp --------------- 
			wireIn(220, 3, 0xFF, device_index); //AUTOISP_PROFILE_ADDR
			//Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters", device_index);
			writeRegister(22, -1, 23, m_arrayBrightness[3], device_index);

			wireIn(220, 2, 0xFF, device_index); //AUTOISP_PROFILE_ADDR
			 //Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters", device_index);
			writeRegister(22, -1, 23, m_arrayBrightness[2], device_index);

			wireIn(220, 1, 0xFF, device_index); //AUTOISP_PROFILE_ADDR
			//Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters", device_index);
			writeRegister(22, -1, 23, m_arrayBrightness[1], device_index);

			wireIn(220, 0, 0xFF, device_index); //AUTOISP_PROFILE_ADDR
			//Load Sensor Core Parameters
			writeCSRDefaults("Sensor_Core_Parameters", device_index);
			writeRegister(22, -1, 23, m_uiBrightness[device_index], device_index);

			wireIn(221, 0, 0xFF, device_index); //AUTOISP_BRT_EN, disable auto ISP
			wireIn(222, 0, 0xFF, device_index); //AUTOISP_TEM_EN
			wireIn(223, 0, 0xFF, device_index); //AUTOISP_TRIGGER

			writeRegister(225, -1, 224, m_uiAutoISPRefreshTime, device_index); //AUTOISP_REFRESH_TIME

			writeRegister(235, -1, 234, m_arrayISPThreshold[0], device_index); //AUTOISP_BRT_THRES1
			writeRegister(237, -1, 236, m_arrayISPThreshold[1], device_index); //AUTOISP_BRT_THRES2
			writeRegister(239, -1, 238, m_arrayISPThreshold[2], device_index); //AUTOISP_BRT_THRES3

			writeRegister(233, -1, 232, 1500, device_index); //AUTOISP_BRT_VALUE
		}
		writeCSRDefaults("Sensor_Operation_Mode_Control_Parameters", device_index);
		//wireIn(53, 3, 0xFF, device_index);

		writeCSRDefaults("Sensor_Data_Transfer_Parameters", device_index);
		wireIn(73, m_iEventDataFormat[device_index], 0xFF, device_index); //EVENT_PACKET_SELECT
		m_pDataProcessThread[device_index]->getDataProcessor5()->setMIPIDataFormat(m_iEventDataFormat[device_index]);

		cout << endl << "--- Enter Start Mode ---" << endl;
		/*if (1 == device_index)*/
		enterStartMode(device_index);
	}
	return true;
}

void CeleX5::wireIn(uint32_t address, uint32_t value, uint32_t mask, int device_index)
{
	if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		if (m_pCeleDriver)
		{
			if (isAutoISPEnabled())
			{
				setALSEnabled(false, device_index);
#ifdef _WIN32
				Sleep(2);
#else
				usleep(1000 * 2);
#endif
			}
			if (m_pCeleDriver->i2c_set(address, value, device_index))
			{
				//cout << "CeleX5::wireIn(i2c_set): address = " << address << ", value = " << value << endl;
			}
			if (isAutoISPEnabled())
			{
				setALSEnabled(true, device_index);
			}
		}
	}
	else if (CeleX5::CeleX5_OpalKelly == m_emDeviceType)
	{
		//To Do
	}
}

void CeleX5::writeRegister(CfgInfo cfgInfo, int device_index)
{
	if (cfgInfo.low_addr == -1)
	{
		wireIn(cfgInfo.high_addr, cfgInfo.value, 0xFF, device_index);
	}
	else
	{
		if (cfgInfo.middle_addr == -1)
		{
			uint32_t valueH = cfgInfo.value >> 8;
			uint32_t valueL = 0xFF & cfgInfo.value;
			wireIn(cfgInfo.high_addr, valueH, 0xFF, device_index);
			wireIn(cfgInfo.low_addr, valueL, 0xFF, device_index);
		}
	}
}

bool CeleX5::openBinFile(std::string filePath, int device_index)
{
	//cout << __FUNCTION__ << endl;
	m_uiPackageCount[0] = 0;
	m_bFirstReadFinished = false;
	if (m_ifstreamPlayback[device_index].is_open())
	{
		m_ifstreamPlayback[device_index].close();
	}
	m_pDataProcessThread[device_index]->clearData();
	//
	m_ifstreamPlayback[device_index].open(filePath.c_str(), std::ios::binary);
	if (!m_ifstreamPlayback[device_index].good())
	{
		cout << "Can't Open File: " << filePath.c_str() << endl;
		return false;
	}
	m_pDataProcessThread[device_index]->setIsPlayback(true);
	m_pDataProcessThread[device_index]->setPlaybackState(Playing);
	// read header
	m_ifstreamPlayback[device_index].read((char*)&m_stBinFileHeader[device_index], sizeof(BinFileAttributes));
	cout << "data_type = " << (int)m_stBinFileHeader[device_index].data_type
		<< ", loopA_mode = " << (int)m_stBinFileHeader[device_index].loopA_mode << ", loopB_mode = " << (int)m_stBinFileHeader[device_index].loopB_mode
		<< ", loopC_mode = " << (int)m_stBinFileHeader[device_index].loopC_mode
		<< ", event_data_format = " << (int)m_stBinFileHeader[device_index].event_data_format
		<< ", hour = " << (int)m_stBinFileHeader[device_index].hour << ", minute = " << (int)m_stBinFileHeader[device_index].minute << ", second = " << (int)m_stBinFileHeader[device_index].second
		<< ", package_count = " << (int)m_stBinFileHeader[device_index].package_count << endl;

	m_uiTotalPackageCount[device_index] = m_stBinFileHeader[device_index].package_count;
	m_pDataProcessThread[device_index]->start();
	//if (m_emDeviceType == CeleX5::CeleX5_MIPI)
	{
		setEventDataFormat(m_stBinFileHeader[device_index].event_data_format, device_index);
	}
	m_pDataProcessThread[device_index]->getDataProcessor5()->setSensorFixedMode(CeleX5::CeleX5Mode(m_stBinFileHeader[device_index].loopA_mode));
	return true;
}

bool CeleX5::readBinFileData(int device_index)
{
	if (m_pDataProcessThread[device_index]->queueSize() > 20)
	{
		return false;
	}
	if (!m_ifstreamPlayback[device_index].is_open())
	{
		return false;
	}
	//cout << __FUNCTION__ << ": ---------------------------- indevie_index = " << device_index << endl;
	bool eof = false;
	uint64_t ifReadPos = m_ifstreamPlayback[device_index].tellg();
	uint32_t lenToRead = 0;
	m_ifstreamPlayback[device_index].read((char*)&lenToRead, 4);
	//cout << "---- lenToRead = " << lenToRead << endl;
	if (NULL == m_pDataToRead)
		m_pDataToRead = new uint8_t[2048000];
	m_ifstreamPlayback[device_index].read((char*)m_pDataToRead, lenToRead);
	//cout << "lenToRead = " << lenToRead << endl;
	//
	int dataLen = m_ifstreamPlayback[device_index].gcount();
	if (dataLen > 0)
	{
		m_uiPackageCount[device_index]++;

		//cout << "data_type = " << (int)m_stBinFileHeader[device_index].data_type << endl;

		if ((0x02 & m_stBinFileHeader[device_index].data_type) == 0x02) //has IMU data
		{
			time_t timeStamp;
			m_ifstreamPlayback[device_index].read((char*)&timeStamp, 8);
			m_pDataProcessThread[device_index]->addData(m_pDataToRead, dataLen, timeStamp);

			int imuSize = 0;
			IMURawData imuRawData;
			vector<IMURawData> vecIMUData;
			m_ifstreamPlayback[device_index].read((char*)&imuSize, 4);
			if (imuSize > 0)
			{
				for (int i = 0; i < imuSize; i++)
				{
					m_ifstreamPlayback[device_index].read((char*)&imuRawData, sizeof(IMURawData));
					//cout << "imuRawData.time_stamp: "<<imuRawData.time_stamp << endl;
					vecIMUData.push_back(imuRawData);
				}
				m_pDataProcessThread[device_index]->addIMUData(vecIMUData);
			}
		}
		else
		{
			m_pDataProcessThread[device_index]->addData(m_pDataToRead, dataLen);
		}
		if (!m_bFirstReadFinished)
		{
			m_vecPackagePos.push_back(ifReadPos);
		}
		//cout << "package_count = " << m_uiPackageCount[device_index] << endl;
	}
	if (m_ifstreamPlayback[device_index].eof())
	{
		eof = true;
		m_bFirstReadFinished = true;
		//m_ifstreamPlayback[device_index].close();
		setPlaybackState(BinReadFinished);
		//cout << "Read Playback file Finished!" << endl;

		m_uiTotalPackageCount[device_index] = m_uiPackageCount[device_index];
	}
	return eof;
}

CeleX5::BinFileAttributes CeleX5::getBinFileAttributes(int device_index)
{
	return m_stBinFileHeader[device_index];
}

uint32_t CeleX5::getTotalPackageCount()
{
	return m_uiPackageCount[0];
}

uint32_t CeleX5::getCurrentPackageNo()
{
	return m_pDataProcessThread[0]->getPackageNo();
}

void CeleX5::setCurrentPackageNo(uint32_t value)
{
	setPlaybackState(Replay);
	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		m_uiPackageCount[i] = value;
		m_ifstreamPlayback[i].clear();
		m_ifstreamPlayback[i].seekg(m_vecPackagePos[value], ios::beg);
	}
	m_pDataProcessThread[0]->setPackageNo(value);
}

void CeleX5::replay()
{
	setPlaybackState(Replay);
	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		m_uiPackageCount[i] = 0;
		m_ifstreamPlayback[i].clear();
		m_ifstreamPlayback[i].seekg(sizeof(BinFileAttributes), ios::beg);
		
		m_pDataProcessThread[i]->setPackageNo(0);
	}
}

void CeleX5::play()
{
	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		m_pDataProcessThread[i]->resume();
	}
}

void CeleX5::pause()
{
	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		m_pDataProcessThread[i]->suspend();
	}
}

PlaybackState CeleX5::getPlaybackState()
{
	return m_pDataProcessThread[0]->getPlaybackState();
}

void CeleX5::setPlaybackState(PlaybackState state)
{
	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		m_pDataProcessThread[i]->setPlaybackState(state);
	}
}

void CeleX5::setIsPlayBack(bool state)
{
	for (int i = 0; i < MAX_SENSOR_NUM; i++)
	{
		m_pDataProcessThread[i]->setIsPlayback(state);
		if (!state) //stop playback
		{
			if (m_ifstreamPlayback[i].is_open())
			{
				m_ifstreamPlayback[i].close();
			}
		}
	}
}

//Enter CFG Mode
void CeleX5::enterCFGMode(int device_index)
{
	wireIn(93, 0, 0xFF, device_index);
	wireIn(90, 1, 0xFF, device_index);
}

//Enter Start Mode
void CeleX5::enterStartMode(int device_index)
{
	wireIn(90, 0, 0xFF, device_index);
	wireIn(93, 1, 0xFF, device_index);
}

void CeleX5::disableMIPI(int device_index)
{
	wireIn(139, 0, 0xFF, device_index);
	wireIn(140, 0, 0xFF, device_index);
	wireIn(141, 0, 0xFF, device_index);
	wireIn(142, 0, 0xFF, device_index);
	wireIn(143, 0, 0xFF, device_index);
	wireIn(144, 0, 0xFF, device_index);
}

void CeleX5::enableMIPI(int device_index)
{
	wireIn(139, 1, 0xFF, device_index);
	wireIn(140, 1, 0xFF, device_index);
	wireIn(141, 1, 0xFF, device_index);
	wireIn(142, 1, 0xFF, device_index);
	wireIn(143, 1, 0xFF, device_index);
	wireIn(144, 1, 0xFF, device_index);
}

void CeleX5::clearData(int device_index)
{
	m_pDataProcessThread[device_index]->clearData();
	if (CeleX5::CeleX5_MIPI == m_emDeviceType)
	{
		m_pCeleDriver->clearData(device_index);
	}
}
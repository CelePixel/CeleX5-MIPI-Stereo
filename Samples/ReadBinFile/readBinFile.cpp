/*
* Copyright (c) 2017-2017-2018 CelePixel Technology Co. Ltd. All Rights Reserved
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

#include <opencv2/opencv.hpp>
#include <celex5/celex5.h>
#include <celex5/celex5datamanager.h>

#ifdef _WIN32
#include <windows.h>
#else
#include<unistd.h>
#include <signal.h>
#endif

#define FPN_PATH_M    "../Samples/config/FPN_M_2.txt"
#define FPN_PATH_S    "../Samples/config/FPN_S_2.txt"

#define BIN_FILE_M    "E:/API/CeleX5/MIPI/Binocular/Samples/MipiData_20190522_150947149_E_100M_M.bin"	//your Master's bin file path 
#define BIN_FILE_S    "E:/API/CeleX5/MIPI/Binocular/Samples/MipiData_20190522_150947149_E_100M_S.bin"	//your Slave's bin file path

CeleX5 *pCeleX5 = new CeleX5;

class SensorDataObserver : public CeleX5DataManager
{
public:
	SensorDataObserver(CX5SensorDataServer* pServer)
	{
		m_pServer = pServer;
		m_pServer->registerData(this, CeleX5DataManager::CeleX_Frame_Data);
	}
	~SensorDataObserver()
	{
		m_pServer->unregisterData(this, CeleX5DataManager::CeleX_Frame_Data);
	}
	virtual void onFrameDataUpdated(CeleX5ProcessedData* pSensorData);//overrides Observer operation

	CX5SensorDataServer* m_pServer;
};

int g_iDeviceIndex = 0;
void SensorDataObserver::onFrameDataUpdated(CeleX5ProcessedData* pSensorData)
{
	if (NULL == pSensorData)
		return;

	g_iDeviceIndex = pSensorData->getDeviceIndex();
	while (pSensorData->getDeviceIndex() == g_iDeviceIndex)
	{
#ifdef _WIN32
		Sleep(1);
#else
		usleep(10);
#endif
	}

	if (pSensorData->getSensorMode() == CeleX5::Full_Picture_Mode)	//if the bin file is fullpic mode
	{
		if (pSensorData->getDeviceIndex() == 0) //Master Sensor
		{
			if (!pCeleX5->getFullPicMat(0).empty())
			{
				cv::Mat fullPicMatMaster = pCeleX5->getFullPicMat(0);
				cv::imshow("Master-FullPic", fullPicMatMaster);
			}
		}
		else //Slave Sensor
		{
			if (!pCeleX5->getFullPicMat(1).empty())
			{
				cv::Mat fullPicMatSlave = pCeleX5->getFullPicMat(1);
				cv::imshow("Slave-FullPic", fullPicMatSlave);
			}
		}
	}
	else if (pSensorData->getSensorMode() == CeleX5::Event_Address_Only_Mode)		//if the bin file is event mode
	{
		if (pSensorData->getDeviceIndex() == 0) //Master Sensor
		{
			if (!pCeleX5->getEventPicMat(CeleX5::EventBinaryPic, 0).empty())
			{
				cv::Mat eventPicMatMaster = pCeleX5->getEventPicMat(CeleX5::EventBinaryPic, 0);
				cv::imshow("Master-Event-EventBinaryPic", eventPicMatMaster);
			}
		}
		else //Slave Sensor
		{
			if (!pCeleX5->getEventPicMat(CeleX5::EventBinaryPic, 1).empty())
			{
				cv::Mat eventPicMatSlave = pCeleX5->getEventPicMat(CeleX5::EventBinaryPic, 1);
				cv::imshow("Slave-Event-EventBinaryPic", eventPicMatSlave);
			}
		}

	}
	cv::waitKey(10);
}

int main()
{
	if (pCeleX5 == NULL)
		return 0;
	pCeleX5->openSensor(CeleX5::CeleX5_MIPI);

	pCeleX5->setFpnFile(FPN_PATH_M, 0);
	pCeleX5->setFpnFile(FPN_PATH_S, 1);

	SensorDataObserver* pSensorData = new SensorDataObserver(pCeleX5->getSensorDataServer(0));
	SensorDataObserver* pSensorData1 = new SensorDataObserver(pCeleX5->getSensorDataServer(1));

	bool success = pCeleX5->openBinFile(BIN_FILE_M, 0);	//open the Master's bin file
	bool success1 = pCeleX5->openBinFile(BIN_FILE_S, 1); //open the Salve's bin file

	if (success && success1)
	{
		pCeleX5->alignBinFileData();

		while (true)
		{
			pCeleX5->readBinFileData(0);	//start reading the bin file
			pCeleX5->readBinFileData(1);	//start reading the bin file

#ifdef _WIN32
			Sleep(1);
#else
			usleep(1000);
#endif
		}
	}
	return 1;
}

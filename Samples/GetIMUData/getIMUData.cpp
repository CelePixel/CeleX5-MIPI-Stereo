/*
* Copyright (c) 2017-2018 CelePixel Technology Co. Ltd. All Rights Reserved
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

#define MAT_ROWS 800
#define MAT_COLS 1280
#define FPN_PATH_M    "../Samples/config/FPN_M_2.txt"
#define FPN_PATH_S    "../Samples/config/FPN_S_2.txt"

#ifdef _WIN32
#include <windows.h>
#else
#include<unistd.h>
#endif

using namespace std;
using namespace cv;

CeleX5 *pCeleX = new CeleX5;

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
		m_pServer->registerData(this, CeleX5DataManager::CeleX_Frame_Data);
	}
	virtual void onFrameDataUpdated(CeleX5ProcessedData* pSensorData);//overrides Observer operation

	CX5SensorDataServer* m_pServer;
};

void SensorDataObserver::onFrameDataUpdated(CeleX5ProcessedData* pSensorData)
{
	if (NULL == pSensorData)
		return;

	int currDeviceIndex = pSensorData->getDeviceIndex();

	if (CeleX5::Event_Address_Only_Mode == pSensorData->getSensorMode() ||
		CeleX5::Event_Intensity_Mode == pSensorData->getSensorMode())
	{
		std::vector<EventData> vecEvent;
		pCeleX->getEventDataVector(vecEvent, currDeviceIndex);

		if (vecEvent.size() > 0)
		{
			vector<IMUData> imu;
			if (pCeleX->getIMUData(imu, currDeviceIndex) > 0)
			{
				for (int i = 0; i < imu.size(); i++)
				{
					cout << "------ currDeviceIndex= "<< currDeviceIndex<<" ,x_ACC = " << imu[i].x_ACC << " ,y_ACC = " << imu[i].y_ACC << " ,z_ACC = " << imu[i].z_ACC << endl;
				}
			}
		}
	}
	else if (CeleX5::Full_Picture_Mode == pSensorData->getSensorMode())
	{
		vector<IMUData> imu;
		if (pCeleX->getIMUData(imu, currDeviceIndex) > 0)
		{
			for (int i = 0; i < imu.size(); i++)
			{
				cout << "------ currDeviceIndex= " << currDeviceIndex << " ,x_ACC = " << imu[i].x_ACC << " ,y_ACC = " << imu[i].y_ACC << " ,z_ACC = " << imu[i].z_ACC << endl;
			}
		}
	}
}

int main()
{
	if (NULL == pCeleX)
		return 0;

	pCeleX->openSensor(CeleX5::CeleX5_MIPI);
	pCeleX->setFpnFile(FPN_PATH_M, 0);
	pCeleX->setFpnFile(FPN_PATH_S, 1);

	pCeleX->setSensorFixedMode(CeleX5::Event_Address_Only_Mode, 0);
	pCeleX->setSensorFixedMode(CeleX5::Event_Address_Only_Mode, 1);

	SensorDataObserver* pSensorData = new SensorDataObserver(pCeleX->getSensorDataServer(0));
	SensorDataObserver* pSensorData1 = new SensorDataObserver(pCeleX->getSensorDataServer(1));

	while (true)
	{
#ifdef _WIN32
		Sleep(10);
#else
		usleep(1000 * 10);
#endif
	}
	return 1;
}
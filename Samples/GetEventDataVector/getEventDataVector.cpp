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

CeleX5 *pCeleX = new CeleX5;
//ofstream of_event;

using namespace std;
using namespace cv;
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

void SensorDataObserver::onFrameDataUpdated(CeleX5ProcessedData* pSensorData)
{
	if (NULL == pSensorData)
		return;
	if (CeleX5::Event_Intensity_Mode == pSensorData->getSensorMode())
	{
		int count1 = 0, count2 = 0, count3 = 0;
		std::vector<EventData> vecEvent, vecEvent1;
		int currDeviceIndex = pSensorData->getDeviceIndex();

		pCeleX->getEventDataVector(vecEvent, currDeviceIndex);

		cv::Mat mat = cv::Mat::zeros(cv::Size(1280, 800), CV_8UC1);
		int dataSize = vecEvent.size();

		for (int i = 0; i < dataSize; i++)
		{
			mat.at<uchar>(800 - vecEvent[i].row - 1, 1280 - vecEvent[i].col - 1) = vecEvent[i].brightness;
			//of_event << vecEvent[i].row << "  " << vecEvent[i].col << "  " << vecEvent[i].brightness << "  " << vecEvent[i].polarity << "  " << vecEvent[i].t << endl;

			/*if (vecEvent[i].polarity == 1)
				count1++;
			else if (vecEvent[i].polarity == -1)
				count2++;
			else
				count3++;*/
		}

		if (dataSize > 0)
		{
			if (currDeviceIndex == 0)
				cv::imshow("Master", mat);
			else
				cv::imshow("Slave", mat);
		}
		cv::waitKey(1);
	}
}

int main()
{
	if (NULL == pCeleX)
		return 0;

	pCeleX->openSensor(CeleX5::CeleX5_MIPI);
	pCeleX->setFpnFile(FPN_PATH_M, 0);
	pCeleX->setFpnFile(FPN_PATH_S, 1);

	pCeleX->setSensorFixedMode(CeleX5::Event_Intensity_Mode, 0);
	pCeleX->setSensorFixedMode(CeleX5::Event_Intensity_Mode, 1);

	SensorDataObserver* pSensorData = new SensorDataObserver(pCeleX->getSensorDataServer(0));
	SensorDataObserver* pSensorData1 = new SensorDataObserver(pCeleX->getSensorDataServer(1));

	//of_event.open("event_data.txt");

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

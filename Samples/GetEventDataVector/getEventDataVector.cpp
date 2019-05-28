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
#include <signal.h>
#endif

CeleX5 *pCeleX5 = new CeleX5;

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
		vector<EventData> vecEvent, vecEvent1;
		int currDeviceIndex = pSensorData->getDeviceIndex();

		pCeleX5->getEventDataVector(vecEvent, currDeviceIndex);

		Mat mat = Mat::zeros(cv::Size(1280, 800), CV_8UC1);
		int dataSize = vecEvent.size();

		for (int i = 0; i < dataSize; i++)
		{
			mat.at<uchar>(800 - vecEvent[i].row - 1, 1280 - vecEvent[i].col - 1) = vecEvent[i].adc;
		}
		if (dataSize > 0)
		{
			if (currDeviceIndex == 0)
			{
				imshow("Master", mat);
				//cout << "----- Master -----" << vecEvent[dataSize - 1].t_increasing << endl;
			}
			else
			{
				imshow("Slave", mat);
				//cout << "----- Slave -----" << vecEvent[dataSize - 1].t_increasing << endl;
			}
		}
		waitKey(1);
	}
}

#ifdef _WIN32
bool exit_handler(DWORD fdwctrltype)
{
	switch (fdwctrltype)
	{
		//ctrl-close: confirm that the user wants to exit.
	case CTRL_CLOSE_EVENT:
	case CTRL_C_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		delete pCeleX5;
		pCeleX5 = NULL;
		return(true);
	default:
		return false;
	}
}
#else
void exit_handler(int sig_num)
{
	printf("SIGNAL received: num =%d\n", sig_num);
	if (sig_num == 1 || sig_num == 2 || sig_num == 3 || sig_num == 9 || sig_num == 15)
	{
		delete pCeleX5;
		pCeleX5 = NULL;
		exit(0);
	}
}
#endif

int main()
{
	if (NULL == pCeleX5)
		return 0;

	pCeleX5->openSensor(CeleX5::CeleX5_MIPI);
	pCeleX5->setFpnFile(FPN_PATH_M, 0);
	pCeleX5->setFpnFile(FPN_PATH_S, 1);

	pCeleX5->setSensorFixedMode(CeleX5::Event_Intensity_Mode, 0);
	pCeleX5->setSensorFixedMode(CeleX5::Event_Intensity_Mode, 1);

	SensorDataObserver* pSensorData = new SensorDataObserver(pCeleX5->getSensorDataServer(0));
	SensorDataObserver* pSensorData1 = new SensorDataObserver(pCeleX5->getSensorDataServer(1));

#ifdef _WIN32
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)exit_handler, true);
#else
	// install signal use sigaction
	struct sigaction sig_action;
	sigemptyset(&sig_action.sa_mask);
	sig_action.sa_flags = 0;
	sig_action.sa_handler = exit_handler;
	sigaction(SIGHUP, &sig_action, NULL);  // 1
	sigaction(SIGINT, &sig_action, NULL);  // 2
	sigaction(SIGQUIT, &sig_action, NULL); // 3
	sigaction(SIGKILL, &sig_action, NULL); // 9
	sigaction(SIGTERM, &sig_action, NULL); // 15
#endif

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

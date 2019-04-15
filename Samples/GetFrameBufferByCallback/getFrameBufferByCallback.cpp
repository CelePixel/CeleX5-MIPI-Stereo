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
#include <celex5/celex5processeddata.h>

#ifdef _WIN32
#include <windows.h>
#else
#include<unistd.h>
#endif

#define FPN_PATH_M    "../Samples/config/FPN_M_2.txt"
#define FPN_PATH_S    "../Samples/config/FPN_S_2.txt"

CeleX5 *pCeleX = new CeleX5;

class SensorDataObserver : public CeleX5DataManager
{
public:
	SensorDataObserver(CX5SensorDataServer* pServer) 
	{ 
		m_pServer = pServer;
		m_pServer->registerData(this, CeleX5DataManager::CeleX_Frame_Data);
		m_pServer->registerData(this, CeleX5DataManager::CeleX_Frame_Data);

	}
	~SensorDataObserver() 
	{
		m_pServer->unregisterData(this, CeleX5DataManager::CeleX_Frame_Data);
	}
	virtual void onFrameDataUpdated(CeleX5ProcessedData* pSensorData);//overrides Observer operation

	CX5SensorDataServer* m_pServer;
};
	
//optical-flow raw data - display color image
void coloredOpticalFlowPic(cv::Mat &rawOpticalFlow,cv::Mat &matOpticalColor)
{
	matOpticalColor = cv::Mat(800, 1280, CV_8UC3);

	for (int i = 0; i < matOpticalColor.rows; ++i)
	{
		cv::Vec3b *p = matOpticalColor.ptr<cv::Vec3b>(i);

		for (int j = 0; j < matOpticalColor.cols; ++j)
		{
			int value = rawOpticalFlow.at<uchar>(i, j);
			//std::cout << value << std::endl;
			if (value == 0)
			{
				p[j][0] = 0;
				p[j][1] = 0;
				p[j][2] = 0;
			}
			else if (value < 50)	//blue
			{
				p[j][0] = 255;
				p[j][1] = 0;
				p[j][2] = 0;
			}
			else if (value < 100)
			{
				p[j][0] = 255;
				p[j][1] = 255;
				p[j][2] = 0;
			}
			else if (value < 150)	//green
			{
				p[j][0] = 0;
				p[j][1] = 255;
				p[j][2] = 0;
			}
			else if (value < 200)
			{
				p[j][0] = 0;
				p[j][1] = 255;
				p[j][2] = 255;
			}
			else	//red
			{
				p[j][0] = 0;
				p[j][1] = 0;
				p[j][2] = 255;
			}
		}
	}
}

void SensorDataObserver::onFrameDataUpdated(CeleX5ProcessedData* pSensorData)
{
	if (NULL == pSensorData)
		return;
	CeleX5::CeleX5Mode sensorMode = pSensorData->getSensorMode();

	int currDeviceIndex = pSensorData->getDeviceIndex();
	cout << "currDeviceIndex: " << currDeviceIndex << endl;

	if (CeleX5::Full_Picture_Mode == sensorMode)
	{
		//get fullpic when sensor works in FullPictureMode
		if (!pCeleX->getFullPicMat(0).empty())
		{
			//full-frame picture
			cv::Mat matFullPicMaster = pCeleX->getFullPicMat(0);
			cv::Mat matFullPicSlave = pCeleX->getFullPicMat(1);

			cv::imshow("FullPic", matFullPicMaster);
			cv::waitKey(1);
		}
	}
	else if (CeleX5::Event_Address_Only_Mode == sensorMode)
	{
		//get buffers when sensor works in EventMode
		if (!pCeleX->getEventPicMat(CeleX5::EventBinaryPic).empty())
		{
			//event binary pic
			cv::Mat matEventPicMaster = pCeleX->getEventPicMat(CeleX5::EventBinaryPic,0);
			cv::Mat matEventPicSlave = pCeleX->getEventPicMat(CeleX5::EventBinaryPic,1);

			cv::imshow("Master-Event Binary Pic", matEventPicMaster);
			cv::imshow("Slave-Event Binary Pic", matEventPicSlave);

			cvWaitKey(1);
		}
	}
	else if (CeleX5::Full_Optical_Flow_S_Mode == sensorMode)
	{
		//get buffers when sensor works in FullPic_Event_Mode
		if (!pCeleX->getOpticalFlowPicMat(CeleX5::Full_Optical_Flow_Pic).empty())
		{
			//full-frame optical-flow pic
			cv::Mat matOpticalRawMaster = pCeleX->getOpticalFlowPicMat(CeleX5::Full_Optical_Flow_Pic,0);
			cv::Mat matOpticalRawSlave = pCeleX->getOpticalFlowPicMat(CeleX5::Full_Optical_Flow_Pic,1);

			cv::Mat matOpticalColorMaster;
			cv::Mat matOpticalColorSlave;

			coloredOpticalFlowPic(matOpticalRawMaster,matOpticalColorMaster);
			coloredOpticalFlowPic(matOpticalRawSlave, matOpticalColorSlave);

			cv::imshow("Master-Optical-Flow Buffer - Color", matOpticalColorMaster);
			cv::imshow("Slave-Optical-Flow Buffer - Color", matOpticalColorSlave);

			cvWaitKey(1);
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

	pCeleX->setSensorFixedMode(CeleX5::Full_Optical_Flow_S_Mode, 0);
	pCeleX->setSensorFixedMode(CeleX5::Full_Optical_Flow_S_Mode, 1);

	SensorDataObserver* pSensorData = new SensorDataObserver(pCeleX->getSensorDataServer(0));
	SensorDataObserver* pSensorData1 = new SensorDataObserver(pCeleX->getSensorDataServer(1));

	while (true)
	{
#ifdef _WIN32
		Sleep(30);
#else
		usleep(1000 * 30);
#endif
	}
	return 1;
}

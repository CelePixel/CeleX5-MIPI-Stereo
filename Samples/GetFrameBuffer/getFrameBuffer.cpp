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

#define FPN_PATH_M    "../Samples/config/FPN_M_2.txt"
#define FPN_PATH_S    "../Samples/config/FPN_S_2.txt"

int main()
{
	CeleX5 *pCeleX = new CeleX5;
	if (NULL == pCeleX)
		return 0;
	pCeleX->openSensor(CeleX5::CeleX5_MIPI);
	pCeleX->setFpnFile(FPN_PATH_M, 0);
	pCeleX->setFpnFile(FPN_PATH_S, 1);

	CeleX5::CeleX5Mode sensorMode = CeleX5::Full_Picture_Mode;
	pCeleX->setSensorFixedMode(sensorMode,0);
	pCeleX->setSensorFixedMode(sensorMode,1);

	int imgSize = 1280 * 800;
	unsigned char* pBuffer1 = new unsigned char[imgSize];
	unsigned char* pBuffer2 = new unsigned char[imgSize];

	while (true)
	{
		if (sensorMode == CeleX5::Full_Picture_Mode)
		{
			//get fullpic when sensor works in FullPictureMode
			pCeleX->getFullPicBuffer(pBuffer1, 0); //full pic
			pCeleX->getFullPicBuffer(pBuffer2, 1); 

			cv::Mat matFullPicMaster(800, 1280, CV_8UC1, pBuffer1);
			cv::Mat matFullPicSlave(800, 1280, CV_8UC1, pBuffer2);

			cv::imshow("Master-FullPic", matFullPicMaster);
			cv::imshow("Slave-FullPic", matFullPicSlave);

			cvWaitKey(10);
		}
		else if (sensorMode == CeleX5::Event_Address_Only_Mode)
		{
			//get buffers when sensor works in EventMode
			pCeleX->getEventPicBuffer(pBuffer1, CeleX5::EventBinaryPic, 0); //event binary pic
			pCeleX->getEventPicBuffer(pBuffer2, CeleX5::EventBinaryPic, 1); //event binary pic

			cv::Mat matEventPicMaster(800, 1280, CV_8UC1, pBuffer1);
			cv::Mat matEventPicSlave(800, 1280, CV_8UC1, pBuffer2);

			cv::imshow("Master-Event-EventBinaryPic", matEventPicMaster);
			cv::imshow("Slave-Event-EventBinaryPic", matEventPicSlave);

			cvWaitKey(10);
		}
	}
	return 1;
}

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

	if (pCeleX == NULL)
		return 0;

	pCeleX->openSensor(CeleX5::CeleX5_MIPI);
	pCeleX->setFpnFile(FPN_PATH_M, 0);
	pCeleX->setFpnFile(FPN_PATH_S, 1);

	CeleX5::CeleX5Mode sensorMode = CeleX5::Event_Address_Only_Mode;
	pCeleX->setSensorFixedMode(sensorMode, 0);
	pCeleX->setSensorFixedMode(sensorMode, 1);

	pCeleX->setClockRate(100);

	while (true)
	{
		if (sensorMode == CeleX5::Full_Picture_Mode)
		{
			if (!pCeleX->getFullPicMat().empty())
			{
				cv::Mat fullPicMatMaster = pCeleX->getFullPicMat(0);
				cv::Mat fullPicMatSlave = pCeleX->getFullPicMat(1);

				cv::imshow("Master-FullPic", fullPicMatMaster);
				cv::imshow("Slave-FullPic", fullPicMatSlave);

				cv::waitKey(10);
			}
		}
		else if (sensorMode == CeleX5::Event_Address_Only_Mode)
		{
			if (!pCeleX->getEventPicMat(CeleX5::EventBinaryPic).empty())
			{
				cv::Mat eventPicMatMaster = pCeleX->getEventPicMat(CeleX5::EventBinaryPic,0);
				cv::Mat eventPicMatSlave = pCeleX->getEventPicMat(CeleX5::EventBinaryPic,1);
				cv::imshow("Master-Event-EventBinaryPic", eventPicMatMaster);
				cv::imshow("Slave-Event-EventBinaryPic", eventPicMatSlave);
			}
			cv::waitKey(10);
		}
	}
	return 1;

}

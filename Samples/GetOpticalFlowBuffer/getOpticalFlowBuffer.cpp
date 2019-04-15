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

int main()
{
	CeleX5 *pCeleX = new CeleX5;
	if (NULL == pCeleX)
		return 0;
	pCeleX->openSensor(CeleX5::CeleX5_MIPI);
	pCeleX->setSensorFixedMode(CeleX5::Full_Optical_Flow_S_Mode, 0);
	pCeleX->setSensorFixedMode(CeleX5::Full_Optical_Flow_S_Mode, 1);

	int imgSize = 1280 * 800;
	unsigned char* pOpticalFlowBufferMaster = new unsigned char[imgSize];
	unsigned char* pOpticalFlowBufferSlave = new unsigned char[imgSize];

	while (true)
	{
		pCeleX->getOpticalFlowPicBuffer(pOpticalFlowBufferMaster, CeleX5::Full_Optical_Flow_Pic, 0); //Master's optical-flow raw buffer
		pCeleX->getOpticalFlowPicBuffer(pOpticalFlowBufferSlave, CeleX5::Full_Optical_Flow_Pic, 1); //Slave's optical-flow raw buffer

		cv::Mat matOpticalRawMaster(800, 1280, CV_8UC1, pOpticalFlowBufferMaster);
		cv::Mat matOpticalRawSlave(800, 1280, CV_8UC1, pOpticalFlowBufferSlave);

		//cv::Mat matOpticalRaw = pCeleX->getOpticalFlowPicMat(0);	//You can get the Mat form of optical flow pic directly.
		cv::imshow("Master-Optical-Flow Buffer - Gray", matOpticalRawMaster);
		cv::imshow("Slave-Optical-Flow Buffer - Gray", matOpticalRawSlave);

		cv::Mat matOpticalSpeed = pCeleX->getOpticalFlowPicMat(CeleX5::Full_Optical_Flow_Speed_Pic);	//get Master's optical flow speed buffer
		cv::imshow("Master-Optical-Flow Speed Buffer - Gray", matOpticalSpeed);

		cv::Mat matOpticalDirection = pCeleX->getOpticalFlowPicMat(CeleX5::Full_Optical_Flow_Direction_Pic);	//get Master's optical flow direction buffer
		cv::imshow("Master-Optical-Flow Direction Buffer - Gray", matOpticalDirection);

		//Master's optical-flow raw data - display color image
		cv::Mat matOpticalRawColor(800, 1280, CV_8UC3);
		int index = 0;
		for (int i = 0; i < matOpticalRawColor.rows; ++i)
		{
			cv::Vec3b *p = matOpticalRawColor.ptr<cv::Vec3b>(i);
			for (int j = 0; j < matOpticalRawColor.cols; ++j)
			{
				int value = matOpticalRawMaster.at<uchar>(i, j);
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
		cv::imshow("Optical-Flow Buffer - Color", matOpticalRawColor);
		cvWaitKey(10);
	}
	return 1;
}
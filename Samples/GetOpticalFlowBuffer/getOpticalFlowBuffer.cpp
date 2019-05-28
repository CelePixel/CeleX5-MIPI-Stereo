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

#ifdef _WIN32
#include <windows.h>
#else
#include<unistd.h>
#include <signal.h>
#endif

CeleX5 *pCeleX5 = new CeleX5;

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
	pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_S_Mode, 0);
	pCeleX5->setSensorFixedMode(CeleX5::Full_Optical_Flow_S_Mode, 1);

	int imgSize = 1280 * 800;
	unsigned char* pOpticalFlowBufferMaster = new unsigned char[imgSize];
	unsigned char* pOpticalFlowBufferSlave = new unsigned char[imgSize];

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
		pCeleX5->getOpticalFlowPicBuffer(pOpticalFlowBufferMaster, CeleX5::Full_Optical_Flow_Pic, 0); //Master's optical-flow raw buffer
		pCeleX5->getOpticalFlowPicBuffer(pOpticalFlowBufferSlave, CeleX5::Full_Optical_Flow_Pic, 1); //Slave's optical-flow raw buffer

		cv::Mat matOpticalRawMaster(800, 1280, CV_8UC1, pOpticalFlowBufferMaster);
		cv::Mat matOpticalRawSlave(800, 1280, CV_8UC1, pOpticalFlowBufferSlave);

		cv::imshow("Master-Optical-Flow Buffer - Gray", matOpticalRawMaster);
		cv::imshow("Slave-Optical-Flow Buffer - Gray", matOpticalRawSlave);

		cv::Mat matOpticalSpeedMaster = pCeleX5->getOpticalFlowPicMat(CeleX5::Full_Optical_Flow_Speed_Pic, 0);	//get Master's optical flow speed buffer
		cv::Mat matOpticalSpeedSlave = pCeleX5->getOpticalFlowPicMat(CeleX5::Full_Optical_Flow_Speed_Pic, 1);	//get Slave's optical flow speed buffer
		cv::imshow("Master-Optical-Flow Speed Buffer - Gray", matOpticalSpeedMaster);
		cv::imshow("Slave-Optical-Flow Speed Buffer - Gray", matOpticalSpeedSlave);

		cv::Mat matOpticalDirectionMaster = pCeleX5->getOpticalFlowPicMat(CeleX5::Full_Optical_Flow_Direction_Pic, 0);	//get Master's optical flow direction buffer
		cv::Mat matOpticalDirectionSlave = pCeleX5->getOpticalFlowPicMat(CeleX5::Full_Optical_Flow_Direction_Pic, 1);	//get Slave's optical flow direction buffer
		cv::imshow("Master-Optical-Flow Direction Buffer - Gray", matOpticalDirectionMaster);
		cv::imshow("Slave-Optical-Flow Direction Buffer - Gray", matOpticalDirectionSlave);

		//Master's optical-flow raw data - display color image
		/*cv::Mat matOpticalRawColor(800, 1280, CV_8UC3);
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
		cv::imshow("Optical-Flow Buffer - Color", matOpticalRawColor);*/
		cvWaitKey(10);
	}
	return 1;
}
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

#define FPN_PATH_M    "../Samples/config/FPN_M_2.txt"
#define FPN_PATH_S    "../Samples/config/FPN_S_2.txt"

#define BIN_FILE_M    "MipiData_20190409_101930758_E_100M_M.bin"	//your Master's bin file path 
#define BIN_FILE_S    "MipiData_20190409_101930758_E_100M_S.bin"	//your Slave's bin file path

int main()
{
	CeleX5 *pCeleX = new CeleX5;

	if (pCeleX == NULL)
		return 0;
	pCeleX->openSensor(CeleX5::CeleX5_MIPI);

	bool success = pCeleX->openBinFile(BIN_FILE_M, 0);	//open the Master's bin file
	bool success1 = pCeleX->openBinFile(BIN_FILE_S, 1);	//open the Salve's bin file

	pCeleX->setFpnFile(FPN_PATH_M, 0);
	pCeleX->setFpnFile(FPN_PATH_S, 1);

	CeleX5::CeleX5Mode sensorMode_M = (CeleX5::CeleX5Mode)pCeleX->getBinFileAttributes(0).loopA_mode;
	CeleX5::CeleX5Mode sensorMode_S = (CeleX5::CeleX5Mode)pCeleX->getBinFileAttributes(1).loopA_mode;

	cout << "SensorMode_M: " << int(sensorMode_M) << " ,SensorMode_S: " << int(sensorMode_S) << endl;
	if (success && success1)
	{
		while (true)
		{
			pCeleX->readBinFileData(0);	//start reading the bin file
			pCeleX->readBinFileData(1);	//start reading the bin file

			if (sensorMode_M == CeleX5::Full_Picture_Mode)	//if the bin file is fullpic mode
			{
				if (!pCeleX->getFullPicMat(0).empty())
				{
					cv::Mat fullPicMatMaster = pCeleX->getFullPicMat(0);
					cv::imshow("Master-FullPic", fullPicMatMaster);
				}
			}
			else if (sensorMode_M == CeleX5::Event_Address_Only_Mode)		//if the bin file is event mode
			{
				if (!pCeleX->getEventPicMat(CeleX5::EventBinaryPic, 0).empty())
				{
					cv::Mat eventPicMatMaster = pCeleX->getEventPicMat(CeleX5::EventBinaryPic, 0);
					cv::imshow("Master-Event-EventBinaryPic", eventPicMatMaster);
				}
			}
			if (sensorMode_S == CeleX5::Full_Picture_Mode)	//if the bin file is fullpic mode
			{
				if (!pCeleX->getFullPicMat(1).empty())
				{
					cv::Mat fullPicMatSlave = pCeleX->getFullPicMat(1);
					cv::imshow("Slave-FullPic", fullPicMatSlave);
				}
			}
			else if (sensorMode_S == CeleX5::Event_Address_Only_Mode)		//if the bin file is event mode
			{
				if (!pCeleX->getEventPicMat(CeleX5::EventBinaryPic, 1).empty())
				{
					cv::Mat eventPicMatSlave = pCeleX->getEventPicMat(CeleX5::EventBinaryPic, 1);
					cv::imshow("Slave-Event-EventBinaryPic", eventPicMatSlave);
				}
			}
			cv::waitKey(10);
		}
	}
	return 1;
}

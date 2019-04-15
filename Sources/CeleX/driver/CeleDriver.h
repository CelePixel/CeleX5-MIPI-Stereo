/*
* Copyright (c) 2017-2018  CelePixel Technology Co. Ltd.  All rights reserved.
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

#ifndef CELEPIXEL_H
#define CELEPIXEL_H

#include <stdint.h>
#include <vector>
#include <ctime>
#include <string>

#define    MAX_DEVICE_COUNT   2

typedef void (*libcelex_transfer_cb_fn)(uint8_t *buffer, int length);

using namespace std;

typedef struct IMU_Raw_Data
{
	uint8_t       imu_data[20];
	std::time_t   time_stamp;
} IMU_Raw_Data;

class Cypress;
#ifdef __linux__
class CeleDriver
#else
	#ifdef FRONTPANEL_EXPORTS
		#define CELEPIXEL_DLL_API __declspec(dllexport)
	#else
		#define CELEPIXEL_DLL_API __declspec(dllimport)
	#endif	
class CELEPIXEL_DLL_API CeleDriver
#endif
{
public:
	CeleDriver(void);
    ~CeleDriver(void);

public:
    bool Open(int device_index = 0);
	//
	bool openUSB(int device_index = 0); //added by xiaoqin @2018.11.02
	bool openStream(int device_index = 0); //added by xiaoqin @2018.11.02

    void Close(int device_index = 0);
	//
	void closeUSB(int device_index = 0); //added by xiaoqin @2018.11.07
	void closeStream(int device_index = 0); //added by xiaoqin @2018.11.07

	bool writeSerialNumber(std::string number, int device_index = 0); //added by xiaoqin @2018.12.06
	std::string getSerialNumber(int device_index = 0); //added by xiaoqin @2018.12.06
	std::string getFirmwareVersion(int device_index = 0); //added by xiaoqin @2018.12.06
	std::string getFirmwareDate(int device_index = 0); //added by xiaoqin @2018.12.06
	bool writeAttribute(std::string attribute, int device_index = 0); //added by xiaoqin @2019.04.03 for distinguishing the sensor is master or slave

	//if the sensor is marked as master, returns "M" 
	//if the sensor is marked as slave, returns "S"
	//if the sensor is not marked as master or salve, returns "M"
	std::string getAttribute(int device_index = 0);

	bool getIMUData( uint8_t* buffer, long& time_stamp1, long& time_stamp2, int device_index = 0); //added by xiaoqin @2019.01.14

    bool getimage(vector<uint8_t> &image, int device_index = 0);
	bool getSensorData(vector<uint8_t> &image, std::time_t& time_stamp_end, vector<IMU_Raw_Data>& imu_data, int device_index = 0); //added by xiaoqin @2019.01.24
	
	void clearData(int device_index = 0);

	int  getDeviceCount(void); //added by xiaoqin @2019.01.28

	//exchange the order of master and salve, in order to ensure the m_pCypress[0] is master and the the m_pCypress[1] is slave
	void exchangeDevices(); //added by xiaoqin @2019.04.09

public:
    bool i2c_set(uint16_t reg, uint16_t value, int device_index = 0);
    bool i2c_get(uint16_t reg, uint16_t &value, int device_index = 0);
    bool mipi_set(uint16_t reg, uint16_t value, int device_index = 0);
    bool mipi_get(uint16_t reg, uint16_t &value, int device_index = 0);

private:
	Cypress*    m_pCypress[MAX_DEVICE_COUNT];
};

#endif // CELEPIXEL_H

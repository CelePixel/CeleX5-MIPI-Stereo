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

#ifndef CELEX5_H
#define CELEX5_H

#include <stdint.h>
#include <vector>
#include <map>
#include <list>
#include <fstream>
#include <opencv2/opencv.hpp>
#include "../celextypes.h"

#ifdef _WIN32
#ifdef CELEX_API_EXPORTS
#define CELEX_EXPORTS __declspec(dllexport)
#else
#define CELEX_EXPORTS __declspec(dllimport)
#endif
#else
#if defined(CELEX_LIBRARY)
#define CELEX_EXPORTS
#else
#define CELEX_EXPORTS
#endif
#endif

using namespace std;

class FrontPanel;
class CeleDriver;
class HHSequenceMgr;
class DataProcessThreadEx;
class DataReaderThread;
class CX5SensorDataServer;
class CommandBase;
class DataRecorder;
class CELEX_EXPORTS CeleX5
{
public:
	enum DeviceType {
		Unknown_Devive = 0,
		CeleX5_MIPI = 1,
		CeleX5_OpalKelly = 2,
	};

	enum CeleX5Mode {
		Unknown_Mode = -1,
		Event_Address_Only_Mode = 0,
		Event_Optical_Flow_Mode = 1,
		Event_Intensity_Mode = 2,
		Full_Picture_Mode = 3,
		Full_Optical_Flow_S_Mode = 4,
		Full_Optical_Flow_M_Mode = 6,
	};

	enum emEventPicType {
		EventBinaryPic = 0,
		EventAccumulatedPic = 1,
		EventGrayPic = 2,
		EventCountPic = 3,
		EventDenoisedBinaryPic = 4,
		EventSuperimposedPic = 5,
		EventDenoisedCountPic = 6
	};

	enum emFullPicType {
		Full_Optical_Flow_Pic = 0,
		Full_Optical_Flow_Speed_Pic = 1,
		Full_Optical_Flow_Direction_Pic = 2
	};

	enum SensorAttribute {
		Master_Sensor = 0,
		Slave_Sensor = 1,
	};

	typedef struct CfgInfo
	{
		std::string name;
		uint32_t    min;
		uint32_t    max;
		uint32_t    value;
		uint32_t    step;
		int16_t     high_addr;
		int16_t     middle_addr;
		int16_t     low_addr;
	} CfgInfo;

	typedef struct BinFileAttributes
	{
		uint8_t    data_type; //bit0: 0: fixed mode; 1: loop mode; bit1: 0: no IMU data; 1: has IMU data
		uint8_t    loopA_mode;
		uint8_t    loopB_mode;
		uint8_t    loopC_mode;
		uint8_t    event_data_format;
		uint8_t    hour;
		uint8_t    minute;
		uint8_t    second;
		uint32_t   package_count;
	} BinFileAttributes;

	CeleX5();
	~CeleX5();
	
	/* 
	* All of the following interfaces are suitable for both a sensor and multiple sensors.
	*
	* When there is only one sensor, the parameter "device_index" can be ignored or set to 0.
	*
	* When there are multiple devices connected, you need to specify the value of parameter "device_index":
	*   device_index = 0: indicates the master sensor or the first connected sensor
	*   device_index = 1: indicates the slave sensor or the second connected sensor
	*   device_index = -1: indicates all connected sensors, that is to say, multiple sensors are controlled at the same time
	*/

	bool openSensor(DeviceType type = CeleX5_MIPI);
	bool isSensorReady(int device_index = 0);

	void getMIPIData(vector<uint8_t> &buffer, std::time_t& time_stamp_end, vector<IMURawData>& imu_data, int device_index = 0);

	CX5SensorDataServer* getSensorDataServer(int device_index = 0);

	DeviceType getDeviceType();

	/***************** Version Information Interfaces *****************/

	/* Get the serial number of the sensor, and each sensor has a unique serial number.
	* Only Support for controlling a single sensor individually.
	*/
	std::string getSerialNumber(int device_index = 0);

	/* Get the firmware version of the sensor.
	* Only Support for controlling a single sensor individually.
	*/
	std::string getFirmwareVersion(int device_index = 0);

	/* Get the release date of firmware.
	 * Only Support for controlling a single sensor individually.
	 */
	std::string getFirmwareDate(int device_index = 0);

	/* Set fpn file to be used in Full_Picture_Mode or Event_Intensity_Mode.
	* Only Support for controlling a single sensor individually.
	*/
	bool setFpnFile(const std::string& fpnFile, int device_index = 0);

	/* Generate fpn file
	* Only Support for controlling a single sensor individually.
	*/
	void generateFPN(std::string fpnFile, int device_index = 0);

	/* Set and get sensor mode (fixed mode)
	* Only Support for controlling a single sensor individually.
	*/
	void setSensorFixedMode(CeleX5Mode mode, int device_index = 0);
	CeleX5Mode getSensorFixedMode(int device_index = 0);

	/* Get the frame time of full-frame picture mode
	*/
	uint32_t getFullPicFrameTime(int device_index = 0);

	/* Set and get event frame time
	* Support for controlling multiple sensors at the same time, and also for controlling a single sensor individually.
	*/
	void setEventFrameTime(uint32_t value, int device_index = -1); //unit: microsecond
	uint32_t getEventFrameTime(int device_index = 0);

	//--- full-frame optical flow mode ---
	void setOpticalFlowFrameTime(uint32_t value); //hardware parameter
	uint32_t getOpticalFlowFrameTime();

	/* Set and get event show method
	* Support for controlling multiple sensors at the same time, and also for controlling a single sensor individually.
	*/
	void setEventShowMethod(EventShowType type, int value, int device_index = -1);
	EventShowType getEventShowMethod(int device_index = 0);

	/* Set and get rotate type
	* Support for controlling multiple sensors at the same time, and also for controlling a single sensor individually.
	*/
	void setRotateType(int type, int device_index = -1);
	int getRotateType(int device_index = 0);

	/* Set and get event count stop
	* Only Support for controlling a single sensor individually.
	*/
	void setEventCountStepSize(uint32_t size, int device_index = 0);
	uint32_t getEventCountStepSize(int device_index = 0);

	uint32_t getFullFrameFPS();

	/*
	* bit7:0~99, bit6:101~199, bit5:200~299, bit4:300~399, bit3:400~499, bit2:500~599, bit1:600~699, bit0:700~799
	* if rowMask = 240 = b'11110000, 0~399 rows will be closed.
	* Support for controlling multiple sensors at the same time, and also for controlling a single sensor individually.
	*/
	void setRowDisabled(uint8_t rowMask, int device_index = -1);

	/*
	* Whether to display the images when recording
	* Support for controlling multiple sensors at the same time, and also for controlling a single sensor individually. 
	*/
	void setShowImagesEnabled(bool enable, int device_index = -1);

	/***************** Sensor Control Interfaces *****************/
	/* Set and get threshold
	* Only Support for controlling a single sensor individually.
	*/
	void setThreshold(uint32_t value, int device_index = 0);
	uint32_t getThreshold(int device_index = 0);

	/* Set and get brightness
	* Only Support for controlling a single sensor individually.
	*/
	void setBrightness(uint32_t value, int device_index = 0);
	uint32_t getBrightness(int device_index = 0);

	/* Set and get clock rate
	* Only Support for controlling a single sensor individually.
	*/
	uint32_t getClockRate(int device_index = 0); //unit: MHz
	void setClockRate(uint32_t value, int device_index = 0); //unit: MHz

	/* Set and get ISO level
	* Only Support for controlling a single sensor individually.
	*/
	void setISOLevel(uint32_t value, int device_index = 0);
	uint32_t getISOLevel(int device_index = 0);

	/* Set and get sensor attribute
	* Only Support for controlling a single sensor individually.
	*/
	void setSensorAttribute(SensorAttribute attribute, int device_index = 0);
	SensorAttribute getSensorAttribute(int device_index = 0);

	/* Set and get event data format
	 * Only Support for controlling a single sensor individually.
	 */
	void setEventDataFormat(int format, int device_index = 0); //0: format 0; 1: format 1; 2: format 2
	int getEventDataFormat(int device_index = 0);

	void reset(int device_index = 0);
	void pauseSensor();
	void restartSensor();

	void setAntiFlashlightEnabled(bool enabled, int device_index = 0);
	void setAutoISPEnabled(bool enable, int device_index = 0);
	bool isAutoISPEnabled(int device_index = 0);
	void setISPThreshold(uint32_t value, int num, int device_index = 0);
	void setISPBrightness(uint32_t value, int num, int device_index = 0);

	/***************** Get Image Interfaces *****************/
	void getFullPicBuffer(unsigned char* buffer, int device_index = 0);
	cv::Mat getFullPicMat(int device_index = 0);

	void getEventPicBuffer(unsigned char* buffer, emEventPicType type = EventBinaryPic, int device_index = 0);
	cv::Mat getEventPicMat(emEventPicType type, int device_index = 0);

	void getOpticalFlowPicBuffer(unsigned char* buffer, emFullPicType type = Full_Optical_Flow_Pic, int device_index = 0);
	cv::Mat getOpticalFlowPicMat(emFullPicType type, int device_index = 0);

	/***************** Get Event Data Vector *****************/
	bool getEventDataVector(std::vector<EventData> &vector, int device_index = 0);
	bool getEventDataVector(std::vector<EventData> &vector, uint64_t& frameNo, int device_index = 0);

	/***************** Get IMU Data *****************/
	int getIMUData(std::vector<IMUData>& data, int device_index = 0);

	/***************** Record Raw Data Interfaces *****************/
	void startRecording(std::string filePath, int device_index = 0);
	void stopRecording(int device_index = -1);

	/***************** Playback Interfaces *****************/
	bool openBinFile(std::string filePath, int device_index = 0);
	bool readBinFileData(int device_index = 0);
	BinFileAttributes getBinFileAttributes(int device_index = 0);

	uint32_t getTotalPackageCount();
	uint32_t getCurrentPackageNo();
	void setCurrentPackageNo(uint32_t value);
	void replay();
	void play();
	void pause();
	PlaybackState getPlaybackState();
	void setPlaybackState(PlaybackState state);
	void setIsPlayBack(bool state);

	//------------- cfgs ------------
	map<string, vector<CfgInfo>> getCeleX5Cfg();
	map<string, vector<CfgInfo>> getCeleX5CfgModified();
	void writeRegister(int16_t addressH, int16_t addressM, int16_t addressL, uint32_t value, int device_index = 0);
	CfgInfo getCfgInfoByName(string csrType, string name, bool bDefault);
	void writeCSRDefaults(string csrType, int device_index = 0);
	void modifyCSRParameter(string csrType, string cmdName, uint32_t value);

private:
	bool configureSettings(DeviceType type, int device_index = 0);
	//for write register
	void wireIn(uint32_t address, uint32_t value, uint32_t mask, int device_index = 0);
	void writeRegister(CfgInfo cfgInfo, int device_index = 0);
	void setALSEnabled(bool enable, int device_index = 0);
	//
	void enterCFGMode(int device_index = 0);
	void enterStartMode(int device_index = 0);
	void disableMIPI(int device_index = 0);
	void enableMIPI(int device_index = 0);
	void clearData(int device_index = 0);

private:
	CeleDriver*                    m_pCeleDriver;

	HHSequenceMgr*                 m_pSequenceMgr;
	DataProcessThreadEx*           m_pDataProcessThread[MAX_SENSOR_NUM];
	DataRecorder*                  m_pDataRecorder[MAX_SENSOR_NUM];
	//
	map<string, vector<CfgInfo>>   m_mapCfgDefaults;
	map<string, vector<CfgInfo>>   m_mapCfgModified;
	//
	unsigned char*                 m_pReadBuffer;
	uint8_t*                       m_pDataToRead;
	std::ifstream                  m_ifstreamPlayback[MAX_SENSOR_NUM]; //playback
	//
	uint32_t                       m_uiBrightness[MAX_SENSOR_NUM];
	uint32_t                       m_uiThreshold[MAX_SENSOR_NUM];
	uint32_t                       m_uiClockRate[MAX_SENSOR_NUM];
	uint32_t                       m_uiLastClockRate[MAX_SENSOR_NUM]; // for test
	int                            m_iEventDataFormat[MAX_SENSOR_NUM];
	uint32_t                       m_uiPackageCount[MAX_SENSOR_NUM];

	uint32_t                       m_uiTotalPackageCount[MAX_SENSOR_NUM];
	vector<uint64_t>               m_vecPackagePos;
	bool                           m_bFirstReadFinished;
	BinFileAttributes              m_stBinFileHeader[MAX_SENSOR_NUM];
	DeviceType                     m_emDeviceType;
	//
	uint32_t                       m_uiPackageCounter[MAX_SENSOR_NUM];
	uint32_t                       m_uiPackageCountPS[MAX_SENSOR_NUM];
	uint32_t                       m_uiPackageTDiff[MAX_SENSOR_NUM];
	uint32_t                       m_uiPackageBeginT[MAX_SENSOR_NUM];
	//
	bool                           m_bAutoISPEnabled[MAX_SENSOR_NUM];
	uint32_t                       m_arrayISPThreshold[3];
	uint32_t                       m_arrayBrightness[4];
	uint32_t                       m_uiAutoISPRefreshTime;
	uint32_t                       m_uiISOLevel[MAX_SENSOR_NUM]; //range: 1 ~ 6

	bool                           m_bHasIMUData;
	bool                           m_bClockAutoChanged;
	uint32_t                       m_uiISOLevelCount; //4 or 6

	string						   m_strFirmwareVer[MAX_SENSOR_NUM];
	uint32_t                       m_uiOpticalFlowFrameTime;
};

#endif // CELEX5_H

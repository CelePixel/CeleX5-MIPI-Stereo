#include <string.h>
#include <stdio.h>
#include "CeleDriver.h"
#include "Cypress.h"

CeleDriver::CeleDriver(void)
{
	for (int i = 0; i < MAX_DEVICE_COUNT; i++)
	{
		m_pCypress[i] = new Cypress;
	}
}

CeleDriver::~CeleDriver(void)
{
	for (int i = 0; i < MAX_DEVICE_COUNT; i++)
	{
		delete m_pCypress[i];
		m_pCypress[i] = NULL;
	}
}

bool CeleDriver::Open(int device_index)
{
    return m_pCypress[device_index]->StreamOn();
}

bool CeleDriver::openUSB(int device_index)
{
	return m_pCypress[device_index]->open_usb(device_index);
}

bool CeleDriver::openStream(int device_index)
{
	return m_pCypress[device_index]->open_stream();
}

void CeleDriver::Close(int device_index)
{
	m_pCypress[device_index]->StreamOff();
}

void CeleDriver::closeUSB(int device_index)
{
	m_pCypress[device_index]->close_usb();
}

void CeleDriver::closeStream(int device_index)
{
	m_pCypress[device_index]->close_stream();
}

bool CeleDriver::i2c_set( uint16_t reg, uint16_t value, int device_index)
{
	return m_pCypress[device_index]->USB_Set(CTRL_I2C_SET_REG, reg, value);
}

bool CeleDriver::i2c_get( uint16_t reg, uint16_t &value, int device_index)
{
	return m_pCypress[device_index]->USB_Get(CTRL_I2C_GET_REG, reg, value);
}

bool CeleDriver::mipi_set(uint16_t reg,uint16_t value, int device_index)
{
	return m_pCypress[device_index]->USB_Set(CTRL_MIPI_SET_REG, reg, value);
}

bool CeleDriver::mipi_get(uint16_t reg,uint16_t &value, int device_index)
{
	return m_pCypress[device_index]->USB_Get(CTRL_MIPI_GET_REG, reg, value);
}

bool CeleDriver::writeSerialNumber(std::string number, int device_index)
{
	return m_pCypress[device_index]->writeSerialNumber(number);
}

std::string CeleDriver::getSerialNumber(int device_index)
{
	return m_pCypress[device_index]->getSerialNumber();
}

std::string CeleDriver::getFirmwareVersion(int device_index)
{
	return m_pCypress[device_index]->getFirmwareVersion();
}

std::string CeleDriver::getFirmwareDate(int device_index)
{
	return m_pCypress[device_index]->getFirmwareDate();
}

//if attribute = "M", means master sensor, then "*M*" will be written after the device serial number
//if attribute = "S", means slave sensor, then "*S*" will be written after the device serial number
bool CeleDriver::writeAttribute(std::string attribute, int device_index)
{
	std::string serial_num = getSerialNumber(device_index);
	//
	std::string new_attribute = "*";
	new_attribute += attribute;
	new_attribute += "*";
	//
	string::size_type pos = serial_num.find("*S*");
	if (pos != std::string::npos)
	{
		serial_num.replace(pos, 3, new_attribute);
	}
	else
	{
		pos = serial_num.find("*M*");
		if (pos != std::string::npos)
			serial_num.replace(pos, 3, new_attribute);
		else
			serial_num += new_attribute;
	}
	writeSerialNumber(serial_num, device_index);
	return true;
}

//if the sensor is marked as master, returns "M" 
//if the sensor is marked as slave, returns "S"
//if the sensor is not marked as master or salve, returns "M"
std::string CeleDriver::getAttribute(int device_index)
{
	std::string serial_num = getSerialNumber(device_index);
	if (serial_num.find("*S*") != std::string::npos)
		return std::string("S");
	else
		return std::string("M");
}

bool CeleDriver::getIMUData(uint8_t* buffer, long& time_stamp, long& time_stamp2, int device_index)
{
	return m_pCypress[device_index]->get_imu_data(buffer, time_stamp, time_stamp2);
}

bool CeleDriver::getimage(vector<uint8_t> &image, int device_index)
{
	return GetPicture(image, device_index);
}

bool CeleDriver::getSensorData(std::vector<uint8_t> &image, std::time_t& time_stamp_end, std::vector<IMU_Raw_Data>& imu_data, int device_index)
{
	return GetPicture(image, time_stamp_end, imu_data, device_index);
}

void CeleDriver::clearData(int device_index)
{
	ClearData();
}

int CeleDriver::getDeviceCount(void)
{
	return m_pCypress[0]->get_device_count();
}

void CeleDriver::exchangeDevices()
{
	device_handle_m = m_pCypress[1]->getDeviceHandle();

	Cypress* pCyTmp = m_pCypress[1];
	m_pCypress[1] = m_pCypress[0];
	m_pCypress[0] = pCyTmp;
}

void CeleDriver::pauseSaveSensorData()
{
	PauseSaveSensorData();
}

void CeleDriver::restartSaveSensorData()
{
	RestartSaveSensorData();
}

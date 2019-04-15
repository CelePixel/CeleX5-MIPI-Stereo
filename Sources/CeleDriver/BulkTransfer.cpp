#include <stdio.h>
#include "BulkTransfer.h"
#ifdef __linux__
#include <semaphore.h>
#include <string.h>
#endif // __linux__

#include <iostream>
#include <chrono>

#define MAX_IMAGE_BUFFER_NUMBER 5

CPackage  image_list_m[MAX_IMAGE_BUFFER_NUMBER];
CPackage* current_package_m = nullptr;
bool      bRunning_m = false;
uint8_t   write_frame_index_m = 0;
uint8_t   current_write_frame_index_m = 0;
uint8_t   read_frame_index_m = 0;
uint32_t  package_size_m = 0;
uint32_t  package_count_m = 0;

CPackage  image_list_s[MAX_IMAGE_BUFFER_NUMBER];
CPackage* current_package_s = nullptr;
bool      bRunning_s = false;
uint8_t   write_frame_index_s = 0;
uint8_t   current_write_frame_index_s = 0;
uint8_t   read_frame_index_s = 0;
uint32_t  package_size_s = 0;
uint32_t  package_count_s = 0;

clock_t clock_begin = 0;
clock_t clock_end = 0;
bool    g_has_imu_data = true;

#ifdef __linux__
    static sem_t     m_sem;
#else
	static HANDLE m_hEventHandle = nullptr;
#endif // __linux__

bool submit_bulk_transfer(libusb_transfer *xfr)
{
    if (xfr)
    {
        if(libusb_submit_transfer(xfr) == LIBUSB_SUCCESS)
        {
            return true;
            // Error
        }
        libusb_free_transfer(xfr);
    }
    return false;
}

std::time_t getTimeStamp() 
{
	std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
	std::time_t timestamp = tmp.count();
	//std::time_t timestamp = std::chrono::system_clock::to_time_t(tp);
	//cout << "     ---- time stamp =       " << timestamp << endl;
	return timestamp;
}

void generate_image(uint8_t *buffer, int length)
{
	if (current_package_m == nullptr)
	{
		current_package_m = &image_list_m[write_frame_index_m];
		current_write_frame_index_m = write_frame_index_m;

		write_frame_index_m++;
		if (write_frame_index_m >= MAX_IMAGE_BUFFER_NUMBER)
			write_frame_index_m = 0;
		//if (current_package_m->Size() > 0)
		//{
		//	//printf("---------- szie = %d\n", current_package_m->Size());
		//	printf("read_frame_index_m = %d, current_write_frame_index_m = %d\n", read_frame_index_m, current_write_frame_index_m);
		//	printf("---------- package_count_m = %d\n", package_count_m);
		//}
		if (package_count_m >= MAX_IMAGE_BUFFER_NUMBER)
		{
			//printf("-------------------- generate_image: buffer is full! --------------------\n");
			//printf("------- current_package_m size = %d\n", current_package_m->Size());
			//printf("read_frame_index_m = %d, current_write_frame_index_m = %d\n", read_frame_index_m, current_write_frame_index_m);
			read_frame_index_m++;
			if (read_frame_index_m >= MAX_IMAGE_BUFFER_NUMBER)
				read_frame_index_m = 0;

			current_package_m->ClearData();
			package_count_m--;
		}
	}
	if (current_package_m)
		current_package_m->Insert(buffer + buffer[0], length - buffer[0]);
	if (g_has_imu_data && buffer[7] == 1)
	{
		IMU_Raw_Data imu_data;
		memcpy(imu_data.imu_data, buffer + 7, 21);
		imu_data.time_stamp = getTimeStamp();
		current_package_m->m_vecIMUData.push_back(imu_data);
	}
	//
	if (buffer[1] & 0x02)
	{
		current_package_m->m_lTime_Stamp_End = getTimeStamp();
		//cout << "------------ image time stamp = " << current_package->m_lTime_Stamp_End << endl;
		if (current_package_m)
		{
			current_package_m->Insert(buffer + 6, 1);
			current_package_m->End();
			current_package_m = nullptr;

			package_count_m++;

//#ifdef __linux__
//			sem_post(&m_sem);
//#else
//			SetEvent(m_hEventHandle);
//#endif // __linux__
		}
	}
}

void generate_image1(uint8_t *buffer, int length)
{
	if (current_package_s == nullptr)
	{
		current_package_s = &image_list_s[write_frame_index_s];
		current_write_frame_index_s = write_frame_index_s;
		write_frame_index_s++;
		if (write_frame_index_s >= MAX_IMAGE_BUFFER_NUMBER)
			write_frame_index_s = 0;

		if (package_count_s >= MAX_IMAGE_BUFFER_NUMBER)
		{
			//printf("-------------------- generate_image: buffer is full! --------------------\n");
			//printf("------- current_package_m size = %d\n", current_package_m->Size());
			//printf("read_frame_index_m = %d, current_write_frame_index_m = %d\n", read_frame_index_m, current_write_frame_index_m);			
			read_frame_index_s++;
			if (read_frame_index_s >= MAX_IMAGE_BUFFER_NUMBER)
				read_frame_index_s = 0;

			current_package_s->ClearData();
			package_count_s--;
		}
	}
	if (current_package_s)
		current_package_s->Insert(buffer + buffer[0], length - buffer[0]);
	if (g_has_imu_data && buffer[7] == 1)
	{
		IMU_Raw_Data imu_data;
		memcpy(imu_data.imu_data, buffer + 7, 21);
		imu_data.time_stamp = getTimeStamp();
		current_package_s->m_vecIMUData.push_back(imu_data);
	}
	//
	if (buffer[1] & 0x02)
	{
		current_package_s->m_lTime_Stamp_End = getTimeStamp();
		//cout << "------------ image time stamp = " << current_package->m_lTime_Stamp_End << endl;
		if (current_package_s)
		{
			current_package_s->Insert(buffer + 6, 1);
			current_package_s->End();
			current_package_s = nullptr;

			package_count_s++;
			//#ifdef __linux__
			//			sem_post(&m_sem);
			//#else
			//			SetEvent(m_hEventHandle);
			//#endif // __linux__
		}
	}
}

bool GetPicture(vector<uint8_t> &Image, int device_index)
{
	if (bRunning_m == true)
	{
//#ifdef __linux__
//		int Ret = sem_wait(&m_sem);
//#else
//		DWORD Ret = WaitForSingleObject(m_hEventHandle, INFINITE);
//#endif // __linux__
		//if (Ret == 0)
		{
			if (0 == device_index)
			{
				if (package_count_m > 0)
				{
					image_list_m[read_frame_index_m].GetImage(Image);
					read_frame_index_m++;
					package_count_m--;
					if (read_frame_index_m >= MAX_IMAGE_BUFFER_NUMBER)
						read_frame_index_m = 0;
					if (Image.size())
						return true;
				}
			}
			else
			{
				if (package_count_s > 0)
				{
					image_list_s[read_frame_index_s].GetImage(Image);
					read_frame_index_s++;
					package_count_s--;
					if (read_frame_index_s >= MAX_IMAGE_BUFFER_NUMBER)
						read_frame_index_s = 0;
					if (Image.size())
						return true;
				}
			}
		}
	}
	return false;
}

bool GetPicture(std::vector<uint8_t> &Image, std::time_t& time_stamp_end, std::vector<IMU_Raw_Data>& imu_data, int device_index)
{
	if (bRunning_m == true)
	{
//#ifdef __linux__
//		int Ret = sem_wait(&m_sem);
//#else
//		DWORD Ret = WaitForSingleObject(m_hEventHandle, INFINITE);
//#endif // __linux__
//if (Ret == 0)
		if (0 == device_index)
		{
			if (package_count_m > 0)
			{
				if (read_frame_index_m != current_write_frame_index_m)
				{
					image_list_m[read_frame_index_m].GetImage(Image);
					time_stamp_end = image_list_m[read_frame_index_m].m_lTime_Stamp_End;
					imu_data = image_list_m[read_frame_index_m].m_vecIMUData;
					image_list_m[read_frame_index_m].m_vecIMUData.clear();
					//printf("------------- read_frame_index = %d:-------------\n", read_frame_index);
					read_frame_index_m++;
					if (read_frame_index_m >= MAX_IMAGE_BUFFER_NUMBER)
						read_frame_index_m = 0;
					package_count_m--;
					if (Image.size())
					{
						return true;
					}
				}
				else
				{
					//printf("------------- package_count_m = %d:-------------\n", package_count_m);
				}
			}
		}
		else
		{
			if (package_count_s > 0)
			{
				if (read_frame_index_s != current_write_frame_index_s)
				{
					image_list_s[read_frame_index_s].GetImage(Image);
					time_stamp_end = image_list_s[read_frame_index_s].m_lTime_Stamp_End;
					imu_data = image_list_s[read_frame_index_s].m_vecIMUData;
					image_list_s[read_frame_index_s].m_vecIMUData.clear();
					//printf("------------- read_frame_index = %d:-------------\n", read_frame_index);
					read_frame_index_s++;
					if (read_frame_index_s >= MAX_IMAGE_BUFFER_NUMBER)
						read_frame_index_s = 0;
					package_count_s--;
					if (Image.size())
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

void ClearData()
{
	for (int i = 0; i < MAX_IMAGE_BUFFER_NUMBER; i++)
	{
		image_list_m[i].ClearData();
		image_list_s[i].ClearData();
	}
	package_count_m = 0;
	write_frame_index_m = 0;
	read_frame_index_m = 0;

	package_count_s = 0;
	write_frame_index_s = 0;
	read_frame_index_s = 0;
}

void callbackUSBTransferComplete(libusb_transfer *xfr)
{
    switch (xfr->status)
    {
        case LIBUSB_TRANSFER_COMPLETED:
            // Success here, data transfered are inside
            // xfr->buffer
            // and the length is xfr->actual_length
			// printf("xfr->actual_length= %d\r\n",xfr->actual_length);
			if (xfr->dev_handle == device_handle_m)
			{
				generate_image(xfr->buffer, xfr->actual_length);
			}
			else
			{
				generate_image1(xfr->buffer, xfr->actual_length);
			}
            submit_bulk_transfer(xfr);
            break;

        case LIBUSB_TRANSFER_TIMED_OUT:
			printf("LIBUSB_TRANSFER_TIMED_OUT\r\n");
            break;

        case LIBUSB_TRANSFER_CANCELLED:
			printf("LIBUSB_TRANSFER_CANCELLED\r\n");
			break;

        case LIBUSB_TRANSFER_NO_DEVICE:
			printf("LIBUSB_TRANSFER_NO_DEVICE\r\n");
			break;

        case LIBUSB_TRANSFER_ERROR:
			printf("LIBUSB_TRANSFER_ERROR\r\n");
			break;

        case LIBUSB_TRANSFER_STALL:
			printf("LIBUSB_TRANSFER_STALL\r\n");
			break;

        case LIBUSB_TRANSFER_OVERFLOW:
			printf("LIBUSB_TRANSFER_OVERFLOW\r\n");
			break;
    }
}

libusb_transfer *alloc_bulk_transfer(libusb_device_handle *device_handle, uint8_t address, uint8_t *buffer)
{
    if (device_handle)
    {
        libusb_transfer *xfr = libusb_alloc_transfer(0);
        if (xfr)
        {
            libusb_fill_bulk_transfer(xfr,
                          device_handle,
                          address, // Endpoint ID
                          buffer,
                          MAX_ELEMENT_BUFFER_SIZE,
                          callbackUSBTransferComplete,
                          nullptr,
                          0);
            if (submit_bulk_transfer(xfr) == true)
                return xfr;
        }
    }
    return nullptr;
}

bool init_bulk_transfer(void)
{
#ifdef __linux__
    if ( sem_init(&m_sem,0,0) == 0 )
    {
#else
    m_hEventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_hEventHandle)
    {
#endif
        bRunning_m = true;
        return true;
    }
    return false;
}

void exit_bulk_transfer(void)
{
    bRunning_m = false;
#ifdef __linux__
    sem_destroy(&m_sem);
#else
    if(m_hEventHandle)
    {
		CloseHandle(m_hEventHandle);
		m_hEventHandle = nullptr;
    }
#endif
}

void cancel_bulk_transfer(libusb_transfer *xfr)
{
    if (xfr)
    {
        libusb_cancel_transfer(xfr);
    }
}
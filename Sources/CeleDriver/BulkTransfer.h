#ifndef __BULK_TRANSFER_H__
#define __BULK_TRANSFER_H__

#include "libusb.h"
#include "Package.h"

extern libusb_device_handle *device_handle_m;

bool init_bulk_transfer(void);
void exit_bulk_transfer(void);

static bool submit_bulk_transfer(libusb_transfer *xfr);
void cancel_bulk_transfer(libusb_transfer *xfr);
	
libusb_transfer *alloc_bulk_transfer(libusb_device_handle *device_handle, uint8_t address, uint8_t *buffer);

bool GetPicture(std::vector<uint8_t> &Image, int device_index);
bool GetPicture(std::vector<uint8_t> &Image, std::time_t& time_stamp_end, std::vector<IMU_Raw_Data>& imu_data, int device_index);
void ClearData();

void generate_image(uint8_t *buffer, int length);
void generate_image1(uint8_t *buffer, int length);


#endif // __BULK_TRANSFER_H__

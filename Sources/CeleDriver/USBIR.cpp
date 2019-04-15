#include "USBIR.h"
#include <stdio.h>
#include <string.h>

#define USB_TIMEOUT     3000  //传输数据的时间延迟

libusb_device_handle *device_handle_m = nullptr;

USBIR::USBIR()
{
    device_handle = nullptr;
	clock_begin = 0;
	clock_end = 0;
}

USBIR::~USBIR()
{

}

bool USBIR::usb_check_device(libusb_device *dev, int usb_vid, int usb_pid, int trans_mode)
{
    libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) == LIBUSB_SUCCESS)
    {
        if ((desc.idVendor == usb_vid) && ( desc.idProduct == usb_pid))
        {
            for (int i = 0; i < desc.bNumConfigurations; i++)
            {
                struct libusb_config_descriptor *config;
                if (libusb_get_config_descriptor(dev, i, &config) == LIBUSB_SUCCESS)
                {
                    bConfigurationValue = config->bConfigurationValue;
                    for (int j = 0; j < config->bNumInterfaces; j++)
                    {
                        for ( int k = 0; k < config->interface[j].num_altsetting; k++)
                        {
                            if (config->interface[j].altsetting[k].bInterfaceClass == LIBUSB_CLASS_VIDEO)
                            {
                                InterfaceNumberList.push_back(config->interface[j].altsetting[k].bInterfaceNumber);
                                for (int l = 0; l < config->interface[j].altsetting[k].bNumEndpoints; l++)
                                {
                                    if ((config->interface[j].altsetting[k].endpoint[l].bmAttributes & 0x03 ) == trans_mode) //找到端点
                                    {
                                        video_endpoint_address = config->interface[j].altsetting[k].endpoint[l].bEndpointAddress;
                                        libusb_free_config_descriptor(config);
                                        return true;
                                    }
                                }
                            }
                        }
                    }
                    libusb_free_config_descriptor(config);
                }
            }
        }
    }
    return false;
}

bool USBIR::usb_GetInterface(int usb_vid, int usb_pid, int trans_mode)
{
    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(nullptr, &devs);

    for (ssize_t i = 0; i < cnt; i++)
    {
        if( usb_check_device(devs[i], usb_vid,usb_pid ,trans_mode) == true )
        {
            libusb_free_device_list(devs, 1);
            return true;
        }
    }
    if (cnt >= 0)
    {
        libusb_free_device_list(devs, 1);
    }
    return false;
}

//bool USBIR::usb_open(int vid, int pid,int trans_mode)
//{
//    if( libusb_init(nullptr) == LIBUSB_SUCCESS )
//    {
//        InterfaceNumberList.clear();
//        if (  usb_GetInterface(vid,pid,trans_mode) == true )
//        {
//            device_handle = libusb_open_device_with_vid_pid(nullptr, vid, pid);
//            if ( device_handle )
//            {
//                 /* 进行设备的初始化
//                 1.设置当前的设备使用的configuration,参数2是要使用配置描述符中的bConfigurationValue */
//                libusb_set_configuration(device_handle,bConfigurationValue);
//                size_t i = 0;
//                for( ;i < InterfaceNumberList.size();i++ )
//                {
//                    int Ret = libusb_detach_kernel_driver(device_handle,InterfaceNumberList[i]);
//                    if ( (Ret == LIBUSB_SUCCESS) || ( Ret == LIBUSB_ERROR_NOT_SUPPORTED ) || (Ret == LIBUSB_ERROR_NOT_FOUND ) )
//                    {
//                        if( libusb_claim_interface(device_handle, InterfaceNumberList[i]) != LIBUSB_SUCCESS )
//                        {
//                            break;
//                        }
//                    }
//                }
//                if ( i < InterfaceNumberList.size() )
//                {
//                    for( size_t j = 0; j < i; j++ )
//                    {
//                        libusb_release_interface(device_handle, InterfaceNumberList[j]);
//                        libusb_attach_kernel_driver(device_handle, InterfaceNumberList[j]);
//                    }
//
//                } else
//                {
//                    video_trans_mode = trans_mode;
//                    return true;
//                }
//                libusb_close(device_handle);
//            }
//        }
//        libusb_exit(nullptr);
//    }
//    return false;
//}

bool USBIR::usb_open(int vid, int pid, int trans_mode, int device_index)
{
	if (libusb_init(&usb_context) == LIBUSB_SUCCESS)
	{
		usb_device_index = device_index;
		InterfaceNumberList.clear();

		//------ get usb device list ------
		libusb_device **devs;
		ssize_t cnt = libusb_get_device_list(usb_context, &devs);
		int usb_device_index = -1;
		for (ssize_t i = 0; i < cnt; i++)
		{
			if (usb_check_device(devs[i], vid, pid, trans_mode) == true)
			{
				usb_device_index++;
				if (usb_device_index != device_index)
					continue;
				//libusb_open_device_with_vid_pid change to libusb_open
				libusb_open(devs[i], &device_handle);
				libusb_free_device_list(devs, 1);

				if (device_handle)
				{
					if (0 == device_index)
						device_handle_m = device_handle;

					/* 进行设备的初始化
					1.设置当前的设备使用的configuration,参数2是要使用配置描述符中的bConfigurationValue */
					libusb_set_configuration(device_handle, bConfigurationValue);
					size_t i = 0;
					for (; i < InterfaceNumberList.size(); i++)
					{
						int Ret = libusb_detach_kernel_driver(device_handle, InterfaceNumberList[i]);
						if ((Ret == LIBUSB_SUCCESS) || (Ret == LIBUSB_ERROR_NOT_SUPPORTED) || (Ret == LIBUSB_ERROR_NOT_FOUND))
						{
							if (libusb_claim_interface(device_handle, InterfaceNumberList[i]) != LIBUSB_SUCCESS)
							{
								break;
							}
						}
					}
					if (i < InterfaceNumberList.size())
					{
						for (size_t j = 0; j < i; j++)
						{
							libusb_release_interface(device_handle, InterfaceNumberList[j]);
							libusb_attach_kernel_driver(device_handle, InterfaceNumberList[j]);
						}
					}
					else
					{
						video_trans_mode = trans_mode;
						return true;
					}
					libusb_close(device_handle);
				}
			}
		}
		if (cnt >= 0)
		{
			libusb_free_device_list(devs, 1);
		}
		libusb_exit(usb_context);
	}
	return false;
}

void USBIR::usb_close(void)
{
    if (device_handle)
    {
        for (size_t i = 0; i < InterfaceNumberList.size(); i++)
        {
            libusb_release_interface(device_handle, InterfaceNumberList[i]);
            libusb_attach_kernel_driver(device_handle, InterfaceNumberList[i]);
        }
        libusb_close(device_handle);
        libusb_exit(NULL);
    }
}

bool USBIR::usb_control(uint8_t request_type,uint8_t request, uint16_t wValue, uint16_t wIndex,uint8_t *buffer,uint16_t wLen)
{
    if (device_handle)
    {
        int ret = libusb_control_transfer(device_handle, request_type, request, wValue, wIndex, buffer,wLen, USB_TIMEOUT);
        if ( ret == wLen )
        {
            return true;
        }
        printf("Write return value = %d\r\n",ret);
    }
    return false;
}

bool USBIR::usb_alloc_bulk_transfer(void)
{
    int i = 0;
    for (i = 0; i < MAX_URB_NUMBER; i++)
    {
        bulk_transfer[i] = alloc_bulk_transfer(device_handle, video_endpoint_address, bulk_buffer[i]);
        if (bulk_transfer[i] == nullptr)
        {
            break;
        }
    }
    if (i > 0)
        return true;
    return false;
}

bool USBIR::Start(void)
{
    if (video_start() == true)
    {
        if (init_bulk_transfer() == true)
        {
            if (usb_alloc_bulk_transfer() == true)
            {
                return true;
            }
			exit_bulk_transfer();
        }
        video_stop();
    }
    return false;
}

void USBIR::Stop(void)
{
    for (int i = 0; i < MAX_URB_NUMBER; i++)
    {
		cancel_bulk_transfer(bulk_transfer[i]);
    }
    video_stop();
	exit_bulk_transfer();
}

bool USBIR::Run(void)
{
	if (libusb_handle_events(usb_context) != LIBUSB_SUCCESS)
	{
		return false;
	}
    return true;
}

bool USBIR::get_imu_data(uint8_t* buffer, long& time_stamp1, long& time_stamp2)
{
	uint16_t address = 0x01C1;
	*(uint16_t*)buffer = address;
	if (usb_control(0x21, 0x01, 0x600, 0x300, buffer, 4))
	{
		if (usb_control(0xA1, 0x81, 0x600, 0x300, buffer, 26))
		{
			time_stamp1 = 0;
			time_stamp2 = clock();
			return true;
		}
	}
	return false;
}

libusb_device_handle* USBIR::getDeviceHandle()
{
	return device_handle;
}

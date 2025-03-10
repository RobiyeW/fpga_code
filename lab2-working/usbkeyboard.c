#include "usbkeyboard.h"
#include <stdio.h>
#include <stdlib.h>

struct libusb_device_handle *openkeyboard(uint8_t *endpoint_address)
{
    libusb_device **devs;
    struct libusb_device_handle *keyboard = NULL;
    struct libusb_device_descriptor desc;
    ssize_t num_devs, d;
    uint8_t i, k;

    if (libusb_init(NULL) < 0)
    {
        fprintf(stderr, "Error: libusb_init failed\n");
        exit(1);
    }

    if ((num_devs = libusb_get_device_list(NULL, &devs)) < 0)
    {
        fprintf(stderr, "Error: libusb_get_device_list failed\n");
        exit(1);
    }

    for (d = 0; d < num_devs; d++)
    {
        libusb_device *dev = devs[d];
        if (libusb_get_device_descriptor(dev, &desc) < 0)
        {
            fprintf(stderr, "Error: libusb_get_device_descriptor failed\n");
            exit(1);
        }

        if (desc.bDeviceClass == LIBUSB_CLASS_PER_INTERFACE)
        {
            struct libusb_config_descriptor *config;
            libusb_get_config_descriptor(dev, 0, &config);
            for (i = 0; i < config->bNumInterfaces; i++)
                for (k = 0; k < config->interface[i].num_altsetting; k++)
                {
                    const struct libusb_interface_descriptor *inter =
                        config->interface[i].altsetting + k;
                    if (inter->bInterfaceClass == LIBUSB_CLASS_HID &&
                        inter->bInterfaceProtocol == USB_HID_KEYBOARD_PROTOCOL)
                    {
                        int r;
                        if ((r = libusb_open(dev, &keyboard)) != 0)
                        {
                            fprintf(stderr, "Error: libusb_open failed: %d\n", r);
                            exit(1);
                        }
                        if (libusb_kernel_driver_active(keyboard, i))
                            libusb_detach_kernel_driver(keyboard, i);
                        libusb_set_auto_detach_kernel_driver(keyboard, i);
                        if ((r = libusb_claim_interface(keyboard, i)) != 0)
                        {
                            fprintf(stderr, "Error: libusb_claim_interface failed: %d\n", r);
                            exit(1);
                        }
                        *endpoint_address = inter->endpoint[0].bEndpointAddress;
                        goto found;
                    }
                }
        }
    }

found:
    libusb_free_device_list(devs, 1);
    return keyboard;
}
from .base import *
# https://pypi.org/project/libusb1/
import usb1

class Controller_libusb(Controller):
    def __init__(self, context = None):
        super().__init__()
        
        self.owns_context = False
        if context:
            self.context = context
        else:
            self.context = usb1.USBContext()
            self.owns_context = True
        
        self.handle = self.context.openByVendorIDAndProductID(
            VENDOR_ID,
            PRODUCT_ID,
            #skip_on_error=True,
        )
        
        if self.handle is None:
            raise usb1.USBErrorNotFound('Device not found or wrong permissions')
        
        if usb1.hasCapability(usb1.CAP_SUPPORTS_DETACH_KERNEL_DRIVER):
            self.handle.setAutoDetachKernelDriver(True) # needed? probably in use by "hid"
        
        self.handle.claimInterface(0)
        
        self.sendPacket(makePacket(0xCC, 0x60, 0x00))
        
        self.led_order0 = LED(2, 1, 0) #TODO
        self.led_order1 = LED(2, 1, 0)
        buff = self.handle.controlRead(0xA1, 0x01, 0x03CC, 0x0000, 64)
        if len(buff) == 64:
            self.report = IT8297_Report.from_buffer(buff)
            print("Product:", self.report.str_product.decode('utf-8'))
            self.led_order0.r = (self.report.byteorder0 >> 16) & 0xFF
            self.led_order0.g = (self.report.byteorder0 >> 8) & 0xFF
            self.led_order0.b = self.report.byteorder0 & 0xFF
            if self.report.chip_id == 0x82970100: # BX version
                self.led_order1.r = (self.report.byteorder1 >> 16) & 0xFF
                self.led_order1.g = (self.report.byteorder1 >> 8) & 0xFF
                self.led_order1.b = self.report.byteorder1 & 0xFF
            else: # AX version
                self.led_order0.r = (self.report.byteorder0 >> 16) & 0xFF
                self.led_order0.g = (self.report.byteorder0 >> 8) & 0xFF
                self.led_order0.b = self.report.byteorder0 & 0xFF
        self._startup()
    
    def __del__(self):
        if self.owns_context:
            self.context.close()
    
    def sendPacket(self, data):
        if not isinstance(data, bytearray):
            data = bytearray(data)
        return self.handle.controlWrite(0x21, 0x09, 0x03CC, 0x0000, data)
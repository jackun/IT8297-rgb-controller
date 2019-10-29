from .base import *
# https://pypi.org/project/hidapi/
import hid

class Controller_hidapi(Controller):
    def __init__(self, context = None):
        super().__init__()
        
        self.device = None
        device_list = hid.enumerate(VENDOR_ID, PRODUCT_ID)
        if len(device_list) == 0:
            raise Exception("No devices found")
        
        self.device = hid.device()
        # TODO device selecting
        self.device.open_path(device_list[0]["path"])
        
        print("Manufacturer: %s" % self.device.get_manufacturer_string())
        print("Product: %s" % self.device.get_product_string())
        print("Serial No: %s" % self.device.get_serial_number_string())
        
        self.sendPacket(makePacket(0xCC, 0x60, 0x00))
        self.led_order0 = LED(2, 1, 0) #TODO
        self.led_order1 = LED(2, 1, 0)
        buff = self.device.get_feature_report(0xCC, 64)
        if len(buff) == 64:
            self.report = IT8297_Report.from_buffer(bytearray(buff))
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
        if self.device:
            self.device.close()
    
    def sendPacket(self, data):
        if not isinstance(data, bytearray):
            data = bytearray(data)
        return self.device.write(data)
import sys
import struct
import itertools
from ctypes import Structure, c_uint, c_ushort, c_uint8, c_char

#LITTLE_ENDIAN = sys.byteorder == 'little'

VENDOR_ID  = 0x048D
PRODUCT_ID = 0x8297

# LED "headers" 0x20..0x27, As seen on Gigabyte X570 Elite board
HDR_IO_PORTS = 0x20
HDR_AUD_CAPS = 0x23
HDR_D_LED1 = 0x25
HDR_D_LED2 = 0x26
HDR_D_LED1_RGB = 0x58
HDR_D_LED2_RGB = 0x59

EFFECT_NONE = 0
EFFECT_STATIC = 1
EFFECT_PULSE = 2
EFFECT_FLASH = 3
EFFECT_COLORCYCLE = 4
EFFECT_COLORCYCLE_TIMED_FADE = 5 #??? period2
EFFECT_COLORCYCLE_SPLASHING = 6 # period0 color cycle time, 
# to be continued...

# LEDCount
LEDS_32 = 0
LEDS_64 = 1
LEDS_256 = 2
LEDS_512 = 3
LEDS_1024 = 4
LEDS_MAX_PER_PKT = 19

LEDS_ORDER_RGB = 0
LEDS_ORDER_GRB = 1
LEDS_ORDER_BGR = 2

def makePacket(*args):
    pkt = bytearray(b'\0' * 64)
    for i, v in enumerate(args):
        pkt[i] = v
    return pkt

def makeColor(r, g, b):
    return (r<<16) | (g<<8) | b

class LED(Structure):
    _pack_ = 1
    _fields_ = [("g", c_uint8), ("r", c_uint8), ("b", c_uint8)]
    def __str__(self):
        return "<{}({}, {}, {})>".format(self.__class__.__name__, self.r, self.g, self.b)

def get_rgb(leds):
    return list(itertools.chain.from_iterable([x.r, x.g, x.b] for x in leds))

def get_grb(leds):
    return list(itertools.chain.from_iterable([x.g, x.r, x.b] for x in leds))

def get_bgr(leds):
    return list(itertools.chain.from_iterable([x.b, x.g, x.r] for x in leds))

class PktRGB(Structure):
    _pack_ = 1
    _fields_ = [("report_id", c_uint8),
        ("header", c_uint8),
        ("boffset", c_ushort),
        ("bcount", c_uint8),
        ("leds", LED * LEDS_MAX_PER_PKT),
        ("padding0", c_ushort)
        ]
    
    def __init__(self, hdr = HDR_D_LED1_RGB):
        pass
    
    def setup(self, hdr, offset = 0, count = 0, leds = []):
        self.report_id = 0xCC
        self.header = hdr; # 0x58 - lower header, 0x59 - upper header;
        self.boffset = offset * 3 # in bytes, absolute
        self.bcount = count * 3
        self.leds = (LED * LEDS_MAX_PER_PKT)(*leds) #FIXME copy non-length-matching array
        self.padding0 = 0
    
    def get_bytes(self, order = LEDS_ORDER_RGB):
        if order == LEDS_ORDER_RGB:
            leds = get_rgb(self.leds)
        elif order == LEDS_ORDER_GRB:
            leds = get_grb(self.leds)
        elif order == LEDS_ORDER_BGR:
            leds = get_rgb(self.leds)
        
        return struct.pack("<BBHB%sB%sx" % (self.bcount, 2 + (57-self.bcount),),
            self.report_id, self.header, self.boffset, self.bcount,
            *leds)

class PktEffect(Structure):
    _pack_ = 1
    _fields_ = [
        ("report_id", c_uint8),  
        ("header", c_uint8),
        ("zone0", c_uint), # rgb fusion seems to set it to pow(2, header - 0x20)
        ("zone1", c_uint),
        ("reserved0", c_uint8),
        ("effect_type", c_uint8),
        ("max_brightness", c_uint8),
        ("min_brightness", c_uint8),
        ("color0", c_uint),
        ("color1", c_uint),
        ("period0", c_ushort), # fade in
        ("period1", c_ushort), # fade out
        ("period2", c_ushort), # hold
        ("period3", c_ushort), # ???
        ("param0", c_uint8), # colorcycle - how many colors to cycle through (how are they set?)
                                    # flash - if >0 cycle through N colors
        ("param1", c_uint8),
        ("param2", c_uint8), # idk, flash effect repeat count
        ("param3", c_uint8),
        ("padding0", c_uint8 * 30)
    ]

    def setup(self, hdr = HDR_D_LED1, effect = EFFECT_STATIC, color0 = 0x00ff2100):
        if hdr < 0x20:
            raise Exception("LED header port index is below 0x20. Fix me if it is correct.")
        self.report_id = 0xCC # uint8_t = 0xCC;
        self.header = hdr # uint8_t  = 0x20;
        self.zone0 = 2**(hdr - 0x20) # uint32_t - rgb fusion seems to set it to pow(2, header - 0x20)
        self.zone1 = 0 # uint32_t
        self.reserved0 = 0 # uint8_t
        self.effect_type = effect # uint8_t
        self.max_brightness = 100 # uint8_t
        self.min_brightness = 0 # uint8_t
        self.color0 = color0 # uint32_t, endianness?
        self.color1 = 0x00000000 # uint32_t
        self.period0 = 540 # uint16_t - fade in, 1/100th of second?
        self.period1 = 100 # uint16_t - fade out
        self.period2 = 440 # uint16_t - hold
        self.period3 = 0 # uint16_t - ???
        self.param0 = 1 # uint8_t - colorcycle - how many colors to cycle through (how are they set?)
                            # flash - if >0 cycle through N colors
        self.param1 = 1 # uint8_t - ???
        self.param2 = 0 # uint8_t - idk, flash effect repeat count
        self.param3 = 1 # uint8_t - idk
        #uint8_t padding0[30];

    def get_bytes(self):
        return struct.pack("<BBIIBBBBIIHHHHBBBB30x", 
            self.report_id, self.header, self.zone0, self.zone1,
            self.reserved0, self.effect_type,
            self.max_brightness, self.min_brightness,
            self.color0, self.color1,
            self.period0, self.period1, self.period2, self.period3,
            self.param0, self.param1,
            self.param2, self.param3)

class IT8297_Report(Structure):
    _pack_ = 1
    _fields_ = [
        ("report_id", c_uint8),
        ("product", c_uint8),
        ("device_num", c_uint8),
        ("total_leds", c_uint8), # current setLedCount()?
        ("fw_ver", c_uint),
        ("ctrlLength", c_ushort), # ???
        ("reserved0", c_ushort),
        ("str_product", c_char * 32), # might be 28, and extra byteorder3
        ("byteorder0", c_uint),
        ("byteorder1", c_uint),
        ("byteorder2", c_uint),
        ("chip_id", c_uint),
        ("reserved1", c_uint)
    ]

class Controller:
    
    def __init__(self):
        self.report = None
        self.led_count = 0

    def _startup(self):
        self.setLedCount()
        self.enableBeat(False)
        self.disableEffect(False)
        self.setAllPorts(EFFECT_PULSE, 0x00FF2100)
    
    def disableEffect(self, b):
        """
        Disable built-in effect. Need for running custom RGB effects.
        """
        return self.sendPacket(makePacket(0xCC, 0x32, 1 if b else 0))
    
    def enableBeat(self, b):
        return self.sendPacket(makePacket(0xCC, 0x31, 1 if b else 0))
    
    def applyEffect(self, strip = 0xFF):
        return self.sendPacket(makePacket(0xCC, 0x28, strip))

    # FIXME suspect
    def saveStateToMCU(self):
        return self.sendPacket(makePacket(0xCC, 0x5E)) == 64
    
    def setAllPorts(self, effect = EFFECT_STATIC, color = 0x00FF2100):
        pkt = PktEffect()
        for hdr in range(0x20, 0x28):
            pkt.setup(hdr, effect, color)
            pkt.param0 = 7
            pkt.param1 = 1
            self.sendPacket(pkt)
        return self.applyEffect()
    
    def stopAll(self):
        for i in range(0, 8):
            self.sendPacket(makePacket(0xCC, 0x20 + i))
        self.applyEffect()
        self.enableBeat(False)
        self.disableEffect(False)
        self.sendPacket(makePacket(0xCC, 0x20, 0xFF))
        self.applyEffect()
    
    def setLedCount(self, s0 = None, s1 = None):
        """
        Set the count of maximum individually addressable leds in a block.
        Blocks are repeated, probably up to 1024 leds:
        0 = 32 leds
        1 = 64
        2 = 256
        3 = 512
        4 = 1024
        """
        if self.report and self.report.chip_id != 0x82970100: #AX, maybe
            if s0:
                self.led_count = (self.report.ctrlLength & 0xF0) | s0
            elif s1:
                self.led_count = (self.report.ctrlLength & 0x0F) | (s1 << 4)
        else:
            if s0:
                self.led_count = (self.led_count & 0xF0) | s0
            if s1:
                self.led_count = (self.led_count & 0x0F) | (s1 << 4)
        return self.sendPacket(makePacket(0xCC, 0x34, self.led_count))
        
    def sendRGB(self, led_data, hdr = HDR_D_LED1_RGB):
        pkt = PktRGB()
        sent = 0
        left = len(led_data)
        while left > 0:
            count = min(LEDS_MAX_PER_PKT, left)
            pkt.setup(hdr, sent, count, led_data[sent:sent+count])
            sent += count
            left -= count
            self.sendPacket(pkt)


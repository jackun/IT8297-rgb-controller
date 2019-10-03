import it8297 as it
import time
import sys

c = it.Controller()

# end it
if sys.argv[-1] == "-s":
    c.stopAll()
    sys.exit(0)

c.setLedCount(it.LEDS_32)

c.setAllPorts(color=0)
time.sleep(.5) # allow effect to fade to black

c.disableEffect(True)

print("\"off-by-one\" RGB effect :P")
leds = (it.LED * 32)()
l = len(leds)
colors = [[128, 0, 0], [128, 128, 0], [0, 128, 0], [0, 128, 128], [0, 0, 128], [128, 0, 128], ]

for y in range(0, l*31):
    col = colors[ int(y/(l)) % len(colors) ]
    off0 = y % l
    off1 = (y + (l-1) - (y//32)%l) % l
    leds[off0].r = col[0]
    leds[off0].g = col[1]
    leds[off0].b = col[2]
    leds[off1].r = 0x00
    leds[off1].g = 0x00
    leds[off1].b = 0x00
    
    c.sendRGB(leds)
    c.sendRGB(leds, HDR_D_LED2_RGB)
    time.sleep(0.01)
c.disableEffect(False)

pkt = it.PktEffect()
print("pulse")
for i in range(0, 8):
    pkt.setup(0x20 + i, it.EFFECT_PULSE, it.makeColor(128, 0, 0))
    pkt.period0 = 1200
    pkt.period1 = 1200
    pkt.period2 = 200
    pkt.effect_param0 = 7
    c.sendPacket(pkt)
c.applyEffect()
time.sleep(10)

print("save MCU:", c.saveStateToMCU())

print("flash")
pkt.setup(it.HDR_D_LED1, it.EFFECT_FLASH)
pkt.period0 = 100
pkt.period1 = 100
pkt.period2 = 1000
pkt.effect_param0 = 7
c.sendPacket(pkt)
c.applyEffect()
time.sleep(10)

print("transition between statics")
for i in range(0, 8):
    pkt.setup(0x20 + i, it.EFFECT_STATIC, 0x21FF00)
    pkt.period0 = 1000
    c.sendPacket(pkt)
    c.applyEffect()
time.sleep(3)
for i in range(0, 8):
    pkt.setup(0x20 + i, it.EFFECT_STATIC, 0x002064)
    pkt.period0 = 1000
    c.sendPacket(pkt)
    c.applyEffect()
time.sleep(3)
for i in range(0, 8):
    pkt.setup(0x20 + i, it.EFFECT_STATIC, 0xFF2100)
    pkt.period0 = 1000
    c.sendPacket(pkt)
    c.applyEffect()
time.sleep(2)

# go crazy
#print("sent:", c.setAllPorts(6, 0x00FF00FF))
#c.applyEffect(0x60)

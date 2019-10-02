import it8297 as it
import time
import sys

c = it.Controller()
c.setLedCount(it.LEDS_256)

c.setAllPorts(color=0)
time.sleep(.5) # allow effect to fade to black
c.disableEffect(True)

pkt = it.PktRGB()
for i in range(0, 7):
    pkt.setup(it.HDR_D_LED1_RGB, i * it.LEDS_MAX_PER_PKT, \
        it.LEDS_MAX_PER_PKT, [it.LED(0, 200, 255)] * it.LEDS_MAX_PER_PKT)
    c.sendPacket(pkt)
time.sleep(3)
c.disableEffect(False)

# start to pulse
pkt = it.PktEffect()
pkt.setup(it.HDR_D_LED1, it.EFFECT_PULSE)
pkt.effect_param0 = 7
c.sendPacket(pkt)
c.startEffect()

time.sleep(3)

# go crazy
#print("sent:", c.setAllPorts(6, 0x00FF00FF))

# end it
if sys.argv[-1] == "-s":
    c.disableEffect(False)
    c.setAllPorts(color=0)
    time.sleep(1)
    c.disableEffect(True)

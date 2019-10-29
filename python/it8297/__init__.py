from .base import *

# TODO probably a better way doing optional imports :P
try:
    from .controller_libusb import Controller_libusb
except ModuleNotFoundError as e:
    print(e)

try:
    from .controller_hidapi import Controller_hidapi
except ModuleNotFoundError as e:
    print(e)
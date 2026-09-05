# This work is licensed under the MIT license.
# Copyright (c) 2013-2023 OpenMV LLC. All rights reserved.
# https://github.com/openmv/openmv/blob/master/LICENSE
#
# Global Shutter Manual Trigger Example
#
# In triggered mode snapshot() normally pulses the FSYNC pin itself, which both starts the
# exposure and then blocks until the frame has been read out. This example drives FSYNC from
# Python instead, so triggering and reading out are decoupled:
#
#   - fsync(True)/fsync(False) starts the exposure exactly when you want it,
#   - readable() reports when the frame has landed in the frame buffer,
#   - snapshot(fsync=False) picks the frame up without re-triggering the sensor and
#     without blocking, leaving the time in between free for your own code.

import csi
import time

csi0 = csi.CSI()
csi0.reset()  # Reset and initialize the sensor.
csi0.pixformat(csi.GRAYSCALE)  # Set pixel format to GRAYSCALE
csi0.framesize(csi.VGA)  # Set frame size to VGA (640x480)
# Triple buffering is required. snapshot() does not release the frame it returned until
# the next call, so with fewer buffers the driver cannot capture the next triggered frame
# while your code is still holding the current one, and readable() never goes true.
csi0.framebuffers(3)
csi0.snapshot(time=2000)  # Wait for settings take effect.
clock = time.clock()  # Create a clock object to track the FPS.

csi0.ioctl(csi.IOCTL_SET_TRIGGERED_MODE, True)

# Arm the capture hardware without triggering the sensor. This returns immediately,
# so the first trigger below is not missed.
csi0.snapshot(blocking=False, fsync=False)

while True:
    clock.tick()  # Update the FPS clock.

    # Trigger the sensor. Integration starts on the rising edge.
    csi0.fsync(True)
    csi0.fsync(False)

    # Your own code can run here while the sensor exposes and reads out.
    while not csi0.readable():
        pass

    # Pick up the frame. Does not trigger the sensor again and does not block.
    img = csi0.snapshot(fsync=False)

    print(clock.fps())  # Note: OpenMV Cam runs about half as fast when connected
    # to the IDE. The FPS should increase once disconnected.

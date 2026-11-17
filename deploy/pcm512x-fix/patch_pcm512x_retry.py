#!/usr/bin/env python3
"""Adds a probe retry loop around the PCM512x_RESET writes.

Some PCM5122 HATs (e.g. InnoMaker DAC Mini HiHat) hold/stretch the I2C bus
while their onboard LDOs settle after power-on, and the bcm2835 I2C
controller cannot handle clock stretching (documented errata). The stock
driver probes once and fails; this patch retries the reset for ~60s.
Run AFTER patch_pcm512x.py.
"""
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "pcm512x.c"
src = open(path).read()

old = """	/* Reset the device, verifying I/O in the process for I2C */
	ret = regmap_write(regmap, PCM512x_RESET,
			   PCM512x_RSTM | PCM512x_RSTR);
	if (ret != 0) {
		dev_err(dev, "Failed to reset device: %d\\n", ret);
		goto err;
	}

	ret = regmap_write(regmap, PCM512x_RESET, 0);
	if (ret != 0) {
		dev_err(dev, "Failed to reset device: %d\\n", ret);
		goto err;
	}
"""
new = """	/* Reset the device, verifying I/O in the process for I2C.
	 * Some PCM5122 HATs (e.g. InnoMaker) hold/stretch the I2C bus while
	 * their onboard LDOs settle after power-on, and the bcm2835 I2C
	 * controller cannot handle clock stretching. Retry the reset for up
	 * to ~60s before giving up. */
	{
		int attempt;

		for (attempt = 0; attempt < 30; attempt++) {
			ret = regmap_write(regmap, PCM512x_RESET,
					   PCM512x_RSTM | PCM512x_RSTR);
			if (ret == 0)
				ret = regmap_write(regmap, PCM512x_RESET, 0);
			if (ret == 0)
				break;
			if (attempt == 0)
				dev_warn(dev,
					 "codec not ready (bus stretch?), retrying reset\\n");
			msleep(2000);
		}
		if (ret != 0) {
			dev_err(dev,
				"Failed to reset device after %d attempts: %d\\n",
				attempt + 1, ret);
			goto err;
		}
	}
"""
assert old in src, "reset block not found"
src = src.replace(old, new)
open(path, "w").write(src)
print("RETRY PATCH OK")

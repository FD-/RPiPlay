#!/usr/bin/env python3
"""Patch pcm512x.c: tolerate runtime-PM resume failures.

The RPi 6.18 kernel returns -EINVAL from the codec's runtime resume path
(regcache_sync / regulator enable). The stock driver propagates that error
into the device's runtime-PM state, after which EVERY open fails with -22.

HATs like the InnoMaker DAC Mini HiHat wire AVDD/DVDD/CPVDD straight to the
Pi's 3V3 rail, so a failed framework-level enable is harmless - the chip is
powered regardless. This patch makes pcm512x_resume log-and-continue instead
of failing; snd_soc_dai hw_params then configures all registers properly.
"""
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "pcm512x.c"
src = open(path).read()

# 1) regulator_bulk_enable in pcm512x_resume: log-and-continue
old_reg = """	ret = regulator_bulk_enable(ARRAY_SIZE(pcm512x->supplies),
				    pcm512x->supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to enable supplies: %d\\n", ret);
		return ret;
	}
"""
new_reg = """	ret = regulator_bulk_enable(ARRAY_SIZE(pcm512x->supplies),
				    pcm512x->supplies);
	if (ret != 0) {
		dev_err(dev, "regulator_bulk_enable failed: %d (HAT rails are hardwired, continuing)\\n", ret);
	}
"""
assert old_reg in src, "regulator block not found"
src = src.replace(old_reg, new_reg)

# 2) regcache_sync in pcm512x_resume: log-and-continue
old_sync = """	regcache_cache_only(pcm512x->regmap, false);
	ret = regcache_sync(pcm512x->regmap);
	if (ret != 0) {
		dev_err(dev, "Failed to sync cache: %d\\n", ret);
		return ret;
	}
"""
new_sync = """	regcache_cache_only(pcm512x->regmap, false);
	ret = regcache_sync(pcm512x->regmap);
	if (ret != 0) {
		dev_err(dev, "regcache_sync failed: %d (continuing - hw_params will reconfigure)\\n", ret);
	}
"""
assert old_sync in src, "sync block not found"
src = src.replace(old_sync, new_sync)

open(path, "w").write(src)
print("PATCHED OK - both resume failure paths now log-and-continue")

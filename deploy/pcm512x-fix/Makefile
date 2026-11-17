obj-m += snd-soc-pcm512x.o
snd-soc-pcm512x-objs := pcm512x.o

# CURDIR (not PWD!) is correct with make -C: PWD is inherited from the
# invoking shell and points at wherever provision.sh was started.
all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(CURDIR) clean

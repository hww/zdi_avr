OBJS=args.o os.o file.o load.o tiva.o msp430.o stm32.o z180.o ez80.o lpc17xx.o
BINS=
TARGET=MCUload
TOOL=
CPU=

# linking/building ELF
$(TARGET): $(OBJS) $(BINS)
#	sed "s/^/echo /;s/\([0-9]*\)$$/\`expr \1 + 1\`>build/" build | bash
#	echo "const char *build=\"`d2u<build`\",*build_date=__DATE__,*build_time=__TIME__;" | arm-elf-gcc -c -o build.o -x c -
	$(TOOL)gcc -Wl,-Map=$(TARGET).map,--cref $(OBJS) -lusb -s -o $(TARGET)

# compiling C-source code
-include $(OBJS:.o=.d)
%.o: %.c
	$(TOOL)gcc -O1 -g0 -c -fno-diagnostics-show-caret -Wall $< -o $@
	$(TOOL)gcc -MM -c -Wall $< > $*.d

version.h: version
	echo "#define SW_VERSION \"`d2u<version`\"" > version.h
	sed "s/$$/.0.0.0/;s/\(\([^\.]*\.\)\{3\}\).*/\10/" version >build

clean:
#	del $(subst /,\,$(OBJS)) $(subst /,\,$(OBJS:.o=.d)) $(TARGET).elf $(TARGET).map $(TARGET).bin $(TARGET).hex $(TARGET).lst
#	rm $(OBJS) $(OBJS:.o=.d) $(TARGET) $(TARGET).map
	rm $(OBJS) $(OBJS:.o=.d) $(TARGET).map

release: ver

ver: clean
	rm -f version.h
	$(MAKE) VER=_`cat version` rar

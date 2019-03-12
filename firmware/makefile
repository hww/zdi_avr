OBJS=main.o zdi.o disasm.o
TARGET=zdi
MCU=atmega328

# coverting HEX & BIN
$(TARGET).hex: $(TARGET).elf
	avr-objcopy -O ihex $< $@
	avr-objdump -t -x -d -M reg-names-std -S $< >$(<:.elf=.lst)
ifeq ($(OS),Windows_NT)
	avr-size --mcu=$(MCU) -C $< #| p:/Pristroje/AVR/bt-100/util/avr-size-parse.exe -C
else
	avr-size --mcu=$(MCU) -C $< #| ~/Projekty/AVR/util/avr-size-parse -C
endif

# linking/building ELF
$(TARGET).elf: $(OBJS)
# prepare build.o
ifeq ($(OS),Windows_NT)
	read B<build;\
	read V<version;\
	echo "BUILD info: $$V.$$B";\
	echo -e "#include <avr/pgmspace.h>\nconst char build[] PROGMEM=\"$$V.$$B\", build_date[] PROGMEM=__DATE__, build_time[] PROGMEM=__TIME__;" | avr-gcc -Os -g -c -mmcu=$(MCU) -Wall -o build.o -x c -
else
	read B<build;\
	read V<version;\
	echo "BUILD info: $$V.$$B";\
	echo "#include <avr/pgmspace.h>\nconst char build[] PROGMEM=\"$$V.$$B\", build_date[] PROGMEM=__DATE__, build_time[] PROGMEM=__TIME__;" | avr-gcc -Os -g -c -mmcu=$(MCU) -Wall -o build.o -x c -
endif
	avr-gcc -mmcu=$(MCU) -Wl,-Map=$(@:.elf=.map),--cref $^ build.o -o $@
# prepare new build number
ifeq ($(OS),Windows_NT)
	read B<build;\
	B=$$(($$B+1));\
	echo "$$B">build
	rm -f build.o
else
	read B<build;\
	B=$$(($$B+1));\
	echo "$$B">build
	rm -f build.o
endif


# compiling C-source code
-include $(OBJS:.o=.d)
%.o: %.c
	avr-gcc -Os -g -c -mmcu=$(MCU) -Wall $< -o $@
	avr-gcc -MM -c -mmcu=$(MCU) -Wall $< > $*.d

%.o: %.S
	avr-gcc -Os -g -c -mmcu=$(MCU) -Wall $< -o $@
	avr-gcc -MM -c -mmcu=$(MCU) -Wall $< > $*.d

clean:
	rm -f *.o *.d $(TARGET).elf $(TARGET).map $(TARGET).lst $(TARGET).hex

release: ver

ver: clean
	rm -f version.h
	$(MAKE) VER=_`cat version` rar

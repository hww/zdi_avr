#ifndef _EZ80_H_
#define	_EZ80_H_

extern unsigned ez_CS[4], ez_iRAM, ez_iFLASH;

#define	ARGS_EZ80_PARAMS \
  {ARGS_INTEGER, "--CS0", "<LBR><UBR><CTL><BMC>", &ez_CS[0], 1, "set CS0 parameters"}, \
  {ARGS_INTEGER, "--CS1", "<LBR><UBR><CTL><BMC>", &ez_CS[1], 1, "set CS1 parameters"}, \
  {ARGS_INTEGER, "--CS2", "<LBR><UBR><CTL><BMC>", &ez_CS[2], 1, "set CS2 parameters"}, \
  {ARGS_INTEGER, "--CS3", "<LBR><UBR><CTL><BMC>", &ez_CS[3], 1, "set CS3 parameters"}, \
  {ARGS_INTEGER, "--iRAM", "<ADDR_U><CTL>", &ez_iRAM, 1, "set internal RAM parameters"}, \
  {ARGS_INTEGER, "--iFLASH", "<ADDR_U><CTRL>", &ez_iFLASH, 1, "set internal Flash parameters"},

extern int eZ80_load (COM_HANDLE tty, const file_data_type *data);

#endif


#include <windows.h>
#include <string.h>
//#include <setupapi.h> // USB_find

#include "com.h"

// ====================================
// serial variables
// ------------------------------------
//int com_port = 7;
char *com_port = NULL; //"COM7";

unsigned usb_vid = 0x16D5; //0;
unsigned usb_pid = 0x0002; //0;
//char usb_serial[256] = {0}; //"000002";
char *usb_serial = "000002"; //NULL;

// ====================================
// open & set serial port
// ------------------------------------
//COM_HANDLE com_open_port (const int com_port, unsigned long speed, unsigned parity) {
COM_HANDLE com_open_port (const char *com_port, unsigned long speed, unsigned parity) {
char port_name[32];
COM_HANDLE tty;
  //sprintf (port_name, "\\\\.\\COM%d", com_port);
  if (*com_port == '\\')
    tty = CreateFile (com_port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  else {
    sprintf (port_name, "\\\\.\\%s", com_port);
    tty = CreateFile (port_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
  }
  if (!COM_INVALID (tty)) {
    com_speed (tty, speed, parity);
  }
  return tty;
}

// ====================================
// change serial speed
// ------------------------------------
void com_speed (COM_HANDLE tty, unsigned long speed, unsigned parity) {
static COMMTIMEOUTS cto = {0, 0, 0, 0, 0};
DCB dcb;
  // nastaveni serioveho portu
  GetCommState (tty, &dcb);
  //BuildCommDCB ("baud=115200 parity=N data=8 stop=1", &dcb);
  // strukturu si nastavim rucne
  dcb.BaudRate = speed;
  if (parity) {
    dcb.fParity = TRUE;
    if (parity == COM_PAR_ODD)
      dcb.Parity = ODDPARITY;
    else
      dcb.Parity = EVENPARITY;
  } else {
    dcb.fParity = FALSE;
    dcb.Parity = NOPARITY;
  }
  dcb.fBinary = TRUE;
  dcb.fOutxCtsFlow = FALSE;
  dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = DTR_CONTROL_DISABLE;
  dcb.fDsrSensitivity = FALSE;
  //dcb.fTXContinueOnXoff
  dcb.fOutX = FALSE;
  dcb.fInX = FALSE;
  dcb.fErrorChar = FALSE;
  dcb.ErrorChar = 0xFF;
  dcb.fNull = FALSE;
  dcb.fRtsControl = RTS_CONTROL_DISABLE;
  dcb.fAbortOnError = FALSE;
  dcb.ByteSize = 8;
  dcb.StopBits = ONESTOPBIT;
  SetCommState (tty, &dcb);
  SetCommTimeouts (tty, &cto);
  PurgeComm (tty, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
}

// ====================================
// control DTR/RTS signals
// ------------------------------------
unsigned com_set_signal (COM_HANDLE tty, unsigned signal, unsigned delay) {
static unsigned char last_state = 0;
#define	DTR_STATE	0x01
#define	RTS_STATE	0x02
  // prepare signal inversion requests
  if (signal & COM_RTS_INV)
    signal |= (last_state & RTS_STATE) ? COM_RTS_CLR : COM_RTS_SET;
  if (signal & COM_DTR_INV)
    signal |= (last_state & DTR_STATE) ? COM_DTR_CLR : COM_DTR_SET;
  // process RTS control request
  if (signal & COM_RTS_CLR) {
    EscapeCommFunction(tty, CLRRTS);
    last_state &= ~RTS_STATE;
  } else
  if (signal & COM_RTS_SET) {
    EscapeCommFunction(tty, SETRTS);
    last_state |= RTS_STATE;
  }
  // some ports/drivers need DTR control call to do RTS change
  if (!(signal & (COM_DTR_CLR | COM_DTR_SET)))
    signal |= (last_state & DTR_STATE) ? COM_DTR_SET : COM_DTR_CLR;
  // process DTR control request
  if (signal & COM_DTR_CLR) {
    EscapeCommFunction(tty, CLRDTR);
    last_state &= ~DTR_STATE;
  } else
  if (signal & COM_DTR_SET) {
    EscapeCommFunction(tty, SETDTR);
    last_state |= DTR_STATE;
  }
  if (delay)
    os_sleep (delay);
  return signal;
}

// ====================================
// send data from buffer
// ------------------------------------
int com_send_buf (COM_HANDLE tty, const unsigned char *buf, unsigned len) {
DWORD size;
  return WriteFile (tty, buf, len, &size, NULL);
}

// ====================================
// receive byte
// ------------------------------------
int com_rec_byte (COM_HANDLE tty, unsigned timeout) {
COMMTIMEOUTS cto;
DWORD size;
unsigned char data;
unsigned start = os_tick ();

  GetCommTimeouts (tty, &cto);
  cto.ReadTotalTimeoutConstant = 0;
  do {
    // nastaveni aktualniho timeoutu
    cto.ReadIntervalTimeout = cto.ReadTotalTimeoutMultiplier = timeout - (os_tick () - start);
    // obcas to hapruje, protoze rozliseni je 20ms; musim provest korekci
    if (!cto.ReadIntervalTimeout)
      cto.ReadIntervalTimeout = 1;
    if (!timeout) {
      cto.ReadIntervalTimeout = MAXDWORD;
      cto.ReadTotalTimeoutMultiplier = 0;
    }
    SetCommTimeouts (tty, &cto);
    // cekani na znak
    if (ReadFile (tty, &data, 1, &size, NULL) && size)
      return data;
  } while (os_tick () - start < timeout);
  return COM_REC_TIMEOUT;
}

// ====================================
// receive data to buffer
// ------------------------------------
int com_rec_buf (COM_HANDLE tty, unsigned char *buf, unsigned len, unsigned timeout) {
unsigned long start = os_tick ();
unsigned pos = 0;
  do {
    int status;
    status = com_rec_byte (tty, timeout - os_tick () + start);
    if (status >= 0)
      buf[pos++] = status;
  } while (pos < len && (os_tick () - start) < timeout);
  if (!pos) {
    printf ("[%lums]", os_tick () - start);
    return COM_REC_TIMEOUT;
  }
  return pos;
}


// ====================================
// flush serial port
// ------------------------------------
void com_flush (COM_HANDLE tty) {
  PurgeComm (tty, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
}

// ====================================
// close serial port
// ------------------------------------
void com_close (COM_HANDLE tty) {
  CloseHandle (tty);
}

// ====================================
// OS specific functions
// ====================================
// get system tick count [msec]
// ------------------------------------
unsigned long os_tick (void) {
  return GetTickCount ();
}

// ====================================
// sleep for some time [msec]
// ------------------------------------
void os_sleep (unsigned long msec) {
  Sleep (msec);
}

#if 0
// ====================================
// get system time
// ------------------------------------
void os_time (TIME_TYPE *time) {
  GetLocalTime (time);
}
#endif

#if 0
// ====================================
// look for connected USB device
// doesn't show disconnected devices
// returns direct path for opening USB device
// eg. "\\?\usb#vid_16d5&pid_0002#000002#{a5dcbf10-6530-11d2-901f-00c04fb951ed}"
// ------------------------------------
int com_find_usb (char *name, unsigned short vid, unsigned short pid, const char *serial) {
//static void USB_find (char *path, unsigned short vid, unsigned short pid, char *sn) {
static const GUID guid = {0xA5DCBF10, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}}; // see GUID_DEVINTERFACE_USB_DEVICE

HDEVINFO deviceInfoList;
SP_DEVICE_INTERFACE_DATA interfaceInfo;
SP_DEVICE_INTERFACE_DETAIL_DATA *interfaceDetails = NULL;
SP_DEVINFO_DATA deviceInfo;
DWORD size;
int i;
char vidpid[48];
  // prepare device list for connected USB devices with selected GUID
  deviceInfoList = SetupDiGetClassDevs (&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
  deviceInfo.cbSize = sizeof (deviceInfo);
  interfaceInfo.cbSize = sizeof (interfaceInfo);
  if (serial)
    sprintf (vidpid, "vid_%04x&pid_%04x#%s", vid, pid, serial);
  else
    sprintf (vidpid, "vid_%04x&pid_%04x", vid, pid);
  *name = 0;
  for (i = 0; SetupDiEnumDeviceInterfaces (deviceInfoList, NULL, &guid, i, &interfaceInfo); i++) {
    if (!name)
      printf ("%d:\t", i);
    // get device path
    SetupDiGetDeviceInterfaceDetail (deviceInfoList, &interfaceInfo, NULL, 0, &size, NULL);
    interfaceDetails = malloc (size);
    if (interfaceDetails) {
      interfaceDetails->cbSize = sizeof (SP_DEVICE_INTERFACE_DETAIL_DATA);
      SetupDiGetDeviceInterfaceDetail (deviceInfoList, &interfaceInfo, interfaceDetails, size, NULL, NULL);
      if (!name)
	printf ("%s\n", interfaceDetails->DevicePath);
      if (strstr (interfaceDetails->DevicePath, vidpid)) {
	if (name) {
	  strcpy (name, interfaceDetails->DevicePath);
	  free (interfaceDetails);
	  SetupDiDestroyDeviceInfoList (deviceInfoList);
	  return 0;
	}
      }
      free (interfaceDetails);
    }
    if (!name)
      putchar ('\n');
  }
  SetupDiDestroyDeviceInfoList (deviceInfoList);
  return -1;
}

#else

// ====================================
// look for COM port number in registry
// correct/valid USB serial number needed!!
// ------------------------------------
int com_find_usb (char *name, unsigned short vid, unsigned short pid, const char *serial) {
char buf[256];
HKEY key;
long size;

  // &Mi_<> is interface number in case device has more interfaces (composite device)
  sprintf (buf,"SYSTEM\\CurrentControlSet\\Enum\\USB\\Vid_%04X&Pid_%04X&Mi_00\\%s_00\\Device Parameters", vid, pid, serial);
  if (RegOpenKey (HKEY_LOCAL_MACHINE, buf, &key) != ERROR_SUCCESS) {
    sprintf (buf, "SYSTEM\\CurrentControlSet\\Enum\\USB\\Vid_%04X&Pid_%04X\\%s\\Device Parameters", vid, pid, serial);
    if (RegOpenKey (HKEY_LOCAL_MACHINE, buf, &key) != ERROR_SUCCESS)
      return -1;
  }
  size = sizeof (buf);
  if (RegQueryValueEx (key, "PortName", NULL, NULL, buf, &size) != ERROR_SUCCESS)
    size = 0;
  RegCloseKey (key);
  if (!size)
    return -2;
  // copy COM port number
  strcpy (name, buf);
  //return size;
  return 0;
}

#endif


#if 0
// ====================================
// unused functions
// ------------------------------------

static int recbuf (DWORD time, unsigned char *data, int len) {
DWORD start, size;
COMMTIMEOUTS cto;

  GetCommTimeouts (hCom, &cto);
  // prectu si predchazejici stav, abych nezmenil zapisovy timeout
  start=GetTickCount ();
  cto.ReadTotalTimeoutConstant=0;
  *data = 0;
  do {
    // nastaveni aktualniho timeoutu
    cto.ReadIntervalTimeout=cto.ReadTotalTimeoutMultiplier=time-(GetTickCount()-start);
    // obcas to hapruje, protoze rozliseni je 20ms; musim provest korekci
    if (!cto.ReadIntervalTimeout) cto.ReadIntervalTimeout=1;
    SetCommTimeouts (hCom, &cto);
    // cekani na znak
    if (!ReadFile (hCom, data, len, &size, NULL)) continue;
    if (!size) continue;
    // zpracovani znaku
    if (size <= len)
      data[size] = 0;
    return size;
  } while ((GetTickCount ()-start)<time);
  return -1;
}

// ====================================
// hledani USB zarizeni
// ====================================

  // zjisteni pripojeneho USB
//  USB_find (0x16D5, 0x0001, 0, line);
  USB_find (0x16D5, 0x0001, "100if", path);
  if (!*path) {
    printf ("USB device not found!\n");
  } else {
    int i;
    printf ("USB device found: \"%s\"\n", path);
    i = 0;
    while (1) {
      hCom = CreateFile (path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
      if (hCom == INVALID_HANDLE_VALUE) {
	if (i)
	  printf ("USB lost!\n");
	i = 0;
      } else {
	CloseHandle (hCom);
	if (!i)
	  printf ("USB found!\n");
	i = 1;
      }
//      Sleep (1000);
    }
  }
  
#endif

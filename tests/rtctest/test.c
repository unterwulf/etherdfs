/*
 * Test program: testing the RTC "master clock" at 046C
 * Copyright (C) 2016 Mateusz Viste
 */


int main(int argc, char **argv) {
  static char hexc[16] = "0123456789ABCDEF";
  static unsigned short far *VGA = (unsigned short far *)(0xB8000000l);
  unsigned char volatile far *rtc = (unsigned char far *)0x46C;
  unsigned short r = 4;
  unsigned long i;
  unsigned char t;
  t = *rtc;
  for (i = 1; i != 0; i++) {
    if (t != *rtc) {
      r++;
      t = *rtc;
    }
    VGA[80 + (r * 3)] = 0x3f00 | hexc[(*rtc >> 4) & 15];
    VGA[81 + (r * 3)] = 0x3f00 | hexc[(*rtc) & 15];
  }
  return(0);
}

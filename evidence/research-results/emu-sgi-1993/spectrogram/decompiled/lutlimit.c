/* lutlimit  entry 0x40b73c  size 12 bytes */

float lutlimit(float param_1)

{
  if (param_1 < 1.0) {
    param_1 = 1.0;
  }
  else if (254.0 < param_1) {
    param_1 = 254.0;
  }
  return param_1;
}


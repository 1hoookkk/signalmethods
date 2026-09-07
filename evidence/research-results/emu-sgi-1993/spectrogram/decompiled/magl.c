/* magl  entry 0x411408  size 12 bytes */

double magl(float param_1,float param_2)

{
  float in_f0;
  float __x;
  
  __x = (param_1 * param_1 + param_2 * param_2) / (float)(DAT_1000b978 * DAT_1000b978);
  if (__x < 1e-05) {
    __x = 1e-05;
  }
  log10f(__x);
  return (double)in_f0 * m + b;
}


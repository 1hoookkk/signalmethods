/* usage  entry 0x40596c  size 12 bytes */

void usage(void)

{
  printf("Usage:\tspan <-ak> <-ffilename> <-d> <-wW> <-oM> <-nN> <-h> <-t> <-p>\n");
  printf("\t<aifffile>\n");
  printf("\n");
  printf("\tDisplays the power spectrum of the audio input.\n");
  printf("\tk is the feedback constant in the exponential smoother (default\n");
  printf("\tno smoothing). filename is an xgraph file to write snapshots to.\n");
  printf("\t-d will cause the DC component to be removed\n");
  printf("\tW is window type (Blackman), M is FFT size(1024)\n");
  printf("\tN is window length(1024), -h prints this msg,\n");
  printf("\t-t prints the window options.\n");
  printf("\t-p displays power (time-normalized) instead of energy\n");
  printf("\tif aifffile is specified, input will be read from the named file.\n");
  printf("\tNOTE: aifffile must be the LAST argument!\n\n");
  printf("\tWhile active, you can press the following keys:\n\n");
  printf("\tC: \treset the averager\n");
  printf("\tQ: \tquit\n");
  printf("\tR: \trewind to beginning of file\n");
  printf("\tS: \ttake a snapshot\n");
  printf("\tX: \tchange x-axis limits\n");
  printf("\tY: \tchange y-axis limits\n");
  return;
}


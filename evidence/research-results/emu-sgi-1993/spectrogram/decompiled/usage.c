/* usage  entry 0x413238  size 12 bytes */

void usage(void)

{
  printf("Usage:\n");
  printf("spectrogram [options] soundfile.[aiff]\n");
  printf("\toptions are (defaults in parentheses):\n");
  printf("\t    -b: double-buffer mode\n");
  printf("\t    -d n: number of frames (500)\n");
  printf("\t    -e: do LPC spectral envelope analysis of input\n");
  printf("\t    -f: 2D (flat) mode\n");
  printf("\t    -g: draw reconstructed waveform (FALSE)\n");
  printf("\t    -h help (this message)\n");
  printf("\t    -l: realtime input mode (FALSE)\n");
  printf("\t    -m n: draw mode (1=line, 2=point, 3=polygon, 4=mesh (DEFAULT))\n");
  printf("\t    -n n: number of samples to analyze/frame (256)\n");
  printf("\t    -o n: FFT size (256)\n");
  printf("\t    -p: polar mode (FALSE)\n");
  printf("\t    -s n: stride: no. of samples to advance each frame (128)\n");
  printf("\t    -S use a synthesis window\n");
  printf("\t    -t: do filter-function synthesis\n");
  printf("\t    -w n: type of window to use (7)\n");
  printf("\t\t0 = exact blackman, 1 = blackman, 2-5 = blackman-harris\n");
  printf("\t\t6 = hamming, 7 = hanning, 8 = rectangular (no windowing!)\n");
  printf("\t    -x: do cross synthesis\n");
  printf("\t    -y: supress perspective\n");
  printf("\t    -z: do z-buffering\n");
  printf("\n");
  return;
}


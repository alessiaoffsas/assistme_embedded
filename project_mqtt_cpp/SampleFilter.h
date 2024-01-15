#ifndef SAMPLEFILTER_H_
#define SAMPLEFILTER_H_

/*

FIR filter designed with
 http://t-filter.appspot.com

sampling frequency: 400 Hz

* 0.3 Hz - 4 Hz
  gain = 1
  desired ripple = 0.1 dB
  actual ripple = 1.3932933234748948 dB

* 10 Hz - 200 Hz
  gain = 0
  desired attenuation = -220 dB
  actual attenuation = -194.4908564259464 dB

*/

#define SAMPLEFILTER_TAP_NUM 407

typedef struct {
  double history[SAMPLEFILTER_TAP_NUM];
  unsigned int last_index;
} SampleFilter;

void SampleFilter_init(SampleFilter* f);
void SampleFilter_put(SampleFilter* f, double input);
double SampleFilter_get(SampleFilter* f);

#endif
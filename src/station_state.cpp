#include "station_state.h"
#include <time.h>

float calcDewPointC(float t, float rh) {
  if (!isfinite(t) || !isfinite(rh) || rh <= 0.0f) return NAN;
  const float a = 17.62f, b = 243.12f;
  const float g = logf(rh / 100.0f) + (a * t) / (b + t);
  return (b * g) / (a - g);
}

float calcHeatIndexC(float tC, float rh) {
  if (!isfinite(tC) || !isfinite(rh)) return NAN;
  float t = tC * 9.0f / 5.0f + 32.0f;
  if (t < 80.0f || rh < 40.0f) return tC;
  float hi = -42.379f + 2.04901523f*t + 10.14333127f*rh
      - 0.22475541f*t*rh - 0.00683783f*t*t - 0.05481717f*rh*rh
      + 0.00122874f*t*t*rh + 0.00085282f*t*rh*rh
      - 0.00000199f*t*t*rh*rh;
  return (hi - 32.0f) * 5.0f / 9.0f;
}

float calcWindChillC(float tC, float windKmh) {
  if (!isfinite(tC) || !isfinite(windKmh) || tC > 10.0f || windKmh < 4.8f) return tC;
  float v = powf(windKmh, 0.16f);
  return 13.12f + 0.6215f*tC - 11.37f*v + 0.3965f*tC*v;
}

uint8_t calcBeaufort(float ms) {
  static const float t[] = {0.3f,1.6f,3.4f,5.5f,8.0f,10.8f,13.9f,17.2f,20.8f,24.5f,28.5f,32.7f};
  for (uint8_t i=0;i<12;i++) if (ms < t[i]) return i;
  return 12;
}

static uint32_t nowEpoch(){ time_t n=time(nullptr); return n>1700000000?(uint32_t)n:0U; }
void updateDailyExtremes(StationState &s) {
  uint32_t n=nowEpoch();
  if(isfinite(s.outTempC) && (!isfinite(s.tempDayHighC)||s.outTempC>s.tempDayHighC)){s.tempDayHighC=s.outTempC;s.tempDayHighEpoch=n;}
  if(isfinite(s.outTempC) && (!isfinite(s.tempDayLowC)||s.outTempC<s.tempDayLowC)){s.tempDayLowC=s.outTempC;s.tempDayLowEpoch=n;}
  if(isfinite(s.windKmh) && (!isfinite(s.windDayMaxKmh)||s.windKmh>s.windDayMaxKmh)){s.windDayMaxKmh=s.windKmh;s.windDayMaxEpoch=n;}
  if(isfinite(s.windGustKmh) && (!isfinite(s.gustDayMaxKmh)||s.windGustKmh>s.gustDayMaxKmh)){s.gustDayMaxKmh=s.windGustKmh;s.gustDayMaxEpoch=n;}
  if(isfinite(s.pressureHpa) && (!isfinite(s.pressureDayHighHpa)||s.pressureHpa>s.pressureDayHighHpa)){s.pressureDayHighHpa=s.pressureHpa;s.pressureDayHighEpoch=n;}
  if(isfinite(s.pressureHpa) && (!isfinite(s.pressureDayLowHpa)||s.pressureHpa<s.pressureDayLowHpa)){s.pressureDayLowHpa=s.pressureHpa;s.pressureDayLowEpoch=n;}
  if(isfinite(s.uv) && (!isfinite(s.uvDayMax)||s.uv>s.uvDayMax)) s.uvDayMax=s.uv;
  if(isfinite(s.solarWm2) && (!isfinite(s.solarDayMax)||s.solarWm2>s.solarDayMax)) s.solarDayMax=s.solarWm2;
}

void resetDailyExtremes(StationState &s){
  s.tempDayHighC=s.tempDayLowC=NAN; s.windDayMaxKmh=s.gustDayMaxKmh=NAN;
  s.pressureDayHighHpa=s.pressureDayLowHpa=NAN; s.uvDayMax=s.solarDayMax=NAN;
  s.tempDayHighEpoch=s.tempDayLowEpoch=s.windDayMaxEpoch=s.gustDayMaxEpoch=0;
  s.pressureDayHighEpoch=s.pressureDayLowEpoch=0;
}

#include "davis_decoder.h"
#include <math.h>

uint8_t reverseBits8(uint8_t b){
  b=((b&0xF0)>>4)|((b&0x0F)<<4); b=((b&0xCC)>>2)|((b&0x33)<<2); b=((b&0xAA)>>1)|((b&0x55)<<1); return b;
}
uint16_t davisCrc16(const uint8_t *p, size_t len, uint16_t crc){
  while(len--){ crc ^= (uint16_t)(*p++) << 8; for(uint8_t i=0;i<8;i++) crc=(crc&0x8000)?(uint16_t)((crc<<1)^0x1021):(uint16_t)(crc<<1); } return crc;
}

bool validateAndNormalizeDavisPacket(const uint8_t *raw, size_t len, uint8_t out[10]){
  if(len != 10) return false;
  for(size_t i=0;i<10;i++) out[i]=reverseBits8(raw[i]);
  const uint16_t crc=davisCrc16(out,6);
  const uint16_t received=((uint16_t)out[6]<<8)|out[7];
  return crc == received;
}

DavisDecodeResult decodeDavisPacket(const uint8_t d[10], StationState &s, float rainTip){
  DavisDecodeResult r; r.stationId=d[0]&0x07U; r.packetType=d[0]>>4; r.valid=true;
  s.stationId=r.stationId; s.batteryLow=(d[0]&0x08U)!=0; s.lastPacketType=r.packetType;

  // Every ISS packet carries instantaneous wind speed (mph), direction and battery.
  s.windKmh=(float)d[1]*1.609344f;
  s.windDirDeg=9.0f+(float)d[2]*(342.0f/255.0f);

  switch(r.packetType){
    case 0x4: { // UV index
      if(d[3]!=0xFF){ uint16_t raw=((uint16_t)d[3]<<8)|d[4]; raw >>= 4; float v=((float)raw-4.0f)/200.0f; if(v>=0&&v<=30) s.uv=v; }
      break;
    }
    case 0x5: { // seconds since rain bucket tip
      const uint8_t msn=d[4]>>4;
      int32_t secs=(msn<4U)?((int32_t)(d[3]>>4)+(int32_t)d[4]-1L):((int32_t)d[3]+(int32_t)(msn-4U)*256L);
      s.rainRateMmH=(secs>0&&secs<1020)?(rainTip*3600.0f/(float)secs):0.0f;
      break;
    }
    case 0x6: { // solar radiation: 10-bit value
      if(d[3]!=0xFF){ uint16_t raw=(uint16_t)d[3]*4U+((uint16_t)d[4]>>6); if(raw<=1800U) s.solarWm2=(float)raw; }
      break;
    }
    case 0x8: { // outside temperature in tenths of Fahrenheit
      int16_t tenthsF=(int16_t)(((uint16_t)d[3]<<8)|d[4]); tenthsF >>= 4;
      float c=(((float)tenthsF/10.0f)-32.0f)*5.0f/9.0f; if(c>-80&&c<80) s.outTempC=c;
      break;
    }
    case 0x9: { // 10-minute gust in mph
      float g=(float)d[3]*1.609344f; if(g>=0&&g<300) s.windGustKmh=g; break;
    }
    case 0xA: { // outside humidity, tenths percent
      uint16_t raw=((uint16_t)(d[4]>>4)<<8)|d[3]; float h=(float)raw/10.0f; if(h>0&&h<=100) s.outHumidity=h; break;
    }
    case 0xE: { // rain bucket counter, rolls 127 -> 0
      uint8_t c=d[3]&0x7FU;
      if(s.haveRainCounter){ int delta=(int)c-(int)s.rainCounter; if(delta<0) delta+=128; if(delta>0&&delta<20){ float mm=(float)delta*rainTip; s.rainDayMm+=mm; s.rainMonthMm+=mm; s.rainYearMm+=mm; s.lastRainTipMs=millis(); }}
      s.rainCounter=c; s.haveRainCounter=true; break;
    }
    default: break;
  }
  if(!isfinite(s.windGustKmh)) s.windGustKmh=s.windKmh;
  updateDailyExtremes(s);
  return r;
}

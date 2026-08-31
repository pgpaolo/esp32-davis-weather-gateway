#include "rain_persistence.h"

#include <Preferences.h>
#include <time.h>

namespace {
Preferences prefs;
uint32_t lastSaveMs = 0;
float lastDay = -1.0f, lastMonth = -1.0f, lastYear = -1.0f;
uint8_t lastCounter = 0;
bool lastHaveCounter = false;
int currentDayKey = 0;
int currentMonthKey = 0;
int currentYearKey = 0;

bool localKeys(int &dayKey, int &monthKey, int &yearKey) {
  time_t now = time(nullptr);
  if(now < 1700000000) return false;
  struct tm t;
  localtime_r(&now, &t);
  const int y = t.tm_year + 1900;
  const int m = t.tm_mon + 1;
  const int d = t.tm_mday;
  dayKey = y*10000 + m*100 + d;
  monthKey = y*100 + m;
  yearKey = y;
  return true;
}

void saveInternal(const StationState &s) {
  prefs.begin("davisrain", false);
  prefs.putFloat("day", s.rainDayMm);
  prefs.putFloat("month", s.rainMonthMm);
  prefs.putFloat("year", s.rainYearMm);
  prefs.putFloat("yday", s.rainYesterdayMm);
  prefs.putInt("daykey", currentDayKey);
  prefs.putInt("monkey", currentMonthKey);
  prefs.putInt("yrkey", currentYearKey);
  prefs.putUChar("counter", s.rainCounter);
  prefs.putBool("havecnt", s.haveRainCounter);
  prefs.end();
  lastDay=s.rainDayMm; lastMonth=s.rainMonthMm; lastYear=s.rainYearMm;
  lastCounter=s.rainCounter; lastHaveCounter=s.haveRainCounter;
  lastSaveMs=millis();
}
}

void initRainPersistence(StationState &s) {
  prefs.begin("davisrain", true);
  s.rainDayMm = prefs.getFloat("day", 0.0f);
  s.rainMonthMm = prefs.getFloat("month", 0.0f);
  s.rainYearMm = prefs.getFloat("year", 0.0f);
  s.rainYesterdayMm = prefs.getFloat("yday", 0.0f);
  currentDayKey = prefs.getInt("daykey", 0);
  currentMonthKey = prefs.getInt("monkey", 0);
  currentYearKey = prefs.getInt("yrkey", 0);
  s.rainCounter = prefs.getUChar("counter", 0);
  s.haveRainCounter = prefs.getBool("havecnt", false);
  prefs.end();

  lastDay=s.rainDayMm; lastMonth=s.rainMonthMm; lastYear=s.rainYearMm;
  lastCounter=s.rainCounter; lastHaveCounter=s.haveRainCounter;

  int dk,mk,yk;
  if(localKeys(dk,mk,yk)) {
    if(currentYearKey != 0 && currentYearKey != yk) {
      s.rainYearMm = 0.0f;
      s.rainMonthMm = 0.0f;
      s.rainYesterdayMm = s.rainDayMm;
      s.rainDayMm = 0.0f;
    } else if(currentMonthKey != 0 && currentMonthKey != mk) {
      s.rainMonthMm = 0.0f;
      s.rainYesterdayMm = s.rainDayMm;
      s.rainDayMm = 0.0f;
    } else if(currentDayKey != 0 && currentDayKey != dk) {
      s.rainYesterdayMm = s.rainDayMm;
      s.rainDayMm = 0.0f;
    }
    currentDayKey=dk; currentMonthKey=mk; currentYearKey=yk;
    saveInternal(s);
  }
}

void serviceRainPersistence(StationState &s) {
  int dk,mk,yk;
  if(localKeys(dk,mk,yk)) {
    bool calendarChanged=false;
    if(currentYearKey != 0 && currentYearKey != yk) {
      s.rainYesterdayMm=s.rainDayMm;
      s.rainDayMm=0.0f; s.rainMonthMm=0.0f; s.rainYearMm=0.0f;
      resetDailyExtremes(s);
      calendarChanged=true;
    } else if(currentMonthKey != 0 && currentMonthKey != mk) {
      s.rainYesterdayMm=s.rainDayMm;
      s.rainDayMm=0.0f; s.rainMonthMm=0.0f;
      resetDailyExtremes(s);
      calendarChanged=true;
    } else if(currentDayKey != 0 && currentDayKey != dk) {
      s.rainYesterdayMm=s.rainDayMm;
      s.rainDayMm=0.0f;
      resetDailyExtremes(s);
      calendarChanged=true;
    }
    currentDayKey=dk; currentMonthKey=mk; currentYearKey=yk;
    if(calendarChanged) saveInternal(s);
  }

  const bool changed = s.rainDayMm != lastDay || s.rainMonthMm != lastMonth || s.rainYearMm != lastYear ||
                       s.rainCounter != lastCounter || s.haveRainCounter != lastHaveCounter;
  if(changed && (uint32_t)(millis()-lastSaveMs) >= 15000UL) saveInternal(s);
}

void forceSaveRainPersistence(const StationState &s) { saveInternal(s); }

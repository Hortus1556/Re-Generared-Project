#include "surgeon/SaveData.h"

double SaveData::totalScore() const {
    double s = 0;
    for (int i = 1; i <= kLevels; ++i) s += score[i];
    return s;
}

double SaveData::totalTimer() const {
    double t = 0;
    for (int i = 1; i <= kLevels; ++i) t += timer[i];
    return t;
}

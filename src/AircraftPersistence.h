#pragma once
#include "Aircraft.h"

class AircraftPersistence {
public:
    static constexpr int kWebHistoryMax = 20;
    static constexpr int kClassCount    = 4;

    void loadCounts(int* counts) const;
    void saveCounts(const int* counts) const;

    int  loadHistory(Aircraft* history) const;  // returns number of entries loaded
    void saveHistory(const Aircraft* history, int count) const;

private:
    static const char* const kNvsNamespace;
    static const char* const kCountKeys[kClassCount];
};

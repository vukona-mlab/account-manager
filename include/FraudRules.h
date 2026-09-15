#pragma once
// =====================================================================
// FraudRules.h
// ---------------------------------------------------------------------
// Holds the CONFIGURABLE thresholds used by the fraud-detection rules.
// These values come from config/fraud_rules.json instead of being
// hardcoded, so you can tune the system's sensitivity without touching
// any C++ code (see NFR: Configurability).
// =====================================================================

#include <string>

struct FraudRuleConfig
{
    int maxWithdrawals = 3;             // Rule 2: how many withdrawals...
    int timeWindowSeconds = 10;         // ...are allowed within this many seconds
    double spendingSpikeMultiplier = 5; // Rule 3: amount must be <= (average * multiplier)
};

class FraudRules
{
public:
    // Reads config/fraud_rules.json and returns the values found there.
    // If the file is missing or malformed, sensible defaults are used
    // instead of crashing - a configuration problem should not bring
    // the whole banking demo down.
    static FraudRuleConfig loadFromFile(const std::string& path);
};

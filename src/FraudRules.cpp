#include "../include/FraudRules.h"
#include "../include/Json.h"
#include <iostream>

FraudRuleConfig FraudRules::loadFromFile(const std::string& path)
{
    FraudRuleConfig config; // starts with default values

    try
    {
        JsonValue root = JsonParser::parseFile(path);

        const JsonValue& rapid = root.at("rapid_withdrawal");
        config.maxWithdrawals = rapid.at("max_withdrawals").asInt();
        config.timeWindowSeconds = rapid.at("time_window_seconds").asInt();

        const JsonValue& spike = root.at("spending_spike");
        config.spendingSpikeMultiplier = spike.at("multiplier").asDouble();
    }
    catch (const std::exception& e)
    {
        std::cout << "[WARNING] Could not load " << path
                  << " (" << e.what() << "). Using default fraud thresholds.\n";
    }

    return config;
}

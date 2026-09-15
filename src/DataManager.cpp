#include "../include/DataManager.h"
#include "../include/Json.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <cstdio>

std::string currentTimestamp()
{
    std::time_t now = std::time(nullptr);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &localTime);
    return std::string(buffer);
}

DataManager::DataManager(std::string usersPath, std::string accountsPath, std::string transactionsPath)
    : usersFilePath(std::move(usersPath)),
      accountsFilePath(std::move(accountsPath)),
      transactionsFilePath(std::move(transactionsPath))
{
}

// ---------------------------------------------------------------------
// LOADING
// ---------------------------------------------------------------------

void DataManager::loadAll()
{
    users.clear();
    accounts.clear();
    transactions.clear();

    // ---- users.json ----
    JsonValue usersJson = JsonParser::parseFile(usersFilePath);
    for (const JsonValue& item : usersJson.arrayValue)
    {
        User user;
        user.userId = item.at("userId").asInt();
        user.name = item.at("name").asString();
        user.accountNumber = item.at("accountNumber").asString();
        user.pin = item.at("pin").asString();
        users.push_back(user);
    }

    // ---- accounts.json ----
    JsonValue accountsJson = JsonParser::parseFile(accountsFilePath);
    for (const JsonValue& item : accountsJson.arrayValue)
    {
        Account account;
        account.accountNumber = item.at("accountNumber").asString();
        account.ownerUserId = item.at("ownerUserId").asInt();
        account.balance = item.at("balance").asDouble();
        accounts.push_back(account);
    }

    // ---- transactions.json ----
    JsonValue transactionsJson = JsonParser::parseFile(transactionsFilePath);
    for (const JsonValue& item : transactionsJson.arrayValue)
    {
        Transaction transaction;
        transaction.id = item.at("id").asInt();
        transaction.accountNumber = item.at("accountNumber").asString();
        transaction.type = item.at("type").asString();
        transaction.amount = item.at("amount").asDouble();
        transaction.status = item.at("status").asString();
        transaction.reason = item.at("reason").asString();
        transaction.timestamp = item.at("timestamp").asString();
        transaction.destinationAccount = item.hasKey("destinationAccount")
            ? item.at("destinationAccount").asString() : "";
        transactions.push_back(transaction);
    }

    std::cout << "[DataManager] Loaded " << users.size() << " users, "
              << accounts.size() << " accounts, "
              << transactions.size() << " transactions.\n";
}

// ---------------------------------------------------------------------
// SAVING
// ---------------------------------------------------------------------

void DataManager::saveUsers() const
{
    JsonValue array = JsonValue::makeArray();
    for (const User& user : users)
    {
        JsonValue obj = JsonValue::makeObject();
        obj.set("userId", JsonValue::makeNumber(user.userId));
        obj.set("name", JsonValue::makeString(user.name));
        obj.set("accountNumber", JsonValue::makeString(user.accountNumber));
        obj.set("pin", JsonValue::makeString(user.pin));
        array.arrayValue.push_back(obj);
    }
    std::ofstream file(usersFilePath);
    file << array.dump();
}

void DataManager::saveAccounts() const
{
    JsonValue array = JsonValue::makeArray();
    for (const Account& account : accounts)
    {
        JsonValue obj = JsonValue::makeObject();
        obj.set("accountNumber", JsonValue::makeString(account.accountNumber));
        obj.set("ownerUserId", JsonValue::makeNumber(account.ownerUserId));
        obj.set("balance", JsonValue::makeNumber(account.balance));
        array.arrayValue.push_back(obj);
    }
    std::ofstream file(accountsFilePath);
    file << array.dump();
}

void DataManager::saveTransactions() const
{
    JsonValue array = JsonValue::makeArray();
    for (const Transaction& t : transactions)
    {
        JsonValue obj = JsonValue::makeObject();
        obj.set("id", JsonValue::makeNumber(t.id));
        obj.set("accountNumber", JsonValue::makeString(t.accountNumber));
        obj.set("type", JsonValue::makeString(t.type));
        obj.set("amount", JsonValue::makeNumber(t.amount));
        obj.set("status", JsonValue::makeString(t.status));
        obj.set("reason", JsonValue::makeString(t.reason));
        obj.set("timestamp", JsonValue::makeString(t.timestamp));
        obj.set("destinationAccount", JsonValue::makeString(t.destinationAccount));
        array.arrayValue.push_back(obj);
    }
    std::ofstream file(transactionsFilePath);
    file << array.dump();
}

// ---------------------------------------------------------------------
// LOOKUPS
// ---------------------------------------------------------------------

bool DataManager::findUserByAccount(const std::string& accountNumber, User& outUser) const
{
    for (const User& user : users)
    {
        if (user.accountNumber == accountNumber)
        {
            outUser = user;
            return true;
        }
    }
    return false;
}

bool DataManager::findAccount(const std::string& accountNumber, Account& outAccount) const
{
    for (const Account& account : accounts)
    {
        if (account.accountNumber == accountNumber)
        {
            outAccount = account;
            return true;
        }
    }
    return false;
}

bool DataManager::accountExists(const std::string& accountNumber) const
{
    Account temp;
    return findAccount(accountNumber, temp);
}

bool DataManager::updateAccountBalance(const std::string& accountNumber, double newBalance)
{
    for (Account& account : accounts)
    {
        if (account.accountNumber == accountNumber)
        {
            account.balance = newBalance;
            saveAccounts(); // persist immediately so data/accounts.json always reflects reality
            return true;
        }
    }
    return false;
}

std::vector<Transaction> DataManager::getTransactionsForAccount(const std::string& accountNumber) const
{
    std::vector<Transaction> result;
    for (const Transaction& t : transactions)
    {
        if (t.accountNumber == accountNumber)
        {
            result.push_back(t);
        }
    }
    return result;
}

int DataManager::nextTransactionId() const
{
    int maxId = 0;
    for (const Transaction& t : transactions)
    {
        if (t.id > maxId) maxId = t.id;
    }
    return maxId + 1;
}

void DataManager::addTransaction(const Transaction& transaction)
{
    transactions.push_back(transaction);
    saveTransactions(); // persist immediately - every attempt is recorded straight away
}

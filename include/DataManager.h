#pragma once
// =====================================================================
// DataManager.h
// ---------------------------------------------------------------------
// The DataManager is the ONLY part of the program that reads or writes
// files. Everything else (dashboard, middleware) asks the DataManager
// for data instead of touching users.json / accounts.json /
// transactions.json directly. This keeps "storage" cleanly separated
// from "business logic" and "user interface" (see section 29 of the
// spec - the core architectural goal of this project).
// =====================================================================

#include <vector>
#include <string>
#include "User.h"
#include "Account.h"
#include "Transaction.h"

class DataManager
{
public:
    DataManager(std::string usersPath, std::string accountsPath, std::string transactionsPath);

    // ---- Loading & saving (Stage 3) ----
    void loadAll();
    void saveUsers() const;
    void saveAccounts() const;
    void saveTransactions() const;

    // ---- Lookups used by authentication & middleware ----
    bool findUserByAccount(const std::string& accountNumber, User& outUser) const;
    bool findAccount(const std::string& accountNumber, Account& outAccount) const;
    bool accountExists(const std::string& accountNumber) const;

    // Updates a single account's balance in memory AND saves it to disk.
    // Returns false if the account does not exist.
    bool updateAccountBalance(const std::string& accountNumber, double newBalance);

    // Returns only the transactions belonging to one account, in the
    // order they were recorded.
    std::vector<Transaction> getTransactionsForAccount(const std::string& accountNumber) const;

    int nextTransactionId() const;

    // Adds a transaction to memory AND appends it to transactions.json.
    void addTransaction(const Transaction& transaction);

    const std::vector<User>& getUsers() const { return users; }
    const std::vector<Account>& getAccounts() const { return accounts; }
    const std::vector<Transaction>& getTransactions() const { return transactions; }

private:
    std::string usersFilePath;
    std::string accountsFilePath;
    std::string transactionsFilePath;

    std::vector<User> users;
    std::vector<Account> accounts;
    std::vector<Transaction> transactions;
};

// A small, reusable helper: returns the current time formatted as
// "YYYY-MM-DDTHH:MM:SS", used as the timestamp for new transactions.
std::string currentTimestamp();

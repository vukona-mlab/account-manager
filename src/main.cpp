// =====================================================================
// main.cpp
// ---------------------------------------------------------------------
// This file is the ENTIRE user interface. Notice what it does NOT do:
// it never checks balances, never counts withdrawals, never compares
// amounts against thresholds. It only:
//   1. asks the user for input,
//   2. builds a TransactionRequest,
//   3. hands it to the TransactionMiddleware,
//   4. prints whatever TransactionResult comes back.
//
// If you ever find yourself wanting to add an "if (balance < amount)"
// check in this file - stop. That check belongs in
// TransactionMiddleware.cpp, not here (see spec section 29).
// =====================================================================

#include <iostream>
#include <iomanip>
#include <sstream>
#include <limits>

#include "../include/User.h"
#include "../include/Account.h"
#include "../include/Transaction.h"
#include "../include/FraudRules.h"
#include "../include/DataManager.h"
#include "../include/TransactionMiddleware.h"

// ---------------------------------------------------------------------
// Small display helpers (pure formatting, no business logic)
// ---------------------------------------------------------------------

// Turns 10000.0 into "R10,000.00"
std::string formatCurrency(double amount)
{
    bool negative = amount < 0;
    if (negative) amount = -amount;

    std::ostringstream raw;
    raw << std::fixed << std::setprecision(2) << amount;
    std::string number = raw.str(); // e.g. "10000.00"

    std::string integerPart = number.substr(0, number.find('.'));
    std::string decimalPart = number.substr(number.find('.'));

    // Insert a comma every 3 digits from the right.
    std::string withCommas;
    int digitsSinceComma = 0;
    for (int i = static_cast<int>(integerPart.size()) - 1; i >= 0; --i)
    {
        withCommas = integerPart[i] + withCommas;
        digitsSinceComma++;
        if (digitsSinceComma == 3 && i != 0)
        {
            withCommas = "," + withCommas;
            digitsSinceComma = 0;
        }
    }

    return std::string(negative ? "-R" : "R") + withCommas + decimalPart;
}

void printCheckLog(const TransactionResult& result)
{
    for (const std::string& line : result.checkLog)
    {
        std::cout << line << "\n";
    }
}

double readAmount(const std::string& prompt)
{
    std::cout << prompt;
    double amount;
    while (!(std::cin >> amount))
    {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Please enter a valid number: ";
    }
    return amount;
}

// ---------------------------------------------------------------------
// Authentication (Stage 4)
// ---------------------------------------------------------------------

bool login(DataManager& data, User& outUser)
{
    std::string accountNumber, pin;

    std::cout << "\n=========================================\n";
    std::cout << "       BANK TRANSACTION SYSTEM - LOGIN\n";
    std::cout << "=========================================\n";
    std::cout << "Account Number: ";
    std::cin >> accountNumber;
    std::cout << "PIN: ";
    std::cin >> pin;

    User candidate;
    bool found = data.findUserByAccount(accountNumber, candidate);

    if (!found || candidate.pin != pin)
    {
        std::cout << "\nLOGIN FAILED\nInvalid account number or PIN.\n";
        return false;
    }

    std::cout << "\nLOGIN SUCCESSFUL\nWelcome " << candidate.name << ".\n";
    outUser = candidate;
    return true;
}

// ---------------------------------------------------------------------
// Dashboard actions (Stages 6, 7, 9-12) - UI ONLY, all decisions come
// from TransactionMiddleware
// ---------------------------------------------------------------------

void showDashboardHeader(const User& user, const Account& account)
{
    std::cout << "\n=========================================\n";
    std::cout << "       BANK TRANSACTION SYSTEM\n";
    std::cout << "=========================================\n\n";
    std::cout << "Welcome, " << user.name << "\n\n";
    std::cout << "Account: " << account.accountNumber << "\n";
    std::cout << "Balance: " << formatCurrency(account.balance) << "\n\n";
    std::cout << "-----------------------------------------\n";
    std::cout << "1. Deposit Money\n";
    std::cout << "2. Withdraw Money\n";
    std::cout << "3. Transfer Money\n";
    std::cout << "4. View Transaction History\n";
    std::cout << "5. View Account Details\n";
    std::cout << "6. Logout\n";
    std::cout << "-----------------------------------------\n";
    std::cout << "Select an option: ";
}

void handleDeposit(TransactionMiddleware& middleware, const User& user)
{
    double amount = readAmount("\nEnter deposit amount: R");

    TransactionRequest request;
    request.accountNumber = user.accountNumber;
    request.type = TransactionType::DEPOSIT;
    request.amount = amount;

    TransactionResult result = middleware.processDeposit(request, user);
    printCheckLog(result);

    if (result.approved)
    {
        std::cout << "\nOld balance: " << formatCurrency(result.balanceBefore) << "\n";
        std::cout << "Deposit:      " << formatCurrency(amount) << "\n";
        std::cout << "New balance: " << formatCurrency(result.balanceAfter) << "\n";
        std::cout << "\nTransaction APPROVED (ID " << result.transactionId << ")\n";
    }
    else
    {
        std::cout << "\nTransaction BLOCKED\nReason: " << result.reason << "\n";
    }
}

void handleWithdraw(TransactionMiddleware& middleware, const User& user)
{
    double amount = readAmount("\nEnter withdrawal amount: R");

    TransactionRequest request;
    request.accountNumber = user.accountNumber;
    request.type = TransactionType::WITHDRAWAL;
    request.amount = amount;

    TransactionResult result = middleware.processTransaction(request, user);
    printCheckLog(result);

    if (result.approved)
    {
        std::cout << "\nTransaction APPROVED (ID " << result.transactionId << ")\n";
        std::cout << "New balance: " << formatCurrency(result.balanceAfter) << "\n";
    }
    else
    {
        std::cout << "\nTransaction BLOCKED\nReason: " << result.reason << "\n";
    }
}

void handleTransfer(TransactionMiddleware& middleware, const User& user)
{
    std::string destination;
    std::cout << "\nRecipient Account: ";
    std::cin >> destination;
    double amount = readAmount("Amount: R");

    TransactionRequest request;
    request.accountNumber = user.accountNumber;
    request.type = TransactionType::TRANSFER;
    request.amount = amount;
    request.destinationAccount = destination;

    TransactionResult result = middleware.processTransaction(request, user);
    printCheckLog(result);

    if (result.approved)
    {
        std::cout << "\nTransaction APPROVED (ID " << result.transactionId << ")\n";
        std::cout << "New balance: " << formatCurrency(result.balanceAfter) << "\n";
    }
    else
    {
        std::cout << "\nTransaction BLOCKED\nReason: " << result.reason << "\n";
    }
}

void handleHistory(DataManager& data, const User& user)
{
    std::cout << "\n=========================================\n";
    std::cout << "TRANSACTION HISTORY\n";
    std::cout << "=========================================\n\n";
    std::cout << std::left << std::setw(6) << "ID"
              << std::setw(13) << "TYPE"
              << std::setw(12) << "AMOUNT"
              << std::setw(10) << "STATUS" << "\n";
    std::cout << "-----------------------------------------\n";

    // Ownership check: a user can only ever request THEIR OWN
    // account's history because we always pass user.accountNumber,
    // never a value typed in by the person at the keyboard.
    std::vector<Transaction> history = data.getTransactionsForAccount(user.accountNumber);

    for (const Transaction& t : history)
    {
        std::cout << std::left << std::setw(6) << t.id
                  << std::setw(13) << t.type
                  << std::setw(12) << formatCurrency(t.amount)
                  << std::setw(10) << t.status << "\n";
        if (t.status == TransactionStatus::BLOCKED)
        {
            std::cout << "      Reason: " << t.reason << "\n";
        }
    }

    if (history.empty())
    {
        std::cout << "(no transactions yet)\n";
    }
}

void handleAccountDetails(const User& user, const Account& account)
{
    std::cout << "\n=========================================\n";
    std::cout << "ACCOUNT DETAILS\n";
    std::cout << "=========================================\n";
    std::cout << "Name:    " << user.name << "\n";
    std::cout << "Account: " << account.accountNumber << "\n";
    std::cout << "Balance: " << formatCurrency(account.balance) << "\n";
}

// ---------------------------------------------------------------------
// Dashboard loop (Stage 13)
// ---------------------------------------------------------------------

void runDashboard(DataManager& data, TransactionMiddleware& middleware, User& user)
{
    bool loggedIn = true;

    while (loggedIn)
    {
        Account account;
        data.findAccount(user.accountNumber, account); // always re-read the latest balance

        showDashboardHeader(user, account);

        int choice;
        if (!(std::cin >> choice))
        {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "\nPlease enter a number between 1 and 6.\n";
            continue;
        }

        switch (choice)
        {
            case 1: handleDeposit(middleware, user); break;
            case 2: handleWithdraw(middleware, user); break;
            case 3: handleTransfer(middleware, user); break;
            case 4: handleHistory(data, user); break;
            case 5:
            {
                Account current;
                data.findAccount(user.accountNumber, current);
                handleAccountDetails(user, current);
                break;
            }
            case 6:
                std::cout << "\nLogging out. Goodbye, " << user.name << "!\n";
                loggedIn = false;
                break;
            default:
                std::cout << "\nInvalid option. Please choose 1-6.\n";
        }
    }
}

// ---------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------

int main()
{
    // Stage 3: load everything the system needs before showing any UI.
    DataManager data("data/users.json", "data/accounts.json", "data/transactions.json");
    data.loadAll();

    FraudRuleConfig fraudConfig = FraudRules::loadFromFile("config/fraud_rules.json");
    std::cout << "[Config] Rapid withdrawal limit: " << fraudConfig.maxWithdrawals
              << " within " << fraudConfig.timeWindowSeconds << "s\n";
    std::cout << "[Config] Spending spike multiplier: " << fraudConfig.spendingSpikeMultiplier << "x\n";

    TransactionMiddleware middleware(data, fraudConfig);

    bool running = true;
    while (running)
    {
        User user;
        if (login(data, user))
        {
            runDashboard(data, middleware, user);
        }
        else
        {
            std::cout << "\nTry again? (y/n): ";
            char again;
            std::cin >> again;
            if (again != 'y' && again != 'Y') running = false;
        }

        if (running)
        {
            std::cout << "\nReturn to login screen? (y/n): ";
            char restart;
            std::cin >> restart;
            if (restart != 'y' && restart != 'Y') running = false;
        }
    }

    std::cout << "\nThank you for using the Bank Transaction System.\n";
    return 0;
}

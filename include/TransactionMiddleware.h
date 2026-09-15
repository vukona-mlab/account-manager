#pragma once
// =====================================================================
// TransactionMiddleware.h
// ---------------------------------------------------------------------
// THIS IS THE MOST IMPORTANT FILE IN THE PROJECT.
//
// The dashboard NEVER decides whether a transaction is allowed. It only
// builds a TransactionRequest and hands it to the middleware. The
// middleware runs every business rule in a fixed order and returns a
// TransactionResult that clearly explains what happened and why.
//
//   Dashboard -> TransactionRequest -> Middleware -> TransactionResult -> Dashboard
//
// This separation is what makes the business rules easy to test,
// easy to change, and impossible to accidentally bypass from the UI.
// =====================================================================

#include <string>
#include <vector>
#include "User.h"
#include "Account.h"
#include "Transaction.h"
#include "FraudRules.h"
#include "DataManager.h"

// What the dashboard asks the middleware to do.
struct TransactionRequest
{
    std::string accountNumber;         // account the request CLAIMS to be from
    std::string type;                  // WITHDRAWAL, TRANSFER, or DEPOSIT
    double amount = 0.0;
    std::string destinationAccount;    // only used for TRANSFER
};

// What the middleware tells the dashboard after processing.
struct TransactionResult
{
    bool approved = false;
    std::string reason = Reason::NONE;
    double balanceBefore = 0.0;
    double balanceAfter = 0.0;
    int transactionId = 0;

    // A human-readable log of each check, e.g. "[PASS] Sufficient funds".
    // This exists purely so the console can print the same step-by-step
    // trace shown in the spec (section 15) - it has no effect on the
    // decision itself.
    std::vector<std::string> checkLog;
};

class TransactionMiddleware
{
public:
    TransactionMiddleware(DataManager& dataManager, FraudRuleConfig config);

    // Handles WITHDRAWAL and TRANSFER requests. Runs the full rule chain:
    //   1. Account ownership
    //   2. Account existence (source & destination)
    //   3. Amount validation
    //   4. Sufficient funds
    //   5. Rapid withdrawal detection (withdrawals only)
    //   6. Spending spike detection
    //   7. Approve -> update balance(s) -> save transaction(s)
    TransactionResult processTransaction(const TransactionRequest& request, const User& authenticatedUser);

    // Deposits skip the fraud rules (you cannot commit fraud by giving
    // yourself money), but they still go through ownership + amount
    // validation and are still fully recorded.
    TransactionResult processDeposit(const TransactionRequest& request, const User& authenticatedUser);

private:
    DataManager& data;
    FraudRuleConfig fraudConfig;

    // ---- Individual rule checks. Each returns true = PASS. ----
    bool checkOwnership(const TransactionRequest& request, const User& user, TransactionResult& result);
    bool checkAmountValid(double amount, TransactionResult& result);
    bool checkAccountExists(const std::string& accountNumber, TransactionResult& result);
    bool checkSufficientFunds(const Account& account, double amount, TransactionResult& result);
    bool checkRapidWithdrawals(const std::string& accountNumber, TransactionResult& result);
    bool checkSpendingSpike(const std::string& accountNumber, double amount, TransactionResult& result);

    // ---- Calculations used by the rules above ----
    // Rule 3 support: average of past WITHDRAWAL + TRANSFER amounts.
    double calculateAverageTransactionAmount(const std::string& accountNumber) const;

    // Rule 2 support: withdrawals for this account within the last
    // fraudConfig.timeWindowSeconds, counted from "now".
    int countRecentWithdrawals(const std::string& accountNumber) const;

    void logStep(TransactionResult& result, const std::string& stepName, bool passed);
};

#include "../include/TransactionMiddleware.h"
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>

// -----------------------------------------------------------------
// Small local helper: turns "YYYY-MM-DDTHH:MM:SS" back into a
// std::time_t so we can subtract two timestamps and get a number of
// seconds. This is only used by the rapid-withdrawal rule.
// -----------------------------------------------------------------
static std::time_t parseTimestamp(const std::string& timestamp)
{
    std::tm tm{};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

TransactionMiddleware::TransactionMiddleware(DataManager& dataManager, FraudRuleConfig config)
    : data(dataManager), fraudConfig(config)
{
}

void TransactionMiddleware::logStep(TransactionResult& result, const std::string& stepName, bool passed)
{
    result.checkLog.push_back(std::string(passed ? "[PASS] " : "[FAIL] ") + stepName);
}

// =====================================================================
// RULE CHECKS
// Each function focuses on ONE rule and returns true when the rule is
// satisfied ("the transaction may continue"). If a rule fails, it also
// fills in result.reason so the caller knows exactly why.
// =====================================================================

bool TransactionMiddleware::checkOwnership(const TransactionRequest& request, const User& user, TransactionResult& result)
{
    // The account named in the request must be the SAME account that
    // belongs to whoever is logged in. This stops Alice from typing
    // Bob's account number into her own withdrawal request.
    bool ok = (request.accountNumber == user.accountNumber);
    logStep(result, "Account ownership", ok);
    if (!ok) result.reason = Reason::UNAUTHORIZED_ACCOUNT_ACCESS;
    return ok;
}

bool TransactionMiddleware::checkAmountValid(double amount, TransactionResult& result)
{
    bool ok = (amount > 0);
    logStep(result, "Amount validation", ok);
    if (!ok) result.reason = Reason::INVALID_AMOUNT;
    return ok;
}

bool TransactionMiddleware::checkAccountExists(const std::string& accountNumber, TransactionResult& result)
{
    bool ok = data.accountExists(accountNumber);
    logStep(result, "Account existence (" + accountNumber + ")", ok);
    if (!ok) result.reason = Reason::ACCOUNT_NOT_FOUND;
    return ok;
}

// ---- RULE 1: Insufficient Funds ----
bool TransactionMiddleware::checkSufficientFunds(const Account& account, double amount, TransactionResult& result)
{
    bool ok = (account.balance >= amount);
    logStep(result, "Sufficient funds", ok);
    if (!ok) result.reason = Reason::INSUFFICIENT_FUNDS;
    return ok;
}

// ---- RULE 2: Rapid Withdrawals ----
// "If there are already 3 withdrawals for the account within the
// previous 10 seconds, block the new withdrawal." We look at recorded
// WITHDRAWAL transactions (of any status) and count how many happened
// within the configured time window, measured from right now.
int TransactionMiddleware::countRecentWithdrawals(const std::string& accountNumber) const
{
    std::time_t now = std::time(nullptr);
    int count = 0;

    for (const Transaction& t : data.getTransactions())
    {
        if (t.accountNumber != accountNumber) continue;
        if (t.type != TransactionType::WITHDRAWAL) continue;

        std::time_t transactionTime = parseTimestamp(t.timestamp);
        double secondsAgo = std::difftime(now, transactionTime);

        if (secondsAgo >= 0 && secondsAgo <= fraudConfig.timeWindowSeconds)
        {
            count++;
        }
    }
    return count;
}

bool TransactionMiddleware::checkRapidWithdrawals(const std::string& accountNumber, TransactionResult& result)
{
    int recentCount = countRecentWithdrawals(accountNumber);
    bool ok = (recentCount < fraudConfig.maxWithdrawals);
    logStep(result, "Rapid withdrawal check (" + std::to_string(recentCount) + " in last "
            + std::to_string(fraudConfig.timeWindowSeconds) + "s)", ok);
    if (!ok) result.reason = Reason::RAPID_WITHDRAWAL_ACTIVITY;
    return ok;
}

// ---- RULE 3: Unusual Spending Spike ----
// Average is calculated ONLY from past WITHDRAWAL and TRANSFER amounts
// (deposits do not reflect "spending behaviour"). If there is no
// spending history yet, the rule cannot apply, so it automatically
// passes rather than dividing by zero.
double TransactionMiddleware::calculateAverageTransactionAmount(const std::string& accountNumber) const
{
    double total = 0.0;
    int count = 0;

    for (const Transaction& t : data.getTransactions())
    {
        if (t.accountNumber != accountNumber) continue;
        if (t.status != TransactionStatus::APPROVED) continue; // only real spending counts
        if (t.type == TransactionType::WITHDRAWAL || t.type == TransactionType::TRANSFER)
        {
            total += t.amount;
            count++;
        }
    }

    if (count == 0) return 0.0; // no history yet - handled safely by the caller
    return total / count;
}

bool TransactionMiddleware::checkSpendingSpike(const std::string& accountNumber, double amount, TransactionResult& result)
{
    double average = calculateAverageTransactionAmount(accountNumber);

    if (average == 0.0)
    {
        // No spending history yet - nothing to compare against, so we
        // cannot call this "unusual". Let it pass.
        logStep(result, "Spending spike check (no history yet)", true);
        return true;
    }

    double threshold = average * fraudConfig.spendingSpikeMultiplier;
    bool ok = (amount <= threshold);
    logStep(result, "Spending spike check (avg R" + std::to_string(average)
            + ", threshold R" + std::to_string(threshold) + ")", ok);
    if (!ok) result.reason = Reason::UNUSUAL_SPENDING_SPIKE;
    return ok;
}

// =====================================================================
// MAIN ENTRY POINTS
// =====================================================================

TransactionResult TransactionMiddleware::processTransaction(const TransactionRequest& request, const User& authenticatedUser)
{
    TransactionResult result;
    std::cout << "\nProcessing transaction...\n";

    // Step 1 & 2: authentication is assumed already done by the caller
    // (you cannot reach this function without being logged in) - here
    // we verify OWNERSHIP of the account named in the request.
    if (!checkOwnership(request, authenticatedUser, result)) return result;

    // Step 3: the source account must exist (it always should, since it
    // belongs to the logged-in user, but we check defensively).
    if (!checkAccountExists(request.accountNumber, result)) return result;

    Account sourceAccount;
    data.findAccount(request.accountNumber, sourceAccount);
    result.balanceBefore = sourceAccount.balance;

    // For transfers, the destination account must also exist and must
    // not be the same as the source account.
    if (request.type == TransactionType::TRANSFER)
    {
        if (!checkAccountExists(request.destinationAccount, result)) return result;

        bool notSelfTransfer = (request.destinationAccount != request.accountNumber);
        logStep(result, "Not a self-transfer", notSelfTransfer);
        if (!notSelfTransfer)
        {
            result.reason = Reason::SELF_TRANSFER_NOT_ALLOWED;
            return result;
        }
    }

    // Step 4: amount must be a sensible positive number.
    if (!checkAmountValid(request.amount, result)) return result;

    // Step 5 (Rule 1): enough money to cover the request.
    if (!checkSufficientFunds(sourceAccount, request.amount, result)) return result;

    // Step 6 (Rule 2): only withdrawals are subject to the rapid-
    // withdrawal check (a transfer is not a cash withdrawal).
    if (request.type == TransactionType::WITHDRAWAL)
    {
        if (!checkRapidWithdrawals(request.accountNumber, result)) return result;
    }

    // Step 7 (Rule 3): compare against historical spending average.
    if (!checkSpendingSpike(request.accountNumber, request.amount, result)) return result;

    // ---- All rules passed: APPROVE and apply the change(s) ----
    result.approved = true;
    result.reason = Reason::NONE;

    double newSourceBalance = sourceAccount.balance - request.amount;
    data.updateAccountBalance(request.accountNumber, newSourceBalance);
    result.balanceAfter = newSourceBalance;

    if (request.type == TransactionType::TRANSFER)
    {
        Account destinationAccount;
        data.findAccount(request.destinationAccount, destinationAccount);
        data.updateAccountBalance(request.destinationAccount, destinationAccount.balance + request.amount);
    }

    Transaction record;
    record.id = data.nextTransactionId();
    record.accountNumber = request.accountNumber;
    record.type = request.type;
    record.amount = request.amount;
    record.status = TransactionStatus::APPROVED;
    record.reason = Reason::NONE;
    record.timestamp = currentTimestamp();
    record.destinationAccount = request.destinationAccount;
    data.addTransaction(record);
    result.transactionId = record.id;

    return result;
}

TransactionResult TransactionMiddleware::processDeposit(const TransactionRequest& request, const User& authenticatedUser)
{
    TransactionResult result;
    std::cout << "\nProcessing deposit...\n";

    if (!checkOwnership(request, authenticatedUser, result)) return result;
    if (!checkAmountValid(request.amount, result)) return result;

    Account account;
    data.findAccount(request.accountNumber, account);
    result.balanceBefore = account.balance;

    double newBalance = account.balance + request.amount;
    data.updateAccountBalance(request.accountNumber, newBalance);
    result.balanceAfter = newBalance;
    result.approved = true;
    result.reason = Reason::NONE;

    Transaction record;
    record.id = data.nextTransactionId();
    record.accountNumber = request.accountNumber;
    record.type = TransactionType::DEPOSIT;
    record.amount = request.amount;
    record.status = TransactionStatus::APPROVED;
    record.reason = Reason::NONE;
    record.timestamp = currentTimestamp();
    record.destinationAccount = "";
    data.addTransaction(record);
    result.transactionId = record.id;

    return result;
}

// =====================================================================
// WHY resultREASON ORDER MATTERS (see spec section 15):
// The rules run in a fixed order: ownership -> existence -> amount ->
// funds -> rapid withdrawal -> spending spike. Each check can stop the
// process early ("fail fast"), which is why checkLog only ever shows
// one [FAIL] line - everything after a failure is never evaluated,
// exactly like the demonstration transcript in the spec.
// =====================================================================

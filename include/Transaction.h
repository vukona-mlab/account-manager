#pragma once
// =====================================================================
// Transaction.h
// ---------------------------------------------------------------------
// A Transaction is a RECORD of something that was attempted - whether
// it was allowed to happen or not. We save every attempt (approved AND
// blocked) so the system is fully auditable (see NFR: Auditability).
// =====================================================================

#include <string>

// A plain "struct" is used here (rather than a class) because a
// Transaction is just a bundle of data with no behaviour of its own -
// it doesn't need private data or member functions.
struct Transaction
{
    int id = 0;                        // Unique transaction ID (auto-incremented)
    std::string accountNumber;         // Account that initiated the transaction
    std::string type;                  // DEPOSIT, WITHDRAWAL, or TRANSFER
    double amount = 0.0;               // Amount of money involved
    std::string status;                // APPROVED or BLOCKED
    std::string reason;                // Why it was blocked (NONE if approved)
    std::string timestamp;             // ISO-like timestamp, e.g. 2026-09-15T10:00:03
    std::string destinationAccount;    // Only set for TRANSFER, "" otherwise
};

// Using namespaces (instead of a C-style #define or a raw string
// scattered through the code) gives us named constants like
// TransactionType::DEPOSIT while still keeping things as plain strings,
// which is what gets stored in the JSON files.
namespace TransactionType
{
    const std::string DEPOSIT = "DEPOSIT";
    const std::string WITHDRAWAL = "WITHDRAWAL";
    const std::string TRANSFER = "TRANSFER";
}

namespace TransactionStatus
{
    const std::string APPROVED = "APPROVED";
    const std::string BLOCKED = "BLOCKED";
}

namespace Reason
{
    const std::string NONE = "NONE";
    const std::string INSUFFICIENT_FUNDS = "INSUFFICIENT_FUNDS";
    const std::string RAPID_WITHDRAWAL_ACTIVITY = "RAPID_WITHDRAWAL_ACTIVITY";
    const std::string UNUSUAL_SPENDING_SPIKE = "UNUSUAL_SPENDING_SPIKE";
    const std::string UNAUTHORIZED_ACCOUNT_ACCESS = "UNAUTHORIZED_ACCOUNT_ACCESS";
    const std::string INVALID_AMOUNT = "INVALID_AMOUNT";
    const std::string ACCOUNT_NOT_FOUND = "ACCOUNT_NOT_FOUND";
    const std::string SELF_TRANSFER_NOT_ALLOWED = "SELF_TRANSFER_NOT_ALLOWED";
}

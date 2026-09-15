#pragma once
// =====================================================================
// Account.h
// ---------------------------------------------------------------------
// Represents a bank account and its balance. Deliberately kept small:
// it does not know how to process transactions - that responsibility
// belongs to the TransactionMiddleware, not the data model.
// =====================================================================

#include <string>

struct Account
{
    std::string accountNumber;
    int ownerUserId = 0;   // links back to User::userId
    double balance = 0.0;
};

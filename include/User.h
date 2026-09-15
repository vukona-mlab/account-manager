#pragma once
// =====================================================================
// User.h
// ---------------------------------------------------------------------
// Represents a bank client who can log in. In a real bank, the PIN
// would be hashed and salted, and authentication would be far more
// complex. This is an EDUCATIONAL PROTOTYPE, so the PIN is stored and
// compared as plain text on purpose - simplicity over security.
// =====================================================================

#include <string>

struct User
{
    int userId = 0;
    std::string name;
    std::string accountNumber;   // the single account this user owns
    std::string pin;             // plain-text PIN, prototype only
};

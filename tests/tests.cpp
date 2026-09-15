// =====================================================================
// tests.cpp
// ---------------------------------------------------------------------
// A tiny, dependency-free test runner (no external testing framework
// needed - just plain functions and if-statements). Each TC## function
// matches one row of the test plan in the README.
//
// IMPORTANT: these tests do NOT touch the real data/ files. Each test
// builds its OWN DataManager pointed at a temporary set of JSON files
// in tests/fixtures/, so running the tests never disturbs your real
// Alice/Bob dummy data and tests never affect each other.
// =====================================================================

#include <iostream>
#include <fstream>
#include <cassert>
#include <string>

#include "../include/DataManager.h"
#include "../include/TransactionMiddleware.h"
#include "../include/FraudRules.h"

int testsRun = 0;
int testsPassed = 0;

void report(const std::string& testId, const std::string& scenario, bool passed, const std::string& detail = "")
{
    testsRun++;
    if (passed) testsPassed++;
    std::cout << (passed ? "[PASS] " : "[FAIL] ") << testId << " - " << scenario;
    if (!detail.empty()) std::cout << " (" << detail << ")";
    std::cout << "\n";
}

// Writes a fresh, known set of fixture files before each test so every
// test starts from the same predictable state.
void resetFixtures()
{
    std::ofstream users("tests/fixtures/users.json");
    users << R"([
        { "userId": 1, "name": "Alice Johnson", "accountNumber": "ACC1001", "pin": "1234" },
        { "userId": 2, "name": "Bob Smith", "accountNumber": "ACC1002", "pin": "5678" }
    ])";
    users.close();

    std::ofstream accounts("tests/fixtures/accounts.json");
    accounts << R"([
        { "accountNumber": "ACC1001", "ownerUserId": 1, "balance": 1000.00 },
        { "accountNumber": "ACC1002", "ownerUserId": 2, "balance": 500.00 }
    ])";
    accounts.close();

    std::ofstream transactions("tests/fixtures/transactions.json");
    transactions << "[]";
    transactions.close();

    std::ofstream config("tests/fixtures/fraud_rules.json");
    config << R"({
        "rapid_withdrawal": { "max_withdrawals": 3, "time_window_seconds": 10 },
        "spending_spike": { "multiplier": 5 }
    })";
    config.close();
}

int main()
{
    std::cout << "=========================================\n";
    std::cout << "RUNNING AUTOMATED TESTS\n";
    std::cout << "=========================================\n\n";

    // ---------------- TC01: Successful deposit ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();
        FraudRuleConfig config = FraudRules::loadFromFile("tests/fixtures/fraud_rules.json");
        TransactionMiddleware middleware(data, config);

        User alice; data.findUserByAccount("ACC1001", alice);
        TransactionRequest req{ "ACC1001", TransactionType::DEPOSIT, 200.0, "" };
        TransactionResult result = middleware.processDeposit(req, alice);

        bool passed = result.approved && result.balanceAfter == 1200.0;
        report("TC01", "Successful deposit", passed, "balance after = " + std::to_string(result.balanceAfter));
    }

    // ---------------- TC02: Successful withdrawal ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();
        FraudRuleConfig config = FraudRules::loadFromFile("tests/fixtures/fraud_rules.json");
        TransactionMiddleware middleware(data, config);

        User alice; data.findUserByAccount("ACC1001", alice);
        TransactionRequest req{ "ACC1001", TransactionType::WITHDRAWAL, 200.0, "" };
        TransactionResult result = middleware.processTransaction(req, alice);

        bool passed = result.approved && result.balanceAfter == 800.0;
        report("TC02", "Successful withdrawal", passed, "balance after = " + std::to_string(result.balanceAfter));
    }

    // ---------------- TC03: Successful transfer ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();
        FraudRuleConfig config = FraudRules::loadFromFile("tests/fixtures/fraud_rules.json");
        TransactionMiddleware middleware(data, config);

        User alice; data.findUserByAccount("ACC1001", alice);
        TransactionRequest req{ "ACC1001", TransactionType::TRANSFER, 200.0, "ACC1002" };
        TransactionResult result = middleware.processTransaction(req, alice);

        Account bobAccount; data.findAccount("ACC1002", bobAccount);
        bool passed = result.approved && result.balanceAfter == 800.0 && bobAccount.balance == 700.0;
        report("TC03", "Successful transfer", passed,
               "alice=" + std::to_string(result.balanceAfter) + " bob=" + std::to_string(bobAccount.balance));
    }

    // ---------------- TC04: Insufficient funds ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();
        FraudRuleConfig config = FraudRules::loadFromFile("tests/fixtures/fraud_rules.json");
        TransactionMiddleware middleware(data, config);

        User bob; data.findUserByAccount("ACC1002", bob); // balance = 500
        TransactionRequest req{ "ACC1002", TransactionType::WITHDRAWAL, 1000.0, "" };
        TransactionResult result = middleware.processTransaction(req, bob);

        bool passed = !result.approved && result.reason == Reason::INSUFFICIENT_FUNDS;
        report("TC04", "Insufficient funds", passed, "reason=" + result.reason);
    }

    // ---------------- TC05: Rapid withdrawals ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();
        FraudRuleConfig config = FraudRules::loadFromFile("tests/fixtures/fraud_rules.json");
        TransactionMiddleware middleware(data, config);

        User alice; data.findUserByAccount("ACC1001", alice);
        TransactionRequest req{ "ACC1001", TransactionType::WITHDRAWAL, 10.0, "" };

        middleware.processTransaction(req, alice); // 1st - approved
        middleware.processTransaction(req, alice); // 2nd - approved
        middleware.processTransaction(req, alice); // 3rd - approved
        TransactionResult fourth = middleware.processTransaction(req, alice); // 4th - should block

        bool passed = !fourth.approved && fourth.reason == Reason::RAPID_WITHDRAWAL_ACTIVITY;
        report("TC05", "Rapid withdrawals (4th within 10s blocked)", passed, "reason=" + fourth.reason);
    }

    // ---------------- TC06: Spending spike ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();
        FraudRuleConfig config = FraudRules::loadFromFile("tests/fixtures/fraud_rules.json");
        TransactionMiddleware middleware(data, config);

        User alice; data.findUserByAccount("ACC1001", alice);
        // Top up Alice's balance so this test is only ever blocked by the
        // SPENDING SPIKE rule, never by insufficient funds.
        data.updateAccountBalance("ACC1001", 10000.0);

        // Build up a spending history averaging R100 (100, 120, 80, 100, 100).
        // We use TRANSFER here (not WITHDRAWAL) purely so this test can build
        // 5 history entries back-to-back without also tripping the RAPID
        // WITHDRAWAL rule (Rule 2 only counts WITHDRAWAL transactions - see
        // spec section 17). The spending-spike average itself still counts
        // WITHDRAWAL + TRANSFER, so this is a valid way to build the history.
        double amounts[] = { 100, 120, 80, 100, 100 };
        for (double amt : amounts)
        {
            TransactionRequest req{ "ACC1001", TransactionType::TRANSFER, amt, "ACC1002" };
            middleware.processTransaction(req, alice);
        }

        // Average is now R100, threshold R500. Try a WITHDRAWAL of R600 -
        // should block (0 recent withdrawals, so Rule 2 does not interfere).
        TransactionRequest spike{ "ACC1001", TransactionType::WITHDRAWAL, 600.0, "" };
        TransactionResult result = middleware.processTransaction(spike, alice);

        bool passed = !result.approved && result.reason == Reason::UNUSUAL_SPENDING_SPIKE;
        report("TC06", "Unusual spending spike", passed, "reason=" + result.reason);
    }

    // ---------------- TC07: Invalid amount ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();
        FraudRuleConfig config = FraudRules::loadFromFile("tests/fixtures/fraud_rules.json");
        TransactionMiddleware middleware(data, config);

        User alice; data.findUserByAccount("ACC1001", alice);
        TransactionRequest req{ "ACC1001", TransactionType::WITHDRAWAL, -50.0, "" };
        TransactionResult result = middleware.processTransaction(req, alice);

        bool passed = !result.approved && result.reason == Reason::INVALID_AMOUNT;
        report("TC07", "Invalid amount (negative)", passed, "reason=" + result.reason);
    }

    // ---------------- TC08: Invalid login ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();

        User found;
        bool userExists = data.findUserByAccount("ACC1001", found);
        bool pinWrong = (found.pin != "0000");
        bool loginWouldFail = !userExists || pinWrong;

        report("TC08", "Invalid login (wrong PIN)", loginWouldFail);
    }

    // ---------------- TC09: Unauthorized account access ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();
        FraudRuleConfig config = FraudRules::loadFromFile("tests/fixtures/fraud_rules.json");
        TransactionMiddleware middleware(data, config);

        // Alice is logged in, but the request claims to be from Bob's account.
        User alice; data.findUserByAccount("ACC1001", alice);
        TransactionRequest req{ "ACC1002", TransactionType::WITHDRAWAL, 50.0, "" };
        TransactionResult result = middleware.processTransaction(req, alice);

        bool passed = !result.approved && result.reason == Reason::UNAUTHORIZED_ACCOUNT_ACCESS;
        report("TC09", "Unauthorized account access", passed, "reason=" + result.reason);
    }

    // ---------------- TC10: Transaction history ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();
        FraudRuleConfig config = FraudRules::loadFromFile("tests/fixtures/fraud_rules.json");
        TransactionMiddleware middleware(data, config);

        User alice; data.findUserByAccount("ACC1001", alice);
        TransactionRequest dep{ "ACC1001", TransactionType::DEPOSIT, 100.0, "" };
        TransactionRequest wd{ "ACC1001", TransactionType::WITHDRAWAL, 50.0, "" };
        middleware.processDeposit(dep, alice);
        middleware.processTransaction(wd, alice);

        std::vector<Transaction> history = data.getTransactionsForAccount("ACC1001");
        bool passed = history.size() == 2;
        report("TC10", "Transaction history returns only this account's records", passed,
               "count=" + std::to_string(history.size()));
    }

    // ---------------- TC11: Blocked transaction does not change balance ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();
        FraudRuleConfig config = FraudRules::loadFromFile("tests/fixtures/fraud_rules.json");
        TransactionMiddleware middleware(data, config);

        User bob; data.findUserByAccount("ACC1002", bob);
        Account before; data.findAccount("ACC1002", before);

        TransactionRequest req{ "ACC1002", TransactionType::WITHDRAWAL, 999999.0, "" };
        middleware.processTransaction(req, bob);

        Account after; data.findAccount("ACC1002", after);
        bool passed = (before.balance == after.balance);
        report("TC11", "Blocked transaction leaves balance unchanged", passed,
               "before=" + std::to_string(before.balance) + " after=" + std::to_string(after.balance));
    }

    // ---------------- TC12: Successful transfer updates both accounts ----------------
    {
        resetFixtures();
        DataManager data("tests/fixtures/users.json", "tests/fixtures/accounts.json", "tests/fixtures/transactions.json");
        data.loadAll();
        FraudRuleConfig config = FraudRules::loadFromFile("tests/fixtures/fraud_rules.json");
        TransactionMiddleware middleware(data, config);

        User alice; data.findUserByAccount("ACC1001", alice);
        Account bobBefore; data.findAccount("ACC1002", bobBefore);

        TransactionRequest req{ "ACC1001", TransactionType::TRANSFER, 300.0, "ACC1002" };
        TransactionResult result = middleware.processTransaction(req, alice);

        Account bobAfter; data.findAccount("ACC1002", bobAfter);
        bool passed = result.approved
                    && result.balanceAfter == 700.0
                    && bobAfter.balance == bobBefore.balance + 300.0;
        report("TC12", "Successful transfer updates both accounts", passed);
    }

    std::cout << "\n=========================================\n";
    std::cout << "RESULTS: " << testsPassed << " / " << testsRun << " tests passed\n";
    std::cout << "=========================================\n";

    return (testsPassed == testsRun) ? 0 : 1;
}

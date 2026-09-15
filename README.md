# Bank Transaction & Fraud Monitoring System (C++)

An educational, beginner-friendly **prototype** of a bank transaction and
fraud-monitoring system. **This is NOT a real banking application** — it
exists to demonstrate business analysis, transaction middleware, business
rules, basic authentication, fraud detection, data persistence, input
validation, testing, and clean, simple C++ architecture.

The most important part of this project is the **transaction
middleware / business-rule engine** (`TransactionMiddleware.cpp`). The
console UI (`main.cpp`) is deliberately kept "dumb" — it never makes a
business decision itself.

---

## 1. How to build and run

You need a C++17 compiler (e.g. `g++`). No external libraries are required.

```bash
cd BankTransactionSystem

# Build the interactive application
g++ -std=c++17 -Wall -Wextra -o bank_app src/main.cpp src/DataManager.cpp src/FraudRules.cpp src/TransactionMiddleware.cpp

# Run it (must be run from the BankTransactionSystem folder, so it can find data/ and config/)
./bank_app
```

```bash
# Build and run the automated tests
g++ -std=c++17 -Wall -Wextra -o run_tests tests/tests.cpp src/DataManager.cpp src/FraudRules.cpp src/TransactionMiddleware.cpp
mkdir -p tests/fixtures
./run_tests
```

### Dummy login credentials

| Client | Account  | PIN  | Starting Balance |
|--------|----------|------|-------------------|
| Alice Johnson | ACC1001 | 1234 | R10,000.00 |
| Bob Smith     | ACC1002 | 5678 | R5,000.00 |
| Carol Mokoena | ACC1003 | 4321 | R2,000.00 |

All of this is stored in `data/users.json` and `data/accounts.json` — edit
those files freely to set up your own test scenarios. Alice's
`data/transactions.json` history is pre-loaded with five withdrawals
(R100, R120, R80, R100, R100 — average R100) so you can immediately try the
spending-spike scenario (attempt R600 → blocked).

---

## 2. Why the code is organised this way

```
BankTransactionSystem/
├── data/                     Editable dummy data (loaded at startup)
│   ├── users.json
│   ├── accounts.json
│   └── transactions.json
├── config/
│   └── fraud_rules.json      Configurable fraud thresholds
├── include/                  Header files (the "shape" of each class)
│   ├── Json.h                 Tiny hand-written JSON reader/writer
│   ├── User.h
│   ├── Account.h
│   ├── Transaction.h
│   ├── FraudRules.h
│   ├── DataManager.h
│   └── TransactionMiddleware.h
├── src/                      Implementation (the "behaviour" of each class)
│   ├── main.cpp                UI / dashboard ONLY
│   ├── DataManager.cpp         Load/save JSON, lookups
│   ├── FraudRules.cpp          Load config/fraud_rules.json
│   └── TransactionMiddleware.cpp  ALL business rules live here
├── tests/
│   └── tests.cpp              12 automated test cases (TC01-TC12)
└── README.md
```

**No external JSON library is used.** This environment has no internet
access to download one, so `include/Json.h` contains a small, well-commented
hand-written JSON parser/writer built only for the shapes of data this
project needs (flat objects, arrays of flat objects, and one level of
nesting for the config file). It is not meant to be a general-purpose
library — just enough to keep the project dependency-free.

### The core architectural rule

```
Dashboard (main.cpp)
      |
      v
TransactionRequest  ------->  TransactionMiddleware  ------->  TransactionResult
                                     |
                                     +-- checkOwnership
                                     +-- checkAccountExists
                                     +-- checkAmountValid
                                     +-- checkSufficientFunds      (Rule 1)
                                     +-- checkRapidWithdrawals     (Rule 2)
                                     +-- checkSpendingSpike        (Rule 3)
```

The dashboard **never** writes an `if (balance < amount)` check itself. It
only builds a `TransactionRequest`, calls
`middleware.processTransaction(...)`, and prints whatever
`TransactionResult` comes back. This means:

- Every rule is defined **once**, in one place.
- Every rule is independently testable (see `tests/tests.cpp`) without
  needing a console or keyboard input.
- You can change a threshold in `config/fraud_rules.json` without touching
  any C++ code at all.

---

## 3. Business rules implemented

### Rule 1 — Insufficient Funds
A withdrawal or transfer is blocked if `amount > account.balance`.
Reason code: `INSUFFICIENT_FUNDS`.

### Rule 2 — Rapid Withdrawals
Implemented literally as specified: *"If there are already
`max_withdrawals` withdrawals for the account within the previous
`time_window_seconds` seconds, block the new withdrawal."*
The middleware looks at each historical `WITHDRAWAL` transaction's
timestamp and counts how many fall within the configured window measured
from the current moment — it does **not** simply count all withdrawals
ever made. Reason code: `RAPID_WITHDRAWAL_ACTIVITY`. Configured in
`config/fraud_rules.json` (`rapid_withdrawal.max_withdrawals`,
`rapid_withdrawal.time_window_seconds`).

### Rule 3 — Unusual Spending Spike
The account's historical average is calculated from **approved
`WITHDRAWAL` and `TRANSFER`** transactions only (deposits are excluded,
since they are not "spending"). If there is no spending history yet, the
rule cannot apply and is skipped safely (no divide-by-zero). If the new
amount exceeds `average * multiplier`, the transaction is blocked.
Reason code: `UNUSUAL_SPENDING_SPIKE`. Configured in
`config/fraud_rules.json` (`spending_spike.multiplier`).

### Authorization — Account Ownership
A logged-in client can only transact against their **own** account,
determined from their authenticated session — never from a value typed
at the keyboard. Reason code: `UNAUTHORIZED_ACCOUNT_ACCESS`.

### Rule execution order
1. Account ownership
2. Account existence (source, and destination for transfers)
3. Amount validation (`amount > 0`)
4. Sufficient funds
5. Rapid withdrawal check (withdrawals only)
6. Spending spike check
7. Approve → update balance(s) → save transaction record

The check stops at the first failed rule ("fail fast"), and every
attempted transaction — approved or blocked — is written to
`data/transactions.json` with a status and reason, so the system is fully
auditable.

---

## 4. Functional Requirements

| ID | Requirement |
|----|-------------|
| FR-001 | User authentication |
| FR-002 | Account ownership verification |
| FR-003 | View account balance |
| FR-004 | Deposit funds |
| FR-005 | Withdraw funds |
| FR-006 | Transfer funds |
| FR-007 | View transaction history |
| FR-008 | Validate sufficient funds |
| FR-009 | Detect rapid withdrawals |
| FR-010 | Detect unusual spending spikes |
| FR-011 | Block suspicious transactions |
| FR-012 | Record transaction outcomes |
| FR-013 | Protect users from accessing another user's account |

## 5. Non-functional requirements

| Quality | How it's addressed |
|---|---|
| **Usability** | Simple numbered menu, clear approve/block messages, formatted currency |
| **Maintainability** | Business rules live only in `TransactionMiddleware.cpp`, fully separated from `main.cpp` |
| **Reliability** | A blocked transaction never calls `updateAccountBalance` — verified by TC11 |
| **Auditability** | Every approved or blocked attempt is saved with a status and reason |
| **Configurability** | Fraud thresholds are loaded from `config/fraud_rules.json`, not hardcoded |

---

## 6. Test cases

Run `./run_tests` to execute all of these automatically. Each test uses
its own temporary fixture files under `tests/fixtures/`, so running tests
never touches your real `data/` folder.

| Test ID | Scenario | Input | Expected Result |
|---|---|---|---|
| TC01 | Successful deposit | Deposit R200 to a R1000 balance | Balance becomes R1200, status APPROVED |
| TC02 | Successful withdrawal | Withdraw R200 from R1000 | Balance becomes R800, status APPROVED |
| TC03 | Successful transfer | Transfer R200 Alice→Bob | Alice -R200, Bob +R200, both APPROVED |
| TC04 | Insufficient funds | Withdraw R1000 from a R500 balance | BLOCKED, reason INSUFFICIENT_FUNDS |
| TC05 | Rapid withdrawals | 4 withdrawals back-to-back | 4th is BLOCKED, reason RAPID_WITHDRAWAL_ACTIVITY |
| TC06 | Spending spike | History averages R100, attempt R600 | BLOCKED, reason UNUSUAL_SPENDING_SPIKE |
| TC07 | Invalid amount | Withdraw -R50 | BLOCKED, reason INVALID_AMOUNT |
| TC08 | Invalid login | Wrong PIN for ACC1001 | Login fails |
| TC09 | Unauthorized account access | Alice's session, request against ACC1002 | BLOCKED, reason UNAUTHORIZED_ACCOUNT_ACCESS |
| TC10 | Transaction history | 2 transactions on one account | History returns exactly those 2 records |
| TC11 | Blocked transaction does not change balance | Withdraw far more than balance | Balance before == balance after |
| TC12 | Successful transfer updates both accounts | Transfer R300 | Source -R300, destination +R300 |

Actual results from the last run of `run_tests` (all 12/12 passing):

```
[PASS] TC01 - Successful deposit
[PASS] TC02 - Successful withdrawal
[PASS] TC03 - Successful transfer
[PASS] TC04 - Insufficient funds
[PASS] TC05 - Rapid withdrawals
[PASS] TC06 - Unusual spending spike
[PASS] TC07 - Invalid amount
[PASS] TC08 - Invalid login
[PASS] TC09 - Unauthorized account access
[PASS] TC10 - Transaction history
[PASS] TC11 - Blocked transaction does not change balance
[PASS] TC12 - Successful transfer updates both accounts

RESULTS: 12 / 12 tests passed
```

---

## 7. Demonstration walkthrough

1. Run `./bank_app`.
2. Log in as Alice: `ACC1001` / `1234`.
3. Choose **1. Deposit Money**, deposit `2000` → balance rises to R12,000.
4. Choose **2. Withdraw Money**, withdraw `500` → APPROVED, balance R11,500.
5. Attempt to withdraw `50000` → BLOCKED, `INSUFFICIENT_FUNDS`, balance
   unchanged.
6. Withdraw `10` three times in a row → all APPROVED.
7. Withdraw `10` a fourth time within 10 seconds → BLOCKED,
   `RAPID_WITHDRAWAL_ACTIVITY`.
8. Log in fresh (or use Carol/Bob) and attempt a withdrawal far above your
   historical average (e.g. Alice's pre-loaded history averages R100 — try
   `600`) → BLOCKED, `UNUSUAL_SPENDING_SPIKE`.
9. Choose **4. View Transaction History** to see every approved and
   blocked attempt with reasons.
10. Choose **6. Logout**.

---

## 8. Known simplifications (by design, per project scope)

- PINs are stored and compared in plain text — a real system would hash
  and salt them. This is explicitly out of scope (see prompt section 3).
- There is no session token or timeout — "logged in" simply means "we
  have a `User` object in memory for the current loop iteration."
- The custom JSON reader/writer in `Json.h` only supports the exact
  shapes of data this project produces; it is not a general-purpose
  library.
- The rapid-withdrawal window is measured from *the current wall-clock
  time* back to each transaction's saved timestamp, using `<ctime>`.

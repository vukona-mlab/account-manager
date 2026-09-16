Banck account manager 

Creating a system that manages bank account holders ,  like transferring money , withdraw ,deposit and also detect suspicious activity or fraud.

REQUIREMENTS
Bank account holders
Money transfer
Money withdrawal
Money transaction limit
Money into the  account

Step1.Create User Profile
Assign each user a unique account number and username and password (e,g ALICE, BOB)
.Store and update the users account balance
.keep a record of all past transactions

Step2.Validate Transactions
.Before depositing ,withdrawing or transferring money user have to check first the account before	 validating a transaction

Step3.Check Available balance
.The system display the users current  balance
.For withdrawal and transfers available balance must be greater than or equal to the amount being moved.

e.g If Alice has R200 and tries to withdraw R400 the transaction must decline

Step4.Monitor Rapid Withdrawals 
.System keeps track of withdrawal times.

.If user tries to withdraw more than 3 times within 10 seconds , the system will detect some fraudulent transactions or suspicious transactions and block the transaction.


Step5.Detect  Unusual Spending

.The system monitors users average  transaction amount 
.If ALICE normally transfers 200 and suddenly wants to transfer 1000
The system will recognize this very different from her normal behavior and flag/blog it according to the system rule 

Step6. Enforce Daily limit
.Set a daily spending limit of R5000
.If the user attempts to spend more than R5000 in a day the transaction will decline 
.User can not make more than 3 transactions that exceed the daily amount  which is R5000

Step7.Handle Deposits 
.Deposited funds added  to the available balance
.Transaction history is updated with each deposit.




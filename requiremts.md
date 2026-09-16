#Bank management system-requirements

# create a bank account management system that allowers users to deposit,withdraw and transfer money while detecting suspicious or fraudulent activities.

#DEPOSIT
user must be able to deposit money into their account and see the balance increases

#WITHDRAW MONEY 
User must be able to withdraw money from their account .
user can not withdraw more money than their available balance 

#TRANSFER
User must be able to transfer money to another user account
the user must have enough funds to make any transfer
the sender balance must decrease after the transfer
the receiver balance must increase

#FRAUD DETECTION

#Insuffient funds
Reject a withdrawal or transfer when the user does not have enough money on the account.

#rapid withdrawl
if a user attempts more than 3 withdrawal within 10 seconds, the system must block suspicious activity

#Unusual spednding
The system must keep track of users average trasaction size.
if a new transaction is significantly larger than the users normal transaction amount, the system must flag or block

#TESTING REQUIREMENTS
successful deposit
successful withdrawl
insufficient funds
successful transfer 
transfer with insufficient  funds 
rapid withdrwals
unusual transaction amount
normal transactions




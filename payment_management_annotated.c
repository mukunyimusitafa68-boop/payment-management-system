/*
 * PAYMENT MANAGEMENT SYSTEM — ANNOTATED LEARNING VERSION
 * ------------------------------------------------------------------
 * Purpose:
 *   A console-based C program for managing customer accounts and recording
 *   deposits, withdrawals, transfers, account approvals and notifications.
 *
 * Storage:
 *   The program stores records in three CSV (comma-separated values) files:
 *     accounts.csv       - account details and current balances
 *     transactions.csv   - transaction history
 *     notifications.csv  - simple system messages
 *
 * Compile with GCC:
 *   gcc payment_management.c -o payment_management
 * Run on Windows:
 *   payment_management.exe
 *
 * Demonstration administrator credentials:
 *   username: admin
 *   password: admin123
 *
 * IMPORTANT LIMITATIONS:
 *   This is a student demonstration, not a real banking system. Customer
 *   passwords are stored as plain text, the admin password is hard-coded,
 *   some operations do not authenticate the customer, and CSV writes are
 *   not one atomic transaction. Do not use it to handle real money.
 *
 * Learning tip:
 *   Read main() first to understand the program's overall flow, then follow
 *   the function calls into the customer and administrator menus.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>   // Provides isspace() and other character-checking functions.

/*
 * HEADER FILES:
 * stdio.h  -> printf(), fgets(), fopen(), fprintf(), fclose(), etc.
 * stdlib.h -> strtol(), strtod(), atof(), atoi(), system(), etc.
 * string.h -> strcmp(), strlen(), strchr(), strtok(), snprintf(), etc.
 * time.h   -> time and date functions used for transaction timestamps.
 * ctype.h  -> isspace(), used to ignore spaces after a typed number.
 */

#define ACCOUNTS_FILE "accounts.csv"
#define TRANSACTIONS_FILE "transactions.csv"
#define NOTIFICATIONS_FILE "notifications.csv"
#define MAX_ACCOUNTS 1000
#define MAX_LINE 2048

/*
 * #define creates named constants. Using these names makes the code easier
 * to maintain: change the value here instead of hunting through the program.
 */

/*
 * STRUCTURE:
 * A structure groups related fields together. One Account object represents
 * one customer account. Character arrays store text; balance stores money;
 * approved and active are integer flags (1 = yes, 0 = no).
 */
typedef struct {
    char accountNo[24];
    char firstName[50];
    char lastName[50];
    char email[100];
    char password[100];
    double balance;
    int approved;
    int active;
} Account;

/*
 * GLOBAL DATA:
 * accounts[] is the in-memory list of accounts loaded from accounts.csv.
 * accountCount says how many array elements are currently used.
 * transactionCounter creates transaction labels such as TX1, TX2, and so on.
 * These variables are static at file scope, so other source files cannot
 * access them directly. The transaction counter resets when the program restarts.
 */
static Account accounts[MAX_ACCOUNTS];
static int accountCount = 0;
static unsigned long transactionCounter = 1;

/* Remove the Enter/newline characters that fgets() may keep at the end of input. */
void trimNewline(char *s) { s[strcspn(s, "\r\n")] = '\0'; }
/* Clear the console. Windows uses "cls"; Linux/macOS use "clear". */
void clearScreen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}
/* Wait for the user to press Enter so the result can be read before a menu returns. */
void pauseScreen(void) {
    char temp[8];
    printf("\nPress ENTER to return to the menu...");
    (void)fgets(temp, sizeof temp, stdin);
}
/* Display a prompt and safely read one line into the caller-provided character array. */
void readLine(const char *prompt, char *out, size_t size) {
    printf("%s", prompt);
    if (!fgets(out, (int)size, stdin)) { out[0] = '\0'; return; }
    trimNewline(out);
}
/* Keep asking until the user types a whole number with no extra non-space characters. */
int readInt(const char *prompt) {
    char buf[80], *end; long value;
    for (;;) {
        readLine(prompt, buf, sizeof buf);
        value = strtol(buf, &end, 10);
        while (isspace((unsigned char)*end)) end++;
        if (buf[0] && *end == '\0') return (int)value;
        puts("Invalid number. Try again.");
    }
}
/* Read a positive amount. Note: this demo should be strengthened to reject infinity/NaN explicitly. */
double readAmount(const char *prompt) {
    char buf[100], *end; double value;
    for (;;) {
        readLine(prompt, buf, sizeof buf);
        value = strtod(buf, &end);
        while (isspace((unsigned char)*end)) end++;
        if (buf[0] && *end == '\0' && value > 0.0) return value;
        puts("Enter a valid amount greater than zero.");
    }
}
/* Format the current local date and time as YYYY-MM-DD HH:MM:SS. */
void timestamp(char *out, size_t size) {
    time_t now = time(NULL); struct tm *t = localtime(&now);
    if (t) strftime(out, size, "%Y-%m-%d %H:%M:%S", t);
    else snprintf(out, size, "unknown-time");
}
/* Append one notification row to notifications.csv; "a" means append mode. */
void addNotification(const char *message) {
    FILE *f = fopen(NOTIFICATIONS_FILE, "a"); char ts[32];
    if (!f) { puts("Warning: could not write notifications.csv"); return; }
    timestamp(ts, sizeof ts);
    /* Store date/time, message and a simple UNREAD status as one CSV row. */
    fprintf(f, "\"%s\",\"%s\",\"%s\"\n", ts, message, "UNREAD");
    fclose(f);
}
/* Append one transaction row. A transfer calls this twice: once for each side. */
void recordTransaction(const char *type, const char *from, const char *to,
                       double amount, const char *status, const char *note) {
    FILE *f = fopen(TRANSACTIONS_FILE, "a"); char ts[32];
    if (!f) { puts("ERROR: Could not save transaction to transactions.csv"); return; }
    timestamp(ts, sizeof ts);
    /* %lu prints the counter as an unsigned long; %.2f prints two decimal places. */
    fprintf(f, "\"TX%lu\",\"%s\",\"%s\",\"%s\",%.2f,\"%s\",\"%s\",\"%s\"\n",
            transactionCounter++, ts, type, from, amount, to, status, note);
    fclose(f);
}
/* Read account rows from CSV into the accounts[] array when the program starts. */
void loadAccounts(void) {
    FILE *f = fopen(ACCOUNTS_FILE, "r"); char line[MAX_LINE];
    accountCount = 0;
    if (!f) return;  // No account file yet: keep the list empty.
    /* Read one complete text line at a time, stopping at EOF or array capacity. */
    while (fgets(line, sizeof line, f) && accountCount < MAX_ACCOUNTS) {
        Account a; char *p;
        /* CSV fields are quoted by this program; commas in user input are rejected. */
        char *fields[8]; int n = 0;
        /* strtok() splits the line at commas/newlines. This is a simple CSV reader, not a full CSV parser. */
        p = strtok(line, ",\r\n");
        while (p && n < 8) { fields[n++] = p; p = strtok(NULL, ",\r\n"); }
        if (n != 8 || strcmp(fields[0], "accountNo") == 0) continue;
        for (int i = 0; i < n; i++) {
            size_t len = strlen(fields[i]);
            if (len >= 2 && fields[i][0] == '"' && fields[i][len-1] == '"') {
                fields[i][len-1] = '\0'; fields[i]++;
            }
        }
        snprintf(a.accountNo, sizeof a.accountNo, "%s", fields[0]);
        snprintf(a.firstName, sizeof a.firstName, "%s", fields[1]);
        snprintf(a.lastName, sizeof a.lastName, "%s", fields[2]);
        snprintf(a.email, sizeof a.email, "%s", fields[3]);
        snprintf(a.password, sizeof a.password, "%s", fields[4]);
        a.balance = atof(fields[5]); a.approved = atoi(fields[6]); a.active = atoi(fields[7]);
        /* Store the loaded account in memory and increase the used array count. */
        accounts[accountCount++] = a;
    }
    fclose(f);
}
/* Rewrite accounts.csv from the current in-memory accounts array. Returns 1 on success, 0 on failure. */
int saveAccounts(void) {
    FILE *f = fopen(ACCOUNTS_FILE, "w");
    if (!f) { puts("ERROR: Cannot save accounts.csv"); return 0; }
    /* First write the column names, then write every account row below. */
    fprintf(f, "accountNo,firstName,lastName,email,password,balance,approved,active\n");
    for (int i = 0; i < accountCount; i++) {
        Account *a = &accounts[i];
        fprintf(f, "\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%.2f,%d,%d\n",
                a->accountNo, a->firstName, a->lastName, a->email,
                a->password, a->balance, a->approved, a->active);
    }
    fclose(f); return 1;
}
/* Create each CSV file and its header row if that file does not already exist. */
void ensureFiles(void) {
    FILE *f;
    f = fopen(ACCOUNTS_FILE, "r");
    if (!f) { f = fopen(ACCOUNTS_FILE, "w"); if (f) { fprintf(f,"accountNo,firstName,lastName,email,password,balance,approved,active\n"); fclose(f); } }
    else fclose(f);
    f = fopen(TRANSACTIONS_FILE, "r");
    if (!f) { f = fopen(TRANSACTIONS_FILE, "w"); if (f) { fprintf(f,"transactionId,dateTime,type,fromAccount,amount,toAccount,status,note\n"); fclose(f); } }
    else fclose(f);
    f = fopen(NOTIFICATIONS_FILE, "r");
    if (!f) { f = fopen(NOTIFICATIONS_FILE, "w"); if (f) { fprintf(f,"dateTime,message,status\n"); fclose(f); } }
    else fclose(f);
}
/* Return an active account index, or -1 if no active matching account exists. */
int findAccount(const char *accountNo) {
    for (int i = 0; i < accountCount; i++)
        if (accounts[i].active && strcmp(accounts[i].accountNo, accountNo) == 0) return i;
    return -1;
}
/* Basic CSV-safety check: reject empty text and commas/quotes/newlines. */
int validSimpleText(const char *s) {
    if (!s[0]) return 0;
    for (size_t i = 0; s[i]; i++) if (s[i] == ',' || s[i] == '"' || s[i] == '\n') return 0;
    return 1;
}
/* Choose the next account number, starting at 10000 or one above the current maximum. */
void makeAccountNo(char *out, size_t size) {
    unsigned long max = 10000;
    for (int i = 0; i < accountCount; i++) {
        unsigned long v = strtoul(accounts[i].accountNo, NULL, 10);
        if (v >= max) max = v + 1;
    }
    snprintf(out, size, "%lu", max);
}
/* Collect customer details, initialise an account, save it, and create a notification. */
int createAccount(int approvedByAdmin) {
    Account a;
    if (accountCount >= MAX_ACCOUNTS) { puts("Account limit reached."); return 0; }
    memset(&a, 0, sizeof a); makeAccountNo(a.accountNo, sizeof a.accountNo);
    puts("\n--- CREATE CUSTOMER ACCOUNT ---");
    for (;;) { readLine("First name: ", a.firstName, sizeof a.firstName); if (validSimpleText(a.firstName)) break; puts("Required; commas and quotation marks are not allowed."); }
    for (;;) { readLine("Last name: ", a.lastName, sizeof a.lastName); if (validSimpleText(a.lastName)) break; puts("Required; commas and quotation marks are not allowed."); }
    for (;;) {
        readLine("Email: ", a.email, sizeof a.email);
        if (strchr(a.email, '@') && strchr(a.email, '.') && validSimpleText(a.email)) break;
        puts("Enter a valid email address.");
    }
    for (;;) { readLine("Password: ", a.password, sizeof a.password); if (strlen(a.password) >= 4 && validSimpleText(a.password)) break; puts("Use at least 4 characters; commas and quotation marks are not allowed."); }
    /* New accounts start with zero balance. Admin-created accounts are approved immediately. */
    a.balance = 0.0; a.approved = approvedByAdmin ? 1 : 0; a.active = 1;
    accounts[accountCount++] = a;
    /* If saving fails, undo the in-memory count change so it is not treated as saved. */
    if (!saveAccounts()) { accountCount--; return 0; }
    char msg[256]; snprintf(msg, sizeof msg, "New account %s created for %s %s; %s", a.accountNo, a.firstName, a.lastName, a.approved ? "approved" : "pending approval");
    addNotification(msg);
    printf("\nAccount created successfully.\nAccount number: %s\nStatus: %s\n", a.accountNo, a.approved ? "APPROVED" : "PENDING APPROVAL");
    if (!a.approved) puts("Wait for an administrator to approve your account before making transactions.");
    return 1;
}
/* Shared login helper: find account, compare password, and check approval status. */
int authenticateCustomer(int *idx) {
    char number[40], password[100];
    readLine("Account number: ", number, sizeof number);
    *idx = findAccount(number);
    if (*idx < 0) { puts("Account not found or deleted."); return 0; }
    readLine("Password: ", password, sizeof password);
    if (strcmp(password, accounts[*idx].password) != 0) { puts("Incorrect password."); return 0; }
    if (!accounts[*idx].approved) { puts("This account is awaiting administrator approval."); return 0; }
    return 1;
}
/* Display a receipt on screen; this function does not save a receipt file. */
void printReceipt(const char *type, const char *from, const char *to, double amount, const char *status) {
    char ts[32]; timestamp(ts, sizeof ts);
    puts("\n========== TRANSACTION RECEIPT ==========");
    printf("Date/time : %s\nType      : %s\nFrom      : %s\n", ts, type, from[0] ? from : "N/A");
    if (to && to[0]) printf("To        : %s\n", to);
    printf("Amount    : %.2f\nStatus    : %s\n", amount, status);
    puts("=========================================");
}
/* Increase an approved account balance and record a successful deposit. */
void depositMoney(void) {
    char no[40]; double amount; int i;
    readLine("Account number: ", no, sizeof no); i = findAccount(no);
    if (i < 0) { puts("Account not found."); return; }
    if (!accounts[i].approved) { puts("Account is not approved."); return; }
    /* Add the deposit to the account balance in memory. */
    amount = readAmount("Amount to deposit: "); accounts[i].balance += amount;
    /* Save the new balance. If saving fails, restore the old balance in memory. */
    if (!saveAccounts()) { accounts[i].balance -= amount; return; }
    recordTransaction("DEPOSIT", no, "", amount, "SUCCESS", "Cash deposit");
    char msg[180]; snprintf(msg, sizeof msg, "Deposit of %.2f to account %s", amount, no); addNotification(msg);
    printReceipt("DEPOSIT", no, "", amount, "SUCCESS"); printf("New balance: %.2f\n", accounts[i].balance);
}
/* Authenticate, check funds, reduce the balance, then save and record withdrawal. */
void withdrawMoney(void) {
    int i; double amount; char no[40], pass[100];
    readLine("Account number: ", no, sizeof no); i = findAccount(no);
    if (i < 0) { puts("Account not found."); return; }
    if (!accounts[i].approved) { puts("Account is not approved."); return; }
    readLine("Password: ", pass, sizeof pass);
    if (strcmp(pass, accounts[i].password) != 0) { puts("Incorrect password."); return; }
    amount = readAmount("Amount to withdraw: ");
    /* Never allow a withdrawal greater than the available balance. */
    if (amount > accounts[i].balance) { puts("Insufficient funds. Withdrawal cancelled."); return; }
    accounts[i].balance -= amount;
    if (!saveAccounts()) { accounts[i].balance += amount; return; }
    recordTransaction("WITHDRAWAL", no, "", amount, "SUCCESS", "Cash withdrawal");
    char msg[180]; snprintf(msg, sizeof msg, "Withdrawal of %.2f from account %s", amount, no); addNotification(msg);
    printReceipt("WITHDRAWAL", no, "", amount, "SUCCESS"); printf("New balance: %.2f\n", accounts[i].balance);
}
/* Move money from one approved account to another and log outgoing/incoming rows. */
void transferMoney(void) {
    char fromNo[40], toNo[40], pass[100]; int from, to; double amount;
    readLine("Your account number: ", fromNo, sizeof fromNo); from = findAccount(fromNo);
    if (from < 0) { puts("Source account not found."); return; }
    if (!accounts[from].approved) { puts("Source account is not approved."); return; }
    readLine("Password: ", pass, sizeof pass);
    if (strcmp(pass, accounts[from].password) != 0) { puts("Incorrect password."); return; }
    readLine("Recipient account number: ", toNo, sizeof toNo); to = findAccount(toNo);
    if (to < 0 || !accounts[to].approved) { puts("Recipient account not found or not approved."); return; }
    if (from == to) { puts("You cannot transfer money to the same account."); return; }
    amount = readAmount("Amount to transfer: ");
    if (amount > accounts[from].balance) { puts("Insufficient funds. Transfer cancelled."); return; }
    /* Transfer logic: subtract from sender and add to recipient. */
    accounts[from].balance -= amount; accounts[to].balance += amount;
    if (!saveAccounts()) { accounts[from].balance += amount; accounts[to].balance -= amount; return; }
    /* Save two related rows so each account has a side of the transfer in its history. */
    recordTransaction("TRANSFER_OUT", fromNo, toNo, amount, "SUCCESS", "Account transfer sent");
    recordTransaction("TRANSFER_IN", toNo, fromNo, amount, "SUCCESS", "Account transfer received");
    char msg[200]; snprintf(msg, sizeof msg, "Transfer of %.2f from %s to %s", amount, fromNo, toNo); addNotification(msg);
    printReceipt("TRANSFER", fromNo, toNo, amount, "SUCCESS"); printf("Your new balance: %.2f\n", accounts[from].balance);
}
/* Verify account password and approval before displaying the balance. */
void customerBalance(void) {
    int i; char no[40], pass[100];
    readLine("Account number: ", no, sizeof no); i = findAccount(no);
    if (i < 0) { puts("Account not found."); return; }
    readLine("Password: ", pass, sizeof pass);
    if (strcmp(pass, accounts[i].password) != 0) { puts("Incorrect password."); return; }
    if (!accounts[i].approved) { puts("Account is pending approval."); return; }
    printf("Account holder: %s %s\nAccount number: %s\nBalance: %.2f\n", accounts[i].firstName, accounts[i].lastName, accounts[i].accountNo, accounts[i].balance);
}
/* Print transaction-file lines containing the requested account number. */
void customerStatement(void) {
    char no[40], line[MAX_LINE]; int found = 0;
    readLine("Account number for statement: ", no, sizeof no);
    int i = findAccount(no); if (i < 0) { puts("Account not found."); return; }
    FILE *f = fopen(TRANSACTIONS_FILE, "r");
    if (!f) { puts("Cannot open transactions file."); return; }
    puts("\n--- TRANSACTION STATEMENT (account matched in from/to columns) ---");
    puts("Transaction records are shown below:");
    while (fgets(line, sizeof line, f)) {
        if (strstr(line, no)) { fputs(line, stdout); found = 1; }
    }
    fclose(f); if (!found) puts("No transactions found for this account.");
}
/* Repeated customer submenu. Each chosen operation is shown on a cleared screen. */
void customerMenu(void) {
    int choice, idx;
    for (;;) {
        clearScreen();  // Remove the previous operation from view before showing this menu.
        puts("\n========== CUSTOMER MENU ==========");
        puts("1. Create account\n2. Deposit money\n3. Withdraw money\n4. Transfer money\n5. Check balance\n6. Print/view statement\n7. Logout");
        choice = readInt("Choose an option: ");
        if (choice == 7) { clearScreen(); puts("Customer menu logged out."); pauseScreen(); return; }
        clearScreen();  // Show only the selected operation, not the menu behind it.
        switch (choice) {
            case 1: createAccount(0); break;
            case 2: depositMoney(); break;
            case 3: withdrawMoney(); break;
            case 4: transferMoney(); break;
            case 5: customerBalance(); break;
            case 6: customerStatement(); break;
            default: puts("Invalid choice.");
        }
        pauseScreen();
    }
}
/* Check credentials against the demonstration credentials hard-coded in the source. */
int adminLogin(void) {
    char user[80], pass[100];
    puts("\n--- ADMIN LOGIN ---"); readLine("Username: ", user, sizeof user); readLine("Password: ", pass, sizeof pass);
    if (strcmp(user, "admin") == 0 && strcmp(pass, "admin123") == 0) { puts("Login successful."); return 1; }
    puts("Invalid administrator credentials."); return 0;
}
/* Display active account summaries and then show full names separately. */
void viewAccounts(void) {
    puts("\n--- ALL ACCOUNTS ---");
    printf("%-12s %-18s %-25s %-12s %-12s\n", "Account", "Customer", "Email", "Balance", "Status");
    for (int i = 0; i < accountCount; i++) if (accounts[i].active)
        printf("%-12s %-18s %-25s %-12.2f %-12s\n", accounts[i].accountNo,
               "", accounts[i].email, accounts[i].balance, accounts[i].approved ? "APPROVED" : "PENDING");
    /* Print names separately to avoid truncation in narrow columns. */
    for (int i = 0; i < accountCount; i++) if (accounts[i].active)
        printf("%s: %s %s | %s\n", accounts[i].accountNo, accounts[i].firstName, accounts[i].lastName, accounts[i].approved ? "APPROVED" : "PENDING APPROVAL");
}
/* List pending accounts and let the administrator approve one account. */
void approveAccounts(void) {
    int pending = 0;
    puts("\n--- PENDING ACCOUNTS ---");
    for (int i = 0; i < accountCount; i++) if (accounts[i].active && !accounts[i].approved) {
        printf("%s | %s %s | %s\n", accounts[i].accountNo, accounts[i].firstName, accounts[i].lastName, accounts[i].email); pending++;
    }
    if (!pending) { puts("No pending accounts."); return; }
    char no[40]; readLine("Enter account number to approve (or 0 to cancel): ", no, sizeof no);
    if (strcmp(no, "0") == 0) return;
    int i = findAccount(no);
    if (i < 0 || accounts[i].approved) { puts("Account not found or already approved."); return; }
    accounts[i].approved = 1;
    if (!saveAccounts()) { accounts[i].approved = 0; return; }
    char msg[180]; snprintf(msg, sizeof msg, "Account %s approved for %s %s", no, accounts[i].firstName, accounts[i].lastName); addNotification(msg);
    puts("Account approved successfully.");
}
/* Soft-delete an account by setting active = 0; historical transactions remain. */
void deleteAccount(void) {
    char no[40], confirm[20]; readLine("Account number to delete: ", no, sizeof no); int i = findAccount(no);
    if (i < 0) { puts("Account not found."); return; }
    printf("Delete account %s (%s %s)? This marks it inactive but keeps old transactions.\n", no, accounts[i].firstName, accounts[i].lastName);
    readLine("Type YES to confirm: ", confirm, sizeof confirm);
    if (strcmp(confirm, "YES") != 0) { puts("Deletion cancelled."); return; }
    accounts[i].active = 0;
    if (!saveAccounts()) { accounts[i].active = 1; return; }
    char msg[160]; snprintf(msg, sizeof msg, "Account %s was deleted/deactivated", no); addNotification(msg); puts("Account deactivated.");
}
/* Read and display all lines in notifications.csv. */
void viewNotifications(void) {
    FILE *f = fopen(NOTIFICATIONS_FILE, "r"); char line[MAX_LINE];
    if (!f) { puts("Cannot open notifications.csv"); return; }
    puts("\n--- NOTIFICATIONS ---"); while (fgets(line, sizeof line, f)) fputs(line, stdout); fclose(f);
}
/* Show one active account's current balance and matching transaction rows. */
void adminStatement(void) {
    char no[40], line[MAX_LINE]; int found = 0;
    readLine("Customer account number: ", no, sizeof no); int i = findAccount(no);
    if (i < 0) { puts("Active account not found."); return; }
    printf("Statement for %s %s | Account %s | Current balance %.2f\n", accounts[i].firstName, accounts[i].lastName, no, accounts[i].balance);
    FILE *f = fopen(TRANSACTIONS_FILE, "r"); if (!f) { puts("Cannot open transactions.csv"); return; }
    while (fgets(line, sizeof line, f)) if (strstr(line, no)) { fputs(line, stdout); found = 1; }
    fclose(f); if (!found) puts("No transaction records found.");
}
/* Repeated administrator submenu for approvals, reports, accounts and logout. */
void adminMenu(void) {
    int c;
    for (;;) {
        clearScreen();
        puts("\n========== ADMINISTRATOR MENU ==========");
        puts("1. Create approved account\n2. Approve pending accounts\n3. View notifications\n4. View all accounts\n5. Delete/deactivate account\n6. View customer balance/statement\n7. Logout");
        c = readInt("Choose an option: ");
        if (c == 7) { clearScreen(); puts("Administrator logged out."); pauseScreen(); return; }
        clearScreen();
        switch (c) {
            case 1: createAccount(1); break;
            case 2: approveAccounts(); break;
            case 3: viewNotifications(); break;
            case 4: viewAccounts(); break;
            case 5: deleteAccount(); break;
            case 6: adminStatement(); break;
            default: puts("Invalid choice.");
        }
        pauseScreen();
    }
}
/* PROGRAM ENTRY POINT: prepare CSV files, load accounts, and show the main menu loop. */
int main(void) {
    int choice;
    /* Ensure storage exists first, then load its saved accounts into memory. */
    ensureFiles(); loadAccounts();
    puts("==========================================");
    puts("       PAYMENT MANAGEMENT SYSTEM          ");
    puts("==========================================");
    for (;;) {
        clearScreen();
        puts("\nMAIN MENU\n1. Customer services\n2. Administrator login\n3. Exit");
        choice = readInt("Choose an option: ");
        if (choice == 3) { clearScreen(); puts("Thank you for using the Payment Management System."); return 0; }
        clearScreen();
        /* Dispatch (route) the chosen number to the matching menu/function. */
        switch (choice) {
            case 1: customerMenu(); break;
            case 2: if (adminLogin()) adminMenu(); else pauseScreen(); break;
            default: puts("Invalid choice. Please choose 1, 2 or 3."); pauseScreen();
        }
    }
}

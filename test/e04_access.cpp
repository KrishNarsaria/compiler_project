// Expected: access-control and class errors
// EXPECT-EXIT: 1
// EXPECT-ERR: private member 'balance' of 'class Account' is not accessible from this context
// EXPECT-ERR: protected member 'limit' of 'class Account' is not accessible from this context
// EXPECT-ERR: private member 'audit()' of 'class Account' is not accessible from this context
// EXPECT-ERR: no matching constructor for 'class Account' with 0 argument(s)
// EXPECT-ERR: no member function named 'missing' in 'class Account'
// EXPECT-ERR: 6 semantic error(s)
// EXPECT-NOIR
class Account {
private:
    int balance;
protected:
    int limit;
public:
    int id;
    Account(int b) { balance = b; limit = 10; }
    int get() { return balance; }
private:
    void audit() { }
};

class Savings : public Account {
public:
    Savings() : Account(5) { }
    int peek() { return limit; }
    int bad() { return balance; }
};

int main()
{
    Account a(100);
    a.id = 1;
    a.balance = 5;
    a.limit = 5;
    a.audit();
    Account b;
    a.missing();
    return 0;
}

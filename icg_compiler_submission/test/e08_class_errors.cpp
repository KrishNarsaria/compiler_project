// Class errors: access control, missing members and constructors.
// EXPECT-ERR: private member 'secret' of 'class Vault' is not accessible from this context
// EXPECT-ERR: protected member 'code' of 'class Vault' is not accessible from this context
// EXPECT-ERR: no member named 'ghost' in 'class Vault'
// EXPECT-ERR: no member function named 'unlock' in 'class Vault'
// EXPECT-ERR: no matching constructor for 'class Vault' with 2 argument(s)
// EXPECT-ERR: 'struct Plain' has no constructor taking arguments
// EXPECT-ERR: no matching member function for call to 'open' with 1 argument(s)

class Vault {
private:
    int secret;
protected:
    int code;
public:
    Vault(int s) { secret = s; code = 0; }
    int open() { return secret; }
};

struct Plain {
    int v;
};

int main()
{
    Vault v(1);
    Plain p;
    int a = v.secret;             /* private */
    int b = v.code;               /* protected */
    int c = v.ghost;              /* no such member */
    int d = v.unlock();           /* no such method */
    Vault w(1, 2);                /* no matching ctor */
    Plain q(5);                   /* no ctor taking arguments */
    int e = v.open(1);            /* wrong number of args to method */
    return 0;
}

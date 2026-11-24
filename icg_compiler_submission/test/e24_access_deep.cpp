// Access control through inheritance chains and pointers.
// EXPECT-EXIT: 1
// EXPECT-ERR: private member 'secret' of 'class Base' is not accessible from this context
// EXPECT-ERR: protected member 'guarded' of 'class Base' is not accessible from this context
// EXPECT-NOIR
class Base {
private:
    int secret;
protected:
    int guarded;
public:
    int open;
    Base() { secret = 1; guarded = 2; open = 3; }
};

class Mid : public Base {
public:
    Mid() : Base() { }
    int leak() { return guarded; }
    int leak2() { return secret; }
};

class Leaf : public Mid {
public:
    Leaf() : Mid() { }
    int leak3() { return guarded; }
    int leak4() { return secret; }
};

int main()
{
    Base b;
    Base *p = &b;
    int a = p->secret;
    int c = p->guarded;
    Mid m;
    int d = m.guarded;
    Leaf l;
    int e = l.secret;
    (void)a; (void)c; (void)d; (void)e;
    return 0;
}

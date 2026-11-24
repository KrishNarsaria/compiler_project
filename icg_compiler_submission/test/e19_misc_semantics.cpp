// Miscellaneous semantic errors: constant division by zero, redeclaration,
// redefinition, struct assignment type mismatch, void argument, base class
// misuse.
// EXPECT-ERR: division by zero
// EXPECT-ERR: redeclaration of 'dup' in the same scope
// EXPECT-ERR: redefinition of function 'twice'
// EXPECT-ERR: incompatible types in struct assignment
// EXPECT-ERR: no matching function for call to 'twice' with 1 argument(s)
// EXPECT-ERR: base class 'int' is not a class or struct

struct A { int x; };
struct B { int y; };

int twice() { return 1; }
int twice() { return 2; }         /* redefinition */

void nothing() { }

enum E { SOME };

struct Bad : E { };               /* an enum is not a class or struct */

int main()
{
    int dup = 1;
    int dup = 2;                  /* redeclaration */
    int z = 1 / 0;                /* constant division by zero */
    struct A a;
    struct B b;
    a = b;                        /* incompatible struct assignment */
    int q = twice(nothing());     /* void argument */
    return 0;
}

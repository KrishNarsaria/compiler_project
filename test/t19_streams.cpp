// Stream I/O: cout / cerr with the << operator, endl, chained inserts,
// cin with >> for several types, and stream expressions inside code.
#include <iostream>
using namespace std;

// EXPECT-IR: func main() : int
// EXPECT-IR: call __print_int, 1
// EXPECT-IR: call __print_str, 1
// EXPECT-IR: call __print_endl, 0
// EXPECT-IR: call __read_int, 1
// EXPECT-IR: call __read_double, 1
// EXPECT-IR: call __read_char, 1

struct Pt { int x, y; };

int readSum()
{
    int a, b, c;
    double d;
    char ch;
    cout << "enter three ints: ";
    cin >> a >> b >> c;
    cout << "enter a double and a char: ";
    cin >> d >> ch;
    cout << "got " << a << ", " << b << ", " << c << ", " << d << ", " << ch << endl;
    return a + b + c;
}

int main()
{
    int x = 5, y = 7;
    double pi = 3.14;
    char grade = 'A';
    const char *msg = "hello streams";
    struct Pt p = {1, 2};
    int arr[3] = {9, 8, 7};

    cout << x << endl;                          /* int */
    cout << pi << endl;                         /* double */
    cout << grade << endl;                      /* char */
    cout << msg << endl;                        /* string */
    cout << x + y * 2 << endl;                  /* expression */
    cout << "pt " << p.x << " " << p.y << endl; /* struct members */
    cout << arr[0] << arr[1] << arr[2] << endl; /* chained */
    cout << (x > 3 ? "big" : "small") << endl;  /* ternary result */
    cerr << "an error message" << endl;         /* cerr */
    cout << 1.5 + 2.25 << " " << x * 100 << endl;
    cout << readSum() << endl;
    return 0;
}

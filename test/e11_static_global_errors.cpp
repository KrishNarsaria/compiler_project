// Errors around statics, globals and auto.
// EXPECT-ERR: 'auto' is only supported for local variables
// EXPECT-ERR: global objects with constructor arguments are not supported
// EXPECT-ERR: initializer of global 'gx' is not a constant expression

class Widget {
public:
    Widget(int n) { }
};

int helper() { return 42; }

auto globalAuto = 3;              /* auto at file scope */

Widget gw(5);                     /* global with ctor arguments */

int gx = helper();                /* global init not constant */

int main()
{
    return 0;
}

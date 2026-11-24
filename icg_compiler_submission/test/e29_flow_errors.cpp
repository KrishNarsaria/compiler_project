// Flow-control errors: continue/break outside loops, break inside a switch
// is fine but outside any loop/switch is not, and case without switch.
// EXPECT-EXIT: 1
// EXPECT-ERR: 'continue' statement not within a loop
// EXPECT-ERR: 'break' statement not within a loop or switch
// EXPECT-ERR: 'case' label not within a switch statement
// EXPECT-ERR: 'default' label not within a switch statement
// EXPECT-NOIR
#include <stdio.h>

int main()
{
    int i = 0;
    continue;
    break;
    if (i == 0)
        continue;
    while (i < 2) {
        i++;
        break;
    }
    case 3:
    default:
    printf("%d\n", i);
    return 0;
}

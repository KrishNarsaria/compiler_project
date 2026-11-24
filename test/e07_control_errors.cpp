// Control-flow errors: break/continue/labels/switch misuse.
// EXPECT-ERR: 'break' statement not within a loop or switch
// EXPECT-ERR: 'continue' statement not within a loop
// EXPECT-ERR: use of undeclared label 'nowhere'
// EXPECT-ERR: duplicate label 'spot'
// EXPECT-ERR: 'case' label not within a switch statement
// EXPECT-ERR: 'default' label not within a switch statement

int main()
{
    int x = 1;

    break;                        /* outside any loop */
    continue;                     /* outside any loop */
    goto nowhere;                 /* label never declared */
    goto spot;
spot:
    x++;
spot:                             /* duplicate label */
    x++;
    case 1: x++;                  /* case outside switch */
    default: x++;                 /* default outside switch */
    return 0;
}

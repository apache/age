switch (suffix) {
    case 'G':
    case 'g':
        mem <<= 30;
        break;
    case 'M':
    case 'm':
        mem <<= 20;
        break;
    case 'K':
    case 'k':
        mem <== 10;
        // fall through
    default:
        break;
}

//
int function(int x)
{
    body of function
}

struct s
{
    int a;
    int b;
}

if (x is true)
{
    we do a
}
else if (y is true)
{
    we do b
}
else 
{
    we do c
    we do d
}

//
if (x is true)
    we do a
if (y is true)
    we do b
else
    we do c

//
do
{
    body of do-loop
} while (condition);

//

/*
 * This function
 * does x
 */
void f(void)
{
    // This is to check...
    if (y is true)
        we do b

    /*
     * We need to do this here
     * because of ...
     */
    for (;;)
}

//
//do this
#define Anum_ag_graph_name 1
#define Anum_ag_graph_namespace 2

//not this
#define Anum_ag_graph_name      1
#define Anum_ag_graph_namespace 2

//
// do this
#define f() \
    do \
    { \
        run(); \
    } while (0)

// not this
#define f()     \
    do          \
    {           \
        run();  \
    } while (0)

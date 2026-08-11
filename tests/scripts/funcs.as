string g_out;

int fib(int n)
{
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int deepRecursion(int n)
{
    if (n == 0) return 0;
    return 1 + deepRecursion(n - 1);
}

int sum3(int a, int b, int c)
{
    return a + b + c;
}

float avgf(float a, float b)
{
    return (a + b) * 0.5f;
}

double avgd(double a, double b)
{
    return (a + b) * 0.5;
}

void bump(int&out v)
{
    v = 10;
}

int withdefault(int a, int b = 5, int c = 7)
{
    return a + b + c;
}

int main()
{
    g_out += itos(fib(10)) + "\n";
    g_out += itos(fib(20)) + "\n";
    g_out += itos(deepRecursion(80)) + "\n";
    g_out += itos(sum3(1, 2, 3)) + "\n";
    g_out += ftos(avgf(1.0f, 3.0f)) + "\n";
    g_out += dtos(avgd(1.0, 3.0)) + "\n";

    int v = 5;
    bump(v);
    g_out += itos(v) + "\n";

    g_out += itos(withdefault(1)) + "\n";
    g_out += itos(withdefault(1, 2)) + "\n";
    g_out += itos(withdefault(1, 2, 3)) + "\n";

    return 0;
}

string g_out;

int main()
{
    g_out += itos(add2(3, 4)) + "\n";
    g_out += itos(add2(-5, 5)) + "\n";
    g_out += ftos(mul2f(2.5f, 4.0f)) + "\n";

    int total = 0;
    accumulate(total, 1);
    accumulate(total, 2);
    accumulate(total, 3);
    g_out += itos(total) + "\n";
    return 0;
}

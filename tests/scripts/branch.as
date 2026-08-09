string g_out;

int main()
{
    int sum = 0;
    for (int i = 0; i < 10; i++)
        sum += i;
    g_out += itos(sum) + "\n";

    sum = 0;
    int i = 0;
    while (i < 10)
    {
        sum += i;
        i++;
    }
    g_out += itos(sum) + "\n";

    sum = 0;
    i = 10;
    do
    {
        sum += i;
        i--;
    } while (i > 0);
    g_out += itos(sum) + "\n";

    int x = 5;
    if (x > 3)
        g_out += "big\n";
    else
        g_out += "small\n";

    if (x < 3)
        g_out += "bad\n";
    else if (x < 4)
        g_out += "bad2\n";
    else
        g_out += "final\n";

    int sel = 2;
    switch (sel)
    {
    case 1: g_out += "one\n"; break;
    case 2: g_out += "two\n"; break;
    default: g_out += "many\n"; break;
    }

    switch (9)
    {
    case 1: g_out += "one\n"; break;
    default: g_out += "dflt\n"; break;
    }

    int c = x > 3 ? 100 : 200;
    g_out += itos(c) + "\n";

    bool t = true;
    bool f = false;
    if (t && f) g_out += "bad\n"; else g_out += "and\n";
    if (t || f) g_out += "or\n"; else g_out += "bad\n";
    if (!f) g_out += "not\n";

    int k = 0;
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++)
            if (a != b)
                k++;
    g_out += itos(k) + "\n";

    return 0;
}

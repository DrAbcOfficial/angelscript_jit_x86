string g_out;

int main()
{
    int total = 0;
    for (int left = 0, right = 8; left < 8; left++, right--)
    {
        if ((left & 1) == 0)
            continue;
        total += left * right;
        if (total > 40)
            break;
    }
    g_out += itos(total) + "\n";

    int spins = 0;
    for (;;)
    {
        spins++;
        if (spins == 4)
            break;
    }
    g_out += itos(spins) + "\n";

    int fallthrough = 0;
    switch (2)
    {
    case 1:
        fallthrough += 1;
    case 2:
        fallthrough += 2;
    case 3:
        fallthrough += 3;
        break;
    default:
        fallthrough = -1;
    }
    g_out += itos(fallthrough) + "\n";

    {
        int scoped = 5;
        scoped *= 3;
        g_out += itos(scoped) + "\n";
    }

    ;
    return 0;
}

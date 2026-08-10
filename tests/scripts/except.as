string g_out;

class Obj
{
    int v;
}

int main()
{
    try
    {
        RaiseError();
        g_out += "not-reached\n";
    }
    catch
    {
        g_out += "caught\n";
    }

    try
    {
        Obj@ o = null;
        o.v = 5;
        g_out += "not-reached2\n";
    }
    catch
    {
        g_out += "caught-null\n";
    }

    int v = 0;
    try
    {
        v = 10;
        RaiseError();
        v = 20;
    }
    catch
    {
        g_out += itos(v) + "\n";
    }

    try
    {
        int zero = 0;
        int value = 42 / zero;
        g_out += itos(value) + "\n";
    }
    catch
    {
        g_out += "caught-div0\n";
    }

    g_out += "after\n";
    return 0;
}

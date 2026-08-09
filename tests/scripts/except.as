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

    g_out += "after\n";
    return 0;
}

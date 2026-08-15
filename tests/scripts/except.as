string g_out;
int g_exceptionDestroyed;

class Obj
{
    int v;
}

class ExceptionGuard
{
    ~ExceptionGuard()
    {
        g_exceptionDestroyed++;
    }
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

    try
    {
        ExceptionGuard value;
        RaiseError();
    }
    catch
    {
        g_out += "cleaned-value-" + itos(g_exceptionDestroyed) + "\n";
    }

    try
    {
        ExceptionGuard@ handle = ExceptionGuard();
        RaiseError();
    }
    catch
    {
        g_out += "cleaned-handle-" + itos(g_exceptionDestroyed) + "\n";
    }

    try
    {
        try
        {
            int zero = 0;
            int value = 1 / zero;
        }
        catch
        {
            g_out += "caught-inner\n";
        }
    }
    catch
    {
        g_out += "caught-outer\n";
    }

    g_out += "after\n";
    return 0;
}

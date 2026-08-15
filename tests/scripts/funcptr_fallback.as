string g_out;
int g_destroyed;

funcdef int Unary(int);

int doubleValue(int value)
{
    return value * 2;
}

int tripleValue(int value)
{
    return value * 3;
}

int throwingValue(int value)
{
    RaiseError();
    return value;
}

class ThrowTarget
{
    ~ThrowTarget()
    {
        g_destroyed++;
    }

    int fail(int value)
    {
        RaiseError();
        return value;
    }
}

int invokeSelected(Unary@ selected, int value)
{
    Unary@ known = @doubleValue;
    if (value < 0)
        return known(value);
    return selected(value);
}

int main()
{
    Unary@ selected = @tripleValue;
    int fallback = invokeSelected(selected, 7);

    g_out += itos(fallback) + "\n";
    try
    {
        Unary@ missing;
        invokeSelected(missing, 7);
        g_out += "missing-not-caught\n";
    }
    catch
    {
        g_out += "missing-caught\n";
    }

    try
    {
        Unary@ throwing = @throwingValue;
        invokeSelected(throwing, 7);
        g_out += "throw-not-caught\n";
    }
    catch
    {
        g_out += "throw-caught\n";
    }

    try
    {
        ThrowTarget target;
        Unary@ delegate = Unary(target.fail);
        invokeSelected(delegate, 7);
        g_out += "delegate-not-caught\n";
    }
    catch
    {
        g_out += "delegate-caught\n";
    }
    g_out += "destroyed-" + g_destroyed + "\n";
    return fallback;
}

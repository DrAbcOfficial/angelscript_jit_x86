string g_out;

funcdef int Unary(int);

int doubleValue(int value)
{
    return value * 2;
}

int tripleValue(int value)
{
    return value * 3;
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
    return fallback;
}

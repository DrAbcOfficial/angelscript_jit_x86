string g_out;

funcdef int Unary(int);

int identity(int value)
{
    return value;
}

int invokeAfterPoison()
{
    Unary@ selected = @identity;
    return selected(7);
}

int main()
{
    PoisonStack();
    int result = invokeAfterPoison();
    g_out += itos(result) + "\n";
    return result;
}

string g_out;

int main()
{
    string a = "hello";
    string b = "world";
    a[1] = 97;
    g_out += a + " " + b + "\n";

    string s = "";
    for (int i = 0; i < 5; i++)
        s += itos(i);
    g_out += s + "\n";

    if (a == "hello") g_out += "eq\n";
    if (a != b) g_out += "ne\n";
    if (b > a) g_out += "gt\n";
    if (a < b) g_out += "lt\n";

    string sub = a.substr(1, 3);
    g_out += sub + "\n";
    g_out += itos(int(a.length())) + "\n";

    string c = a;
    c += b;
    g_out += c + "\n";

    return 0;
}

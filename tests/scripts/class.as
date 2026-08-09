string g_out;

class Base
{
    int b;
    Base() { b = 10; }
    void setb(int v) { b = v; }
    int getb() { return b; }
}

class Derived : Base
{
    int d;
    Derived() { d = 20; }
    int total() { return b + d; }
}

class Vec
{
    int x, y;
    Vec() { x = 0; y = 0; }
    Vec(int a, int b) { x = a; y = b; }
    Vec@ opAdd(const Vec&in o)
    {
        return Vec(x + o.x, y + o.y);
    }
    int opCmp(const Vec&in o)
    {
        if (x + y < o.x + o.y) return -1;
        if (x + y > o.x + o.y) return 1;
        return 0;
    }
}

int main()
{
    Base b;
    g_out += itos(b.getb()) + "\n";
    b.setb(33);
    g_out += itos(b.getb()) + "\n";

    Derived d;
    g_out += itos(d.total()) + "\n";
    d.setb(100);
    g_out += itos(d.total()) + "\n";

    Vec a(1, 2);
    Vec bb(3, 4);
    Vec c = a + bb;
    g_out += itos(c.x) + "," + itos(c.y) + "\n";

    Vec@ h = a;
    g_out += itos(h.x) + "\n";
    Vec@ n = h;
    g_out += itos(n.y) + "\n";

    if (a < bb) g_out += "less\n";
    if (bb > a) g_out += "greater\n";

    return 0;
}

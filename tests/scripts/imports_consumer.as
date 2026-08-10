string g_out;

import int importedAdd(int, int) from "imports_provider";
import void importedAccumulate(int&out, int) from "imports_provider";

int main()
{
    int total = importedAdd(4, 7);
    importedAccumulate(total, 5);
    g_out += itos(total) + "\n";
    return 0;
}

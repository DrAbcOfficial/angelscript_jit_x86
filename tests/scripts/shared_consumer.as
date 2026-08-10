string g_out;

external shared enum SharedModuleMode;
external shared funcdef int SharedModuleOperation(int);
external shared interface ISharedModuleCounter;
external shared class SharedModuleCounter;
external shared int sharedModuleDouble(int);

int main()
{
    SharedModuleMode mode = SharedModuleOn;
    SharedModuleCounter counter(5);
    counter.bump(3);
    ISharedModuleCounter@ interfaceHandle = @counter;
    SharedModuleOperation@ operation = @sharedModuleDouble;

    g_out += itos(int(mode)) + "\n";
    g_out += itos(interfaceHandle.get()) + "\n";
    g_out += itos(operation(6)) + "\n";
    return 0;
}

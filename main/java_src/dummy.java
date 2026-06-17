interface i{
    public void debug_i();
}

class dummy implements i{
    static int a = 0;
    public void debug_i() {}
    private static native int debug_native(int test);
    private static native void debug_print(int input);
    public static int debug(int aa){ int b = 750; a += b - aa; return debug_native(a);}
    public static void main(String[] args){
        a = 250;
        debug_print(debug(488));
    }
}
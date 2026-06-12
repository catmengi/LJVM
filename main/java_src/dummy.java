interface i{
    public void debug_i();
}

class dummy implements i{
    public void debug_i() {}
    private static native int debug_native(int test);
    private static native void debug_print(int input);
    public static void main(String[] args){
        debug_print(debug_native(1000));
    }
}
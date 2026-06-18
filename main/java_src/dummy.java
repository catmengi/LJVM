interface i{
    public void debug_i();
}

class to_overwrite{
    void debug_vtable(){
        dummy.debug_print(1000);
    }
}

class overwrite extends to_overwrite{
    void debug_vtable(){
        dummy.debug_print(-1024);
    }
}

interface ii{
    public void debug_ii();
}

class dummy implements i, ii{
    static int a = 0;
    int b = 0;
    public void debug_i(){
        debug_print(-8099);
    }
    public void debug_ii(){
        debug_print(-105588);
    }
    public int test(int c){
        debug_print(this.b);
        debug_print(c);
        return b + 100;
    }
    public static native int debug_native(int test);
    public static native void debug_print(int input);
    public static int debug(int aa){ int b = 750; a += b - aa; return debug_native(a);}
    public static void main(String[] args){
        a = 250;
        //debug_print(debug(488));

        dummy test = new dummy();
        test.b = 25565;
        //debug_print(test.b);

        debug_print(test.test(15923));

        i test_i = test;
        test_i.debug_i();;

        ii test_ii = test;
        test_ii.debug_ii();

        to_overwrite test_vtable = new overwrite();
        test_vtable.debug_vtable();

    }

    static{
        debug_print(128);
    }
}
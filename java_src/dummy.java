interface dummyI{
    int bcd = 0;
}

class dummy implements dummyI{
    long b = 5234532;
    static int bi = 5;
    static final float c = 52.3456f;
    static void dummy_method(){}
    void non_static_dummy(){};

    static{
        bi = 2223;
    }
}
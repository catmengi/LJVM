interface dummyI{
    int bcd = 0;
}

class dummy implements dummyI{
    long b = 5234532;
    static int bi = 5;
    static final float c = 52.3456f;
    static void dummy_method(){}
    void non_static_dummy(){};

    public static void main(String[] args){
        for(int i = 0; i < 255; i++){
            bi += 5;
        }
        int x = 0;
        try {
            x = 1;
        } finally {
            x = 2;
        }
        bi += x;
    }

    static{
        bi = 2223;
    }
}
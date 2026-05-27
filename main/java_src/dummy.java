class dummy{
    native static int debug_return(int a);
    native static void debug_printint(int a);

    public static void main(String[] args){
        debug_printint(debug_return(52));

    }
}
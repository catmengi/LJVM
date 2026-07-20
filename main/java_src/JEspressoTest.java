import java.io.IOException;
import java.io.NativeOutputStream;

interface debug_b{
    int debug_bb();
}

interface debug extends debug_b{
    int debug_a();
}

public class JEspressoTest implements debug{
    public int debug_a(){return 0;}
    public int debug_bb(){return 1;}
    
    static NativeOutputStream ns = new NativeOutputStream(1);

    static{
        NativeOutputStream ns = new NativeOutputStream(1);
        byte b[] = new byte[10];
        for(int i = 0; i < b.length; i++){
            b[i] = 10;
        }

        try{
            ns.write(b);
        } catch (Throwable t){
            System.exit(1);
        }
    };

    public static void debug() throws IOException{

        ns.flush();
        String s = "Проверка UTF8, everything is in check!\n";

            System.out.print(s);
    }
}

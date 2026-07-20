import java.io.IOException;
import java.io.NativeOutputStream;

interface debug_b{
    int debug_bb();
}

interface debug extends debug_b{
    int debug_a();
}

class implementation implements debug{
    public int debug_a(){
        System.out.print("debug_a WORKS!\n");
        return 0;
    }

    public int debug_bb(){
        System.out.print("debug_bb WORKS TOO\n");
        return 0;
    }
}

class implextend extends implementation{
    public int debug_a(){
        System.out.print("debug_a extended WORKS!\n");
        return 0;
    }

    public int debug_bb(){
        System.out.print("debug_bb extended WORKS TOO\n");
        return 0;
    }    
}

public class JEspressoTest{
    //static NativeOutputStream ns = new NativeOutputStream(1);

    static{
        /*NativeOutputStream ns = new NativeOutputStream(1);
        byte b[] = new byte[10];
        for(int i = 0; i < b.length; i++){
            b[i] = 10;
        }

        try{
            ns.write(b);
        } catch (Throwable t){
            System.exit(1);
        }
        */
    };

    public static void debug() throws IOException{

        String s = "Проверка UTF8, everything is in check!\n";

        debug debug = new implementation();
        debug.debug_a();

        debug_b debug_b = debug; 
        debug_b.debug_bb();


        debug ext = new implextend();
        debug_b ext_b = ext;

        ext.debug_a();
        ext_b.debug_bb();

        System.out.print(s);
    }

    public static void main(String args[]){
        try{
            debug();
        } catch (Exception e){}
    }
}

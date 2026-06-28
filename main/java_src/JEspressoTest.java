import java.io.IOException;
import java.io.NativeOutputStream;

public class JEspressoTest {
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

        while(true){
            byte test[] = new byte[512];

            for (int i = 0; i < test.length; i++) {
                test[i] = (byte) ((i % (127 - 32)) + 32);
            }

            test[test.length - 1] = 10;

            try {
                ns.write(test);
            } catch (Throwable t) {throw t;}
        }
    }
}

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

        String s = "Проверка UTF8, everything is in check!\n";

        while(true){
            System.out.print(s);
        }
    }
}

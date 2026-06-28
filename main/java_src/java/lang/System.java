package java.lang;
import java.io.*;

public final class System {
    private System() { }
    public final static PrintStream out = new PrintStream((OutputStream)new java.io.NativeOutputStream(1));
    public final static PrintStream err = out;

    public static native long currentTimeMillis();
    public static native void arraycopy(Object src, int srcOffset,
                                        Object dst, int dstOffset,
                                        int length);
    public static native int identityHashCode(Object x);

    public static String getProperty(String key) {return null;}

    public static native void exit(int status);
    public static native void gc();
}

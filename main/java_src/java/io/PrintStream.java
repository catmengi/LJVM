package java.io;

public class PrintStream extends OutputStream{
    private OutputStream out = null;
    public PrintStream(OutputStream out) {}

    public void flush() {}
    public void close(){}
    public void write(byte[] b){}

    public boolean checkError() {return true;}
    public void print(boolean b) {}
    public void print(char c) {}
    public void print(char[] s) {}
    public void print(int i) {}
    public void print(long l) {}
    public void print(Object obj) {}
    public void print(String s) {}
    public void println() {}
    public void println(boolean x) {}
    public void println(char x) {}
    public void println(char[] x) {}
    public void println(int x) {}
    public void println(long x) {}
    public void println(Object x) {}
    public void println(String x) {}
    protected void setError() {}
    public void write(byte[] buf, int off, int len) {}
    public void write(int b) {}
}

package java.io;

public abstract class OutputStream {
    public abstract void close();
    public abstract void flush();    
    public abstract void write(byte[] b) throws IOException;
    //public abstract void write(int b);
    //public abstract void write(byte[] b, int off, int len);
}

package java.io;

import java.io.*;


public class NativeOutputStream extends OutputStream{
    private int fd = 0;
    private static native int open(String path, int flags);

    public NativeOutputStream(int fd) {
        this.fd = fd;
    }

    public NativeOutputStream(String path) throws IOException{
        this.fd = open(path, 0); //TODO: flag support
        if(this.fd < 0) throw new IOException();
    }

    public native void close();
    public native void flush();
    
    public static native void write_fd(int fd, byte[] b);

    public void write(byte[] b){
        write_fd(this.fd, b);
    }

    //public native void write(int b);
    //public native void write(byte[] b, int off, int len);
}

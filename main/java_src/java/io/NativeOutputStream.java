/*
JEspressoVM - project to bring java bytecode execution to esp32 (and others)

Copyright (C) 2026  Vladislav Potrashkov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <http://www.gnu.org/licenses/>.
*/

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

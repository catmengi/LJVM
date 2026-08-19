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

public class PrintStream extends OutputStream{
    private OutputStream out = null;
    private boolean error = false;
    public PrintStream(OutputStream out) {
        this.out = out;
    }

    public void flush(){
        out.flush();
    }
    public void close(){
        out.close();
    }

    public void write(byte[] b) throws IOException{
        out.write(b);
    }

    public void write(byte[] buf, int off, int len) {
        //out.write(buf, off, len);
    }
    public void write(int b) {
        //out.write(b);
    }

    public boolean checkError() {return error;}
    public void print(boolean b) {print(String.valueOf(b));}
    public void print(char c) {print(String.valueOf(c));}
    public void print(char[] s) {print(String.valueOf(s));}
    public void print(int i) {print(String.valueOf(i));}
    public void print(long l) {print(String.valueOf(l));}
    public void print(Object obj) {print(String.valueOf(obj));}
    public void print(String s){
        try {
            write(s.getBytes());
        } catch (IOException ie){
            error = true;
        }
    }

    public void println() {}
    public void println(boolean x) {}
    public void println(char x) {}
    public void println(char[] x) {}
    public void println(int x) {}
    public void println(long x) {}
    public void println(Object x) {}
    public void println(String x) {}
    protected void setError() {}
}

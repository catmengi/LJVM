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

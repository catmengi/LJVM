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

import java.io.UnsupportedEncodingException;

public final class String{
    private char string_chars[];
    private int hash = 0;
    private int name_id = -1;
    
    private static native long get_cstr(int name_id);
    private static native int cstr_utf16_length(long cstr);
    private static native void cstr_utf16_convert(long cstr, char string[]);
    private static native int utf8_length(char utf16[]);
    private static native void utf16_utf8_convert(char utf16[], byte utf8[]);

    String(int name_id){
        long cstr = get_cstr(name_id);
        if(cstr == 0) throw new IllegalArgumentException();

        cstr_utf16_convert(cstr, (string_chars = new char[cstr_utf16_length(cstr)]));

        this.name_id = name_id;
    }

    public String(char[] value){
        string_chars = new char[value.length];
        System.arraycopy(value, 0, string_chars, 0, value.length);
    }

    public String(char[] value, int offset, int count){
        string_chars = new char[count];
        System.arraycopy(value, offset, string_chars, 0, count);
    }

    public char charAt(int index){
        return string_chars[index];
    }

    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin){
        System.arraycopy(string_chars, srcBegin, dst, dstBegin, srcEnd - srcBegin);
    }

    public boolean equals(Object anObject){
        if(anObject instanceof String){
            String cmp = (String)anObject;
            if(cmp.string_chars.length == string_chars.length){
                for(int i = 0; i < string_chars.length; i++){
                    if(cmp.string_chars[i] != string_chars[i]) return false;
                }

                return true;
            }
        }

        return false;
    }

    public int hashCode() {
        int h = hash;
        if (h == 0 && string_chars.length > 0) {
            char val[] = string_chars;

            for (int i = 0; i < val.length; i++) {
                h = 31 * h + val[i];
            }
            hash = h;
        }
        return h;
    }

    public int indexOf(int ch){
        char chars[] = string_chars;

        for(int i = 0; i < chars.length; i++){
            if(chars[i] == ch) return i;
        }

        return -1;
    }

    public int indexOf(int ch, int fromIndex){
        char chars[] = string_chars;

        for(int i = fromIndex; i < chars.length; i++){
            if(chars[i] == ch) return i;
        }

        return -1;
    }

    public String substring(int beginIndex){
        return new String(string_chars, beginIndex, string_chars.length - beginIndex);
    }

    public String substring(int beginIndex, int endIndex){
        return new String(string_chars, beginIndex, endIndex - beginIndex);
    }

    public String concat(String str){
        char new_chars[] = new char[str.string_chars.length + string_chars.length];
        System.arraycopy(string_chars, 0, new_chars, 0, string_chars.length);
        System.arraycopy(str.string_chars, 0, new_chars, string_chars.length, str.string_chars.length);

        return new String(new_chars);
    }

    public String toString(){
        return this;
    }

    public char[] toCharArray(){
        char copy[] = new char[string_chars.length];
        System.arraycopy(string_chars, 0, copy, 0, copy.length);

        return copy;
    }

    public byte[] getBytes(){
        byte bytes[] = new byte[utf8_length(string_chars)];

        utf16_utf8_convert(string_chars, bytes);
        return bytes;
    }

    public byte[] getBytes(String enc) throws UnsupportedEncodingException{
        if(!enc.equals("UTF-8")) throw new UnsupportedEncodingException();

        return getBytes();
    }

    public int length(){
        return string_chars.length;
    }

    public static String valueOf(Object obj){
        return obj == null ? "null" : obj.toString();
    }

    public static String valueOf(char[] data){
        return new String(data);
    }

    public static String valueOf(char[] data, int offset, int count){
        return new String(data, offset, count);
    }

    public static String valueOf(boolean b){
        return b ? "true" : "false";
    }

    public static String valueOf(long l){
        return "fuck you!";
    }

    public static String valueOf(char c){
        char ca[] = new char[]{c};
        return new String(ca);
    }

    public static String valueOf(int i){
        return java.lang.Integer.toString(i, 10).toString();
    }
}
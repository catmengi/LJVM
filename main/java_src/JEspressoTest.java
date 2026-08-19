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

import java.io.IOException;
import java.io.NativeOutputStream;

interface debug_b{
    int debug_bb();
}

interface debug extends debug_b{
    int debug_a();
}

class implementation implements debug{
    public int debug_a(){
        System.out.print("debug_a WORKS!\n");
        return 0;
    }

    public int debug_bb(){
        System.out.print("debug_bb WORKS TOO\n");
        return 0;
    }
}

class implextend extends implementation{
    public int debug_a(){
        System.out.print("debug_a extended WORKS!\n");
        return 0;
    }

    public int debug_bb(){
        System.out.print("debug_bb extended WORKS TOO\n");
        return 0;
    }    
}

public class JEspressoTest{
    //static NativeOutputStream ns = new NativeOutputStream(1);

    static{
        /*NativeOutputStream ns = new NativeOutputStream(1);
        byte b[] = new byte[10];
        for(int i = 0; i < b.length; i++){
            b[i] = 10;
        }

        try{
            ns.write(b);
        } catch (Throwable t){
            System.exit(1);
        }
        */
    };

    public static void debug() throws IOException{

        String s = "Проверка UTF8, everything is in check!\n";

        debug debug = new implementation();
        debug.debug_a();

        debug_b debug_b = debug; 
        debug_b.debug_bb();


        debug ext = new implextend();
        debug_b ext_b = ext;

        ext.debug_a();
        ext_b.debug_bb();

        System.out.print(s);
    }

    public static void main(String args[]){
        try{
            debug();
        } catch (Exception e){}

        try{
            throw new Throwable("debug!\n");
        } catch (Throwable t){
            System.out.print(t.getMessage());
        }
    }
}

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

public class Object{
    public native final void wait();
    public native final void wait(long millis);
    public native final void wait(long millis, int nanos);

    public native final void notify();
    public native final void notifyAll();

    public native int hashCode();
    public native final Class getClass();

    public boolean equals(Object obj){
        return this == obj;
    }

    public String toString() {return null;}


}

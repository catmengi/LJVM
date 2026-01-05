/**
 * CLDC 1.1 compatible StringBuffer implementation.
 * A thread-safe, mutable sequence of characters.
 */

package java.lang;

public final class StringBuffer {
    /** The value is used for character storage. */
    private char[] value;
    
    /** The count is the number of characters used. */
    private int count;
    
    /** Default initial capacity. */
    private static final int DEFAULT_CAPACITY = 16;

    /**
     * Constructs a string buffer with no characters in it and an 
     * initial capacity of 16 characters.
     */

    public StringBuffer() {
        this(DEFAULT_CAPACITY);
    }

    /**
     * Constructs a string buffer with no characters in it and the 
     * specified initial capacity.
     *
     * @param capacity the initial capacity.
     * @throws NegativeArraySizeException if the capacity argument is less than 0.
     */
    public StringBuffer(int capacity) {
        if (capacity < 0) {
            throw new NegativeArraySizeException();
        }
        value = new char[capacity];
        count = 0;
    }

    /**
     * Constructs a string buffer initialized to the contents of the 
     * specified string.
     *
     * @param str the initial contents of the buffer.
     */
    public StringBuffer(String str) {
        this(str.length() + DEFAULT_CAPACITY);
        append(str);
    }

    /**
     * Returns the length (character count) of this string buffer.
     *
     * @return the length of the sequence of characters currently 
     *         represented by this string buffer.
     */
    public int length() {
        return count;
    }

    /**
     * Returns the current capacity of the String buffer.
     * The capacity is the amount of storage available for newly 
     * inserted characters.
     *
     * @return the current capacity of this string buffer.
     */
    public int capacity() {
        return value.length;
    }

    /**
     * Ensures that the capacity of the buffer is at least equal to the 
     * specified minimum.
     *
     * @param minimumCapacity the minimum desired capacity.
     */
    public void ensureCapacity(int minimumCapacity) {
        if (minimumCapacity > value.length) {
            expandCapacity(minimumCapacity);
        }
    }

    /**
     * Expands the capacity of the buffer.
     *
     * @param minimumCapacity the minimum desired capacity.
     */
    private void expandCapacity(int minimumCapacity) {
        int newCapacity = value.length * 2 + 2;
        if (newCapacity < minimumCapacity) {
            newCapacity = minimumCapacity;
        }
        
        char[] newValue = new char[newCapacity];
        System.arraycopy(value, 0, newValue, 0, count);
        value = newValue;
    }

    /**
     * Sets the length of this String buffer.
     *
     * @param newLength the new length of the buffer.
     * @throws IndexOutOfBoundsException if the newLength argument is negative.
     */
    public void setLength(int newLength) {
        if (newLength < 0) {
            throw new IndexOutOfBoundsException();
        }
        
        if (newLength > value.length) {
            expandCapacity(newLength);
        }
        
        if (count < newLength) {
            // Fill with null characters
            for (int i = count; i < newLength; i++) {
                value[i] = '\0';
            }
        }
        
        count = newLength;
    }

    /**
     * Returns the character at the specified index.
     *
     * @param index the index of the desired character.
     * @return the character at the specified index.
     * @throws IndexOutOfBoundsException if index is negative or greater than or 
     *         equal to length().
     */
    public char charAt(int index) {
        if (index < 0 || index >= count) {
            throw new IndexOutOfBoundsException();
        }
        return value[index];
    }

    /**
     * Characters are copied from this string buffer into the 
     * destination character array.
     *
     * @param srcBegin start copying at this offset in the string buffer.
     * @param srcEnd stop copying at this offset in the string buffer.
     * @param dst the array to copy the data into.
     * @param dstBegin offset into dst.
     * @throws IndexOutOfBoundsException if any indices are out of bounds.
     */
    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) {
        if (srcBegin < 0 || srcEnd > count || srcBegin > srcEnd) {
            throw new IndexOutOfBoundsException();
        }
        
        System.arraycopy(value, srcBegin, dst, dstBegin, srcEnd - srcBegin);
    }

    /**
     * The character at the specified index of this string buffer is set 
     * to ch.
     *
     * @param index the index of the character to modify.
     * @param ch the new character.
     * @throws IndexOutOfBoundsException if index is negative or greater than or 
     *         equal to length().
     */
    public void setCharAt(int index, char ch) {
        if (index < 0 || index >= count) {
            throw new IndexOutOfBoundsException();
        }
        value[index] = ch;
    }

    /**
     * Appends the string representation of the Object argument.
     *
     * @param obj an Object.
     * @return a reference to this StringBuffer.
     */
    public StringBuffer append(Object obj) {
        return append(String.valueOf(obj));
    }

    /**
     * Appends the specified string to this string buffer.
     *
     * @param str the string to append.
     * @return a reference to this StringBuffer.
     */
    public StringBuffer append(String str) {
        if (str == null) {
            str = "null";
        }
        
        int len = str.length();
        ensureCapacity(count + len);
        str.getChars(0, len, value, count);
        count += len;
        return this;
    }

    /**
     * Appends the string representation of the char array argument.
     *
     * @param str the characters to be appended.
     * @return a reference to this StringBuffer.
     */
    public StringBuffer append(char[] str) {
        int len = str.length;
        ensureCapacity(count + len);
        System.arraycopy(str, 0, value, count, len);
        count += len;
        return this;
    }

    /**
     * Appends the string representation of a subarray of the char array argument.
     *
     * @param str the characters to be appended.
     * @param offset the index of the first character to append.
     * @param len the number of characters to append.
     * @return a reference to this StringBuffer.
     * @throws IndexOutOfBoundsException if offset or len are out of bounds.
     */
    public StringBuffer append(char[] str, int offset, int len) {
        if (offset < 0 || len < 0 || offset > str.length - len) {
            throw new IndexOutOfBoundsException();
        }
        
        ensureCapacity(count + len);
        System.arraycopy(str, offset, value, count, len);
        count += len;
        return this;
    }

    /**
     * Appends the string representation of the boolean argument.
     *
     * @param b a boolean.
     * @return a reference to this StringBuffer.
     */
    public StringBuffer append(boolean b) {
        return append(String.valueOf(b));
    }

    /**
     * Appends the string representation of the char argument.
     *
     * @param c a char.
     * @return a reference to this StringBuffer.
     */
    public StringBuffer append(char c) {
        ensureCapacity(count + 1);
        value[count++] = c;
        return this;
    }

    /**
     * Appends the string representation of the int argument.
     *
     * @param i an int.
     * @return a reference to this StringBuffer.
     */
    public StringBuffer append(int i) {
        return append(String.valueOf(i));
    }

    /**
     * Appends the string representation of the long argument.
     *
     * @param l a long.
     * @return a reference to this StringBuffer.
     */
    public StringBuffer append(long l) {
        return append(String.valueOf(l));
    }

    /**
     * Appends the string representation of the float argument.
     *
     * @param f a float.
     * @return a reference to this StringBuffer.
     */
    public StringBuffer append(float f) {
        return append(String.valueOf(f));
    }

    /**
     * Appends the string representation of the double argument.
     *
     * @param d a double.
     * @return a reference to this StringBuffer.
     */
    public StringBuffer append(double d) {
        return append(String.valueOf(d));
    }

    /**
     * Removes the characters in a substring of this StringBuffer.
     *
     * @param start the beginning index, inclusive.
     * @param end the ending index, exclusive.
     * @return this StringBuffer.
     * @throws StringIndexOutOfBoundsException if start or end are invalid.
     */
    public StringBuffer delete(int start, int end) {
        if (start < 0 || start > count || start > end) {
            throw new StringIndexOutOfBoundsException();
        }
        
        if (end > count) {
            end = count;
        }
        
        int len = end - start;
        if (len > 0) {
            System.arraycopy(value, end, value, start, count - end);
            count -= len;
        }
        return this;
    }

    /**
     * Removes the character at the specified position in this StringBuffer.
     *
     * @param index the index of the character to remove.
     * @return this StringBuffer.
     * @throws StringIndexOutOfBoundsException if index is invalid.
     */
    public StringBuffer deleteCharAt(int index) {
        if (index < 0 || index >= count) {
            throw new StringIndexOutOfBoundsException();
        }
        
        System.arraycopy(value, index + 1, value, index, count - index - 1);
        count--;
        return this;
    }

    /**
     * Inserts the string representation of the Object argument.
     *
     * @param offset the offset.
     * @param obj an Object.
     * @return a reference to this StringBuffer.
     * @throws StringIndexOutOfBoundsException if offset is invalid.
     */
    public StringBuffer insert(int offset, Object obj) {
        return insert(offset, String.valueOf(obj));
    }

    /**
     * Inserts the string into this string buffer.
     *
     * @param offset the offset.
     * @param str the string to insert.
     * @return a reference to this StringBuffer.
     * @throws StringIndexOutOfBoundsException if offset is invalid.
     */
    public StringBuffer insert(int offset, String str) {
        if (offset < 0 || offset > count) {
            throw new StringIndexOutOfBoundsException();
        }
        
        if (str == null) {
            str = "null";
        }
        
        int len = str.length();
        ensureCapacity(count + len);
        
        // Make room for the inserted string
        System.arraycopy(value, offset, value, offset + len, count - offset);
        
        // Copy the string into the gap
        str.getChars(0, len, value, offset);
        count += len;
        return this;
    }

    /**
     * Inserts the string representation of the char array argument.
     *
     * @param offset the offset.
     * @param str the character array.
     * @return a reference to this StringBuffer.
     * @throws StringIndexOutOfBoundsException if offset is invalid.
     */
    public StringBuffer insert(int offset, char[] str) {
        if (offset < 0 || offset > count) {
            throw new StringIndexOutOfBoundsException();
        }
        
        int len = str.length;
        ensureCapacity(count + len);
        
        // Make room for the inserted characters
        System.arraycopy(value, offset, value, offset + len, count - offset);
        
        // Copy the characters into the gap
        System.arraycopy(str, 0, value, offset, len);
        count += len;
        return this;
    }

    /**
     * Inserts the string representation of the boolean argument.
     *
     * @param offset the offset.
     * @param b a boolean.
     * @return a reference to this StringBuffer.
     * @throws StringIndexOutOfBoundsException if offset is invalid.
     */
    public StringBuffer insert(int offset, boolean b) {
        return insert(offset, String.valueOf(b));
    }

    /**
     * Inserts the string representation of the char argument.
     *
     * @param offset the offset.
     * @param c a char.
     * @return a reference to this StringBuffer.
     * @throws StringIndexOutOfBoundsException if offset is invalid.
     */
    public StringBuffer insert(int offset, char c) {
        if (offset < 0 || offset > count) {
            throw new StringIndexOutOfBoundsException();
        }
        
        ensureCapacity(count + 1);
        
        // Make room for the inserted character
        System.arraycopy(value, offset, value, offset + 1, count - offset);
        
        // Insert the character
        value[offset] = c;
        count++;
        return this;
    }

    /**
     * Inserts the string representation of the int argument.
     *
     * @param offset the offset.
     * @param i an int.
     * @return a reference to this StringBuffer.
     * @throws StringIndexOutOfBoundsException if offset is invalid.
     */
    public StringBuffer insert(int offset, int i) {
        return insert(offset, String.valueOf(i));
    }

    /**
     * Inserts the string representation of the long argument.
     *
     * @param offset the offset.
     * @param l a long.
     * @return a reference to this StringBuffer.
     * @throws StringIndexOutOfBoundsException if offset is invalid.
     */
    public StringBuffer insert(int offset, long l) {
        return insert(offset, String.valueOf(l));
    }

    /**
     * Inserts the string representation of the float argument.
     *
     * @param offset the offset.
     * @param f a float.
     * @return a reference to this StringBuffer.
     * @throws StringIndexOutOfBoundsException if offset is invalid.
     */
    public StringBuffer insert(int offset, float f) {
        return insert(offset, String.valueOf(f));
    }

    /**
     * Inserts the string representation of the double argument.
     *
     * @param offset the offset.
     * @param d a double.
     * @return a reference to this StringBuffer.
     * @throws StringIndexOutOfBoundsException if offset is invalid.
     */
    public StringBuffer insert(int offset, double d) {
        return insert(offset, String.valueOf(d));
    }

    /**
     * Reverses the sequence of characters in this StringBuffer.
     *
     * @return a reference to this StringBuffer.
     */
    public StringBuffer reverse() {
        int n = count - 1;
        for (int i = (n - 1) >> 1; i >= 0; i--) {
            int j = n - i;
            char c = value[i];
            value[i] = value[j];
            value[j] = c;
        }
        return this;
    }

    /**
     * Converts to a string representing the data in this string buffer.
     *
     * @return a string representation of the string buffer.
     */
    public String toString() {
        return new String(value, 0, count);
    }
}
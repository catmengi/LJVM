package java.lang;

/**
 * The <code>String</code> class represents character strings.
 * All string literals in Java programs, such as <code>"abc"</code>, are
 * implemented as instances of this class.
 * <p>
 * Strings are constant; their values cannot be changed after they are created.
 * String buffers support mutable strings.
 *
 * @author  CLDC 1.0, minimal embedded version
 * @version 1.0
 */
public final class String{

    // ── instance fields ──────────────────────────────────────
    private final char[] value;   // the characters
    private final int    offset;  // index of first character
    private final int    count;   // number of characters
    private int          hash;    // cached hash code (0 = not computed)


    // ── constructors ────────────────────────────────────────

    /** Constructs an empty String. (CLDC 1.0) */
    public String() {
        this.value  = new char[0];
        this.offset = 0;
        this.count  = 0;
    }

    /**
     * Constructs a new String that contains the same sequence of characters
     * as the specified string.
     */
    public String(String str) {
        this.value  = str.value;
        this.offset = str.offset;
        this.count  = str.count;
        // hash can be shared, but str.hash may not be set yet; we copy it if set.
        this.hash   = str.hash;
    }

    /**
     * Allocates a new String so that it represents the sequence of characters
     * currently contained in the character array argument.
     */
    public String(char[] chars) {
        this.value = new char[chars.length];
        System.arraycopy(chars, 0, this.value, 0, chars.length);
        this.offset = 0;
        this.count  = chars.length;
    }

    /**
     * Allocates a new String that contains characters from a subarray of the
     * character array argument.
     */
    public String(char[] chars, int start, int len) {
        if (start < 0) {
            throw new StringIndexOutOfBoundsException(start);
        }
        if (len < 0) {
            throw new StringIndexOutOfBoundsException(len);
        }
        if (start + len > chars.length) {
            throw new StringIndexOutOfBoundsException(start + len);
        }
        this.value = new char[len];
        System.arraycopy(chars, start, this.value, 0, len);
        this.offset = 0;
        this.count  = len;
    }

    /**
     * Construct a new String by converting the specified array of bytes
     * using the platform's default character encoding.
     * (CLDC 1.0 – no encoding parameter; uses platform default.)
     */
    public String(byte[] bytes) {
        this(bytes, 0, bytes.length);
    }

    /**
     * Construct a new String by converting the specified subarray of bytes
     * using the platform's default character encoding.
     */
    public String(byte[] bytes, int off, int len) {
        if (bytes == null) {
            throw new NullPointerException();
        }
        if (off < 0 || len < 0 || off + len > bytes.length) {
            throw new StringIndexOutOfBoundsException();
        }
        // Convert bytes to characters using ISO-8859-1 (Latin‑1),
        // which is the default charset for CLDC 1.0.
        char[] v = new char[len];
        for (int i = 0; i < len; i++) {
            v[i] = (char)(bytes[off + i] & 0xFF);
        }
        this.value  = v;
        this.offset = 0;
        this.count  = len;
    }

    // ── length / charAt ─────────────────────────────────────

    /** Returns the length of this string. */
    public int length() {
        return count;
    }

    /**
     * Returns the character at the specified index.
     *
     * @throws StringIndexOutOfBoundsException if index is negative or not less than length.
     */
    public char charAt(int index) {
        if (index < 0 || index >= count) {
            throw new StringIndexOutOfBoundsException(index);
        }
        return value[offset + index];
    }

    // ── comparison ──────────────────────────────────────────

    /**
     * Compares this string to the specified object.
     * The result is <code>true</code> if and only if the argument is not
     * <code>null</code> and is a <code>String</code> object that represents
     * the same sequence of characters as this object.
     */
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (obj instanceof String) {
            String another = (String) obj;
            int n = count;
            if (n == another.count) {
                char[] v1 = value;
                char[] v2 = another.value;
                int i = offset;
                int j = another.offset;
                while (n-- != 0) {
                    if (v1[i++] != v2[j++]) return false;
                }
                return true;
            }
        }
        return false;
    }

    /**
     * Compares this String to another String, ignoring case considerations.
     * Characters are converted to upper case using
     * {@link Character#toUpperCase(char)} before comparison.
     */
    public boolean equalsIgnoreCase(String anotherString) {
        if (anotherString == null) return false;
        if (this == anotherString) return true;
        int n = count;
        if (n != anotherString.count) return false;
        char[] v1 = value;
        char[] v2 = anotherString.value;
        int i = offset;
        int j = anotherString.offset;
        while (n-- != 0) {
            char c1 = Character.toUpperCase(v1[i++]);
            char c2 = Character.toUpperCase(v2[j++]);
            if (c1 != c2) return false;
        }
        return true;
    }

    /**
     * Compares two strings lexicographically.
     */
    public int compareTo(String anotherString) {
        int len1 = count;
        int len2 = anotherString.count;
        int lim = Math.min(len1, len2);
        char[] v1 = value;
        char[] v2 = anotherString.value;
        int i = offset;
        int j = anotherString.offset;
        while (lim-- != 0) {
            char c1 = v1[i++];
            char c2 = v2[j++];
            if (c1 != c2) {
                return c1 - c2;
            }
        }
        return len1 - len2;
    }

    // ── region matching ─────────────────────────────────────

    /**
     * Tests if two string regions are equal.
     */
    public boolean regionMatches(boolean ignoreCase,
                                 int toffset, String other, int ooffset, int len) {
        if (other == null) return false;
        if (toffset < 0 || ooffset < 0
                || toffset + len > this.count
                || ooffset + len > other.count) {
            return false;
        }
        char[] v1 = value;
        char[] v2 = other.value;
        int i = offset + toffset;
        int j = other.offset + ooffset;
        if (ignoreCase) {
            while (len-- != 0) {
                if (Character.toUpperCase(v1[i++]) !=
                    Character.toUpperCase(v2[j++])) {
                    return false;
                }
            }
        } else {
            while (len-- != 0) {
                if (v1[i++] != v2[j++]) {
                    return false;
                }
            }
        }
        return true;
    }

    // ── prefix/suffix ──────────────────────────────────────

    /** Tests if this string starts with the specified prefix. */
    public boolean startsWith(String prefix) {
        return startsWith(prefix, 0);
    }

    /** Tests if the substring beginning at <code>toffset</code> starts with the specified prefix. */
    public boolean startsWith(String prefix, int toffset) {
        if (toffset < 0 || toffset > count - prefix.count) return false;
        int i = offset + toffset;
        int j = prefix.offset;
        int k = prefix.count;
        while (k-- != 0) {
            if (value[i++] != prefix.value[j++]) return false;
        }
        return true;
    }

    /** Tests if this string ends with the specified suffix. */
    public boolean endsWith(String suffix) {
        return startsWith(suffix, count - suffix.count);
    }

    // ── hash code ───────────────────────────────────────────

    /**
     * Returns a hash code for this string.
     */
    public int hashCode() {
        int h = hash;
        if (h == 0 && count > 0) {
            int off = offset;
            char[] val = value;
            int len = count;
            for (int i = 0; i < len; i++) {
                h = 31 * h + val[off++];
            }
            hash = h;
        }
        return h;
    }

    // ── search ──────────────────────────────────────────────

    /**
     * Returns the index within this string of the first occurrence of the
     * specified character.
     */
    public int indexOf(int ch) {
        return indexOf(ch, 0);
    }

    /**
     * Returns the index within this string of the first occurrence of the
     * specified character, starting the search at <code>fromIndex</code>.
     */
    public int indexOf(int ch, int fromIndex) {
        int max = count;
        if (fromIndex < 0) fromIndex = 0;
        if (fromIndex >= max) return -1;
        if (ch < Character.MIN_VALUE || ch > Character.MAX_VALUE) return -1;
        for (int i = fromIndex; i < max; i++) {
            if (value[offset + i] == ch) return i;
        }
        return -1;
    }

    /**
     * Returns the index within this string of the last occurrence of the
     * specified character.
     */
    public int lastIndexOf(int ch) {
        return lastIndexOf(ch, count - 1);
    }

    /**
     * Returns the index within this string of the last occurrence of the
     * specified character, searching backward starting at <code>fromIndex</code>.
     */
    public int lastIndexOf(int ch, int fromIndex) {
        int min = 0;
        if (fromIndex >= count) fromIndex = count - 1;
        if (fromIndex < 0) return -1;
        for (int i = fromIndex; i >= min; i--) {
            if (value[offset + i] == ch) return i;
        }
        return -1;
    }

    /**
     * Returns the index within this string of the first occurrence of the
     * specified substring.
     */
    public int indexOf(String str) {
        return indexOf(str, 0);
    }

    /**
     * Returns the index within this string of the first occurrence of the
     * specified substring, starting at the specified index.
     */
    public int indexOf(String str, int fromIndex) {
        int slen = str.count;
        if (fromIndex < 0) fromIndex = 0;
        if (slen == 0) return (fromIndex <= count ? fromIndex : count);
        if (slen > count) return -1;
        char first = str.value[str.offset];
        int max = offset + (count - slen);
        for (int i = offset + fromIndex; i <= max; i++) {
            if (value[i] == first) {
                int j = i + 1;
                int end = j + slen - 1;
                int k = str.offset + 1;
                while (j < end) {
                    if (value[j++] != str.value[k++]) break;
                }
                if (j == end) return i - offset;
            }
        }
        return -1;
    }

    // ── substring ──────────────────────────────────────────

    /**
     * Returns a new string that is a substring of this string.
     * The substring begins with the character at the specified index and
     * extends to the end of this string.
     */
    public String substring(int beginIndex) {
        return substring(beginIndex, count);
    }

    /**
     * Returns a new string that is a substring of this string.
     * The substring begins at <code>beginIndex</code> and extends to
     * <code>endIndex - 1</code>.
     */
    public String substring(int beginIndex, int endIndex) {
        if (beginIndex < 0) {
            throw new StringIndexOutOfBoundsException(beginIndex);
        }
        if (endIndex > count) {
            throw new StringIndexOutOfBoundsException(endIndex);
        }
        if (beginIndex > endIndex) {
            throw new StringIndexOutOfBoundsException(endIndex - beginIndex);
        }
        // share the same char[] but with adjusted offset/length
        return new String(value, offset + beginIndex, endIndex - beginIndex);
    }

    // ── concat ─────────────────────────────────────────────

    /**
     * Concatenates the specified string to the end of this string.
     */
    public String concat(String str) {
        int otherLen = str.length();
        if (otherLen == 0) return this;
        char[] buf = new char[count + otherLen];
        System.arraycopy(value, offset, buf, 0, count);
        System.arraycopy(str.value, str.offset, buf, count, otherLen);
        return new String(buf);
    }

    // ── replace ────────────────────────────────────────────

    /**
     * Returns a new string resulting from replacing all occurrences of
     * <code>oldChar</code> in this string with <code>newChar</code>.
     */
    public String replace(char oldChar, char newChar) {
        if (oldChar != newChar) {
            int len = count;
            for (int i = 0; i < len; i++) {
                if (value[offset + i] == oldChar) {
                    // Found a match; allocate new array and copy + replace
                    char[] buf = new char[len];
                    System.arraycopy(value, offset, buf, 0, i);
                    buf[i] = newChar;
                    for (int j = i + 1; j < len; j++) {
                        char c = value[offset + j];
                        buf[j] = (c == oldChar) ? newChar : c;
                    }
                    return new String(buf);
                }
            }
        }
        return this;
    }

    // ── trim ──────────────────────────────────────────────

    /**
     * Returns a copy of the string, with leading and trailing whitespace omitted.
     */
    public String trim() {
        int len = count;
        int st = 0;
        while (st < len && value[offset + st] <= ' ') st++;
        while (st < len && value[offset + len - 1] <= ' ') len--;
        return (st > 0 || len < count) ? substring(st, len) : this;
    }

    // ── case conversion ───────────────────────────────────

    /**
     * Converts all of the characters in this <code>String</code> to lower case
     * using the rules of the default locale.
     */
    public String toLowerCase() {
        for (int i = 0; i < count; i++) {
            char c = value[offset + i];
            if (c != Character.toLowerCase(c)) {
                // need to create a new string
                char[] buf = new char[count];
                System.arraycopy(value, offset, buf, 0, i);
                buf[i] = Character.toLowerCase(c);
                for (int j = i + 1; j < count; j++) {
                    buf[j] = Character.toLowerCase(value[offset + j]);
                }
                return new String(buf);
            }
        }
        return this;
    }

    /**
     * Converts all of the characters in this <code>String</code> to upper case
     * using the rules of the default locale.
     */
    public String toUpperCase() {
        for (int i = 0; i < count; i++) {
            char c = value[offset + i];
            if (c != Character.toUpperCase(c)) {
                char[] buf = new char[count];
                System.arraycopy(value, offset, buf, 0, i);
                buf[i] = Character.toUpperCase(c);
                for (int j = i + 1; j < count; j++) {
                    buf[j] = Character.toUpperCase(value[offset + j]);
                }
                return new String(buf);
            }
        }
        return this;
    }

    // ── conversion to char arrays ─────────────────────────

    /**
     * Converts this string to a new character array.
     */
    public char[] toCharArray() {
        char[] result = new char[count];
        System.arraycopy(value, offset, result, 0, count);
        return result;
    }

    /**
     * Copies characters from this string into the destination character array.
     * (Package‑private, used by StringBuffer)
     */
    void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) {
        if (srcBegin < 0) throw new StringIndexOutOfBoundsException(srcBegin);
        if (srcEnd > count) throw new StringIndexOutOfBoundsException(srcEnd);
        if (srcBegin > srcEnd) throw new StringIndexOutOfBoundsException(srcEnd - srcBegin);
        System.arraycopy(value, offset + srcBegin, dst, dstBegin, srcEnd - srcBegin);
    }

    // ── toString ──────────────────────────────────────────

    /** Returns this same string. */
    public String toString() {
        return this;
    }

    // ── intern (native) ───────────────────────────────────

    /**
     * Returns a canonical representation for the string object.
     * A pool of strings, initially empty, is maintained privately by the VM.
     */
   //public native String intern();

    // ── valueOf (static factory methods) ─────────────────

    /** Returns the string representation of an <code>Object</code>. */
    public static String valueOf(Object obj) {
        return (obj == null) ? "null" : obj.toString();
    }

    /** Returns the string representation of a <code>boolean</code>. */
    public static String valueOf(boolean b) {
        return b ? "true" : "false";
    }

    /** Returns the string representation of a <code>char</code>. */
    public static String valueOf(char c) {
        return new String(new char[] { c });
    }

    /** Returns the string representation of an <code>int</code>. */
    public static String valueOf(int i) {
        return Integer.toString(i);
    }

    /** Returns the string representation of a <code>long</code>. */
    public static String valueOf(long l) {
        return Long.toString(l);
    }

    /** Returns the string representation of a <code>float</code>. */
    public static String valueOf(float f) {
        return Float.toString(f);
    }

    /** Returns the string representation of a <code>double</code>. */
    public static String valueOf(double d) {
        return Double.toString(d);
    }

    /** Returns the string representation of the <code>char</code> array argument. */
    public static String valueOf(char[] data) {
        return new String(data);
    }

    /** Returns the string representation of a specific subarray of the <code>char</code> array argument. */
    public static String valueOf(char[] data, int offset, int count) {
        return new String(data, offset, count);
    }
}
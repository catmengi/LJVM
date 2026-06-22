interface Verifiable {
    int verify();
}

class Node {
    Node next;
    int value;

    Node(int v) {
        value = v;
    }
}

class Data implements Verifiable {
    int a, b;
    Data self;               // for circular reference test

    Data(int x, int y) {
        a = x;
        b = y;
        self = this;         // create a self‑cycle
    }

    public int verify() {
        return a + b;
    }
}

class Container {
    static Verifiable keep;   // static root – holds an object across GC
    static int pass = 0;
}

public class dummy {

    public static native void debug_gc();
    public static native void debug_print(int print);
    static int passFlag = 0;

    // print a pass marker (42) or a fail marker (0)
    static synchronized void pass() {
        passFlag = 42;
        debug_print(42);
    }
    static void fail(int code) {
        debug_print(code);
    }

    public static synchronized void main(String[] args) {
        // -------- Test 1: allocate many tiny objects, force GC, check the root is safe
        Node root = new Node(0);
        Node add_cur = root;
        for (int i = 1; i < 100; i++) {
            add_cur.next = new Node(i);
            add_cur = add_cur.next;
        }
        // root now holds a linked list of 100 nodes – all reachable
        debug_gc();   // first GC – should preserve all 100 nodes

        // verify the list is intact by summing values
        int sum = 0;
        Node cur = root;
        while (cur != null) {
            sum = sum + cur.value;
            cur = cur.next;
        }
        if (sum != 4950) { fail(sum); return; }   // sum of 0..99 is 4950

        // -------- Test 2: interface dispatch after compaction
        Container.keep = new Data(10, 20);      // reachable via static field
       // debug_gc();   // second GC – moves objects, updates static field
        int verifyResult = Container.keep.verify();
        if (verifyResult != 30) { fail(2); return; }

        // -------- Test 3: circular reference (self‑cycle) survives GC
        Data d = new Data(5, 7);
        d.self = d;             // explicit self‑reference
        Container.keep = d;     // keep it reachable
        //debug_gc();             // third GC – must not crash on cycle
        if (d.verify() != 12) { fail(3); return; }

        // -------- Test 4: mass garbage, then verify the root still works
        // allocate a huge number of throw‑away objects
        for (int i = 0; i < 50000; i++) {
            new Node(i);        // all become garbage immediately
        }
        debug_gc();             // fourth GC – heap should be compacted, many dead objects
        // Container.keep must still be a valid Data(5,7)
        if (Container.keep.verify() != 12) { fail(4); return; }

        // -------- Test 5: allocate new objects after massive collection
        Node fresh = new Node(999);
        if (fresh.value != 999) { fail(5); return; }

        // -------- All tests passed
        pass();

        dummy[] a = new dummy[4];

        a[0] = new dummy();

        debug_gc();
    }
}
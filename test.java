import java.io.IOException;

class private_fieldC{
	public int second_field = 322;
	private static int field = 52;
	public static int getter(){
		return field;
	}
};

class test_app{
	static int field = 0;
	int second_field = 0;
	static String s = "Hello World!\n";

	private static int private_field = 1884;
	private static void private_method(int test_val){
		System.out.print("Private successful! Value:   ");
		System.out.println(test_val);
	}
	public static native void debug_segfault(String[] args);
	public static synchronized int test(int until){
		int a = 0;
		for(int i = 0; i < until; i++){
			a = i + i * i;
		}
		return a;
	}

	public static void expection_test() throws IOException{
		throw new IOException();
	}

	public static void main(String[] args){

		field = test(512);
		s = "Fuck";

		try {
			expection_test();
		} catch(IOException e){
			System.out.println("\n ============= \nExpection test successful!\n ============= \n");
		}

		

		System.out.println("Hello world!");
		System.out.println(s);
		System.out.println();
		System.out.println(0.1234f);
		System.out.println(0.1234567890123456789);
		System.out.println(1234);
		System.out.println(5456677l);

		private_method(private_field);
		System.out.println(private_fieldC.getter());

		private_fieldC C = new private_fieldC();

		System.out.println(C.second_field);
	}
}

import java.io.IOException;

class test_app{
	static int field = 0;
	int second_field = 0;
	static String s = "Hello World!\n";

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

		/*field = test(512);
		byte ln = 10;
		s = "Fuck";

		byte bytes[] = new byte[32];
		byte j = 67;
		for(byte i = 0; i < 31; i++){
				bytes[i] = j;
		}
		bytes[31] = ln;

		try {
			System.out.write(bytes);
		} catch (IOException e){
			debug_segfault(null);
		}

		try {
			expection_test();
		} catch(IOException e){
			byte nj = 70;
			byte[] new_bytes = new byte[16];
			for(byte i = 0; i < 16; i++){
				new_bytes[i] = nj;
			}
			new_bytes[15] = ln;

			try{
				System.out.write(new_bytes);
			}catch(IOException ne){}
		}
		*/

		System.out.println("Hello world!");
		System.out.println(0.1234f);
		System.out.println(0.1234567890123456789);
		System.out.println(1234);
		System.out.println(5456677l);
	}
}

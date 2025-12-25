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
	public static void main(String[] args){
		field = test(512);
		s = "Fuck";

		byte bytes[] = new byte[32];
		byte j = 67;
		for(byte i = 0; i < 31; i++){
				bytes[i] = j;
		}

		try {
			System.out.write(bytes);
		} catch (IOException e){

		}


		debug_segfault(args);
	}
}

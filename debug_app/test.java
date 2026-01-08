import java.util.*;

class thread implements Runnable{
	String msg = "Thread!!!!";
	int duration = 0;
	thread(String m, int d){
		msg = m;
		duration = d;
	}
		public void run(){
		for(int i = 0; i < duration; i++){
			System.out.println(msg);
		}
		System.exit(0);
	}
}
class test_app{
	public static native void debug_segfault(String[] args);


	public static void main(String[] args){
		System.out.println(String.valueOf(14.243474));

		StringBuffer sb = new StringBuffer();
		sb.append(7.654321).append(1.23456789f);
		//sb.append(false).append(1488).append(3.14d).append(2.88f).append("31 братушка");
		String ssb = sb.toString();

		System.out.println(ssb);

		Hashtable ht = new Hashtable();
		ht.put("hewo","there!");

		//System.out.println((String)ht.get("hewo"));
		//System.out.println((String)ht.get("Fuck!"));

		thread n = new thread("t1",4096);
		thread n2 = new thread("t2",1000000);
		Thread t = new Thread(n);
		Thread t2 = new Thread(n2);

		t.start();
		t2.start();

		//System.out.println("Fuck!");
		for(;;){}
	}
}

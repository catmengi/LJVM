import java.util.*;

class thread implements Runnable{
	String msg = "Thread!!!!";
	int duration = 0;
	thread(String m, int d){
		if(m != null) msg = m;
		duration = d;
	}
	synchronized public void run(){
		for(int i = 0; i < duration; i++){
			System.out.println(Thread.currentThread());
		}
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

		Thread threads[] = new Thread[256]; //Should exhaust current monitor pool
		for(int i = 0; i < threads.length; i++){
			threads[i] = new Thread(((Runnable)(new thread("thread",32))));
			threads[i].start();
		} 

		for(int i = 0; i < threads.length; i++){
			try {
				threads[i].join();
			}catch (Throwable e){e.printStackTrace();};
		} 
	}
}

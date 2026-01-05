import java.util.*;

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

		System.out.println((String)ht.get("hewo"));
		System.out.println((String)ht.get("Fuck!"));
	}
}

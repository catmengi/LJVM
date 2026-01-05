class test_app{
	public static native void debug_segfault(String[] args);
	public static void main(String[] args){
		StringBuffer sb = new StringBuffer();
		sb.append(false).append(false).append(false).append(false);
		String ssb = sb.toString();

		System.out.println(ssb);
	}
}

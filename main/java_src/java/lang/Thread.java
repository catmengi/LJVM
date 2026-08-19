package java.lang;

public class Thread implements Runnable{
    public static final int MIN_PRIORITY = 1;
    public static final int NORM_PRIORITY = 5;
    public static final int MAX_PRIORITY = 10;


    private long VMThread = 0; //Internal VM Thread_t pointer. Can be used for isAlive()
    private String name = "";
    private Runnable runnable = this;

    public Thread(){

    }

    public Thread(String name){
        this.name = name;
    }

    public Thread(Runnable target){
        this.runnable = target;
    }

    public Thread(Runnable target, String name){
        this.runnable = target;
        this.name = name;
    }

    public void run(){
        if(runnable != this){
            runnable.run();
        }
    }

    public void start(){
        if(VMThread == 0){
            //TODO: thread starting
        } else throw new IllegalThreadStateException();
    }

    public boolean isAlive(){
        return VMThread != 0 ? true : false;
    }
}

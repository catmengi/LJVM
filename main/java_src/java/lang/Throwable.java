package java.lang;

public class Throwable{
    private String message = null;
    private String[] stackTrace = null;

    public Throwable(){}
    public Throwable(String msg){
        message = msg;
    }

    public String getMessage(){
        return message;
    }

    public String toString(){
        return ((Object)this).toString() + ":" + (message != null ? getMessage() : "");
    }

    public void printStackTrace(){
        System.err.println(toString());
    }
}
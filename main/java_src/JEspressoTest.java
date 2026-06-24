public class JEspressoTest {
    // Общий объект-замок
    private static final Object lock = new Object();
    private static int sharedData = 0;

    // Этот метод ваша VM должна запустить в первом нативном потоке
    public static void runConsumer() {
        synchronized (lock) {
            while (sharedData == 0) {
                try {
                    lock.wait(); // Уходит в wait_set, отпускает замок, C-поток блокируется
                } catch (Exception e) {
                    // Обработка исключений (InterruptedException) в Java 6 обязательна
                }
            }
            // Если поток проснулся и кооперативно вернул замок:
            if (sharedData == 42) {
                // Успех! Логика отработала штатно
                //printsuccess(1);
            }
        }
    }

    // Этот метод ваша VM должна запустить во втором нативном потоке
    public static void runProducer() {
        synchronized (lock) {
            sharedData = 42;
            lock.notify(); // Нативный метод. Переводит Consumer в enter_set.
            // Из-за кооперативности Producer дорабатывает квоту опкодов до конца блока!
            //printsuccess(2);
        }
    }
}

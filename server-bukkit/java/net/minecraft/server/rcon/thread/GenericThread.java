// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.rcon.thread;
import java.util.concurrent.atomic.AtomicInteger;
import net.minecraft.DefaultUncaughtExceptionHandlerWithName;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
public abstract class GenericThread implements Runnable {
    private static final Logger kLog = LoggerFactory.getLogger(GenericThread.class);
    private static final AtomicInteger kNextId = new AtomicInteger();
    private static final int kStopWarnSeconds = 5;
    protected volatile boolean running;
    protected final String name;
    protected Thread thread;
    protected GenericThread(String name) {
        this.name = name;
    }
    public synchronized boolean start() {
        if (running) {
            return true;
        }
        running = true;
        thread = new Thread(this, name + " #" + kNextId.incrementAndGet());
        thread.setUncaughtExceptionHandler(new DefaultUncaughtExceptionHandlerWithName(kLog));
        thread.start();
        kLog.info("Thread {} started", name);
        return true;
    }
    public synchronized void stop() {
        running = false;
        if (thread == null) {
            return;
        }
        int waited = 0;
        while (thread.isAlive()) {
            try {
                thread.join(1000L);
                waited++;
                if (waited >= kStopWarnSeconds) {
                    kLog.warn("Waited {} seconds attempting force stop!", waited);
                } else if (thread.isAlive()) {
                    kLog.warn("Thread {} ({}) failed to exit after {} second(s)", this, thread.getState(), waited, new Exception("Stack:"));
                    thread.interrupt();
                }
            } catch (InterruptedException ignored) {
                // keep waiting
            }
        }
        kLog.info("Thread {} stopped", name);
        thread = null;
    }
    public boolean isRunning() {
        return running;
    }
}

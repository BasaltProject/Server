// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.concurrent.locks.ReentrantLock;
import net.minecraft.ReportedException;

public class ThreadingDetector {
    private final String name;
    private final ReentrantLock lock = new ReentrantLock();

    public ThreadingDetector(String name) {
        this.name = name;
    }

    public void checkAndLock() {
        lock.lock();
    }

    public void checkAndUnlock() {
        if (lock.isHeldByCurrentThread()) {
            lock.unlock();
        }
    }

    @Override
    public String toString() {
        return name;
    }
    public static ReportedException makeThreadingException(String name, Thread threadThatFailedToAcquire) {
        String error = "Accessing " + name + " from multiple threads";
        IllegalStateException cause = new IllegalStateException(error + "\n" + stackOf(Thread.currentThread())
            + (threadThatFailedToAcquire == null ? "" : "\n" + stackOf(threadThatFailedToAcquire)));
        return new ReportedException(new net.minecraft.CrashReport(error, cause));
    }
    private static String stackOf(Thread thread) {
        StringBuilder out = new StringBuilder(thread.getName()).append(':');
        for (StackTraceElement frame : thread.getStackTrace()) {
            out.append("\n\tat ").append(frame);
        }
        return out.toString();
    }
}

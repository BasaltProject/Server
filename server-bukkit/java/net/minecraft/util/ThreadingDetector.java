package net.minecraft.util;

import java.util.concurrent.Semaphore;

public class ThreadingDetector {
    private final String name;
    private Semaphore lock = new Semaphore(1);

    public ThreadingDetector(String name) {
        this.name = name;
    }

    public void checkAndLock() {
        lock.acquireUninterruptibly();
    }

    public void checkAndUnlock() {
        lock.release();
    }

    @Override
    public String toString() {
        return name;
    }
}

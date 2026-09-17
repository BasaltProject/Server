package net.minecraft.util;

import java.util.concurrent.locks.ReentrantLock;

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
}

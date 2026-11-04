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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.ThreadingDetector#makeThreadingException");
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dedicated;
import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.util.Arrays;
import java.util.Comparator;
import net.minecraft.CrashReport;
public class ServerWatchdog implements Runnable {
    private static final Comparator<ThreadInfo> kOrder = Comparator.comparing(ThreadInfo::isDaemon).thenComparing(ThreadInfo::getThreadState).thenComparing(ThreadInfo::getThreadName);
    private final DedicatedServer server;
    public ServerWatchdog(DedicatedServer server) {
        this.server = server;
    }
    public void run() {
        // the native tick supervisor owns hang detection, so this loop has nothing left to watch
    }
    public static CrashReport createWatchdogCrashReport(String message, long mainThreadId) {
        ThreadInfo[] threads = ManagementFactory.getThreadMXBean().dumpAllThreads(true, true);
        Arrays.sort(threads, kOrder);
        StringBuilder dump = new StringBuilder();
        Error error = new Error("Watchdog (" + message + ")");
        for (ThreadInfo thread : threads) {
            if (thread.getThreadId() == mainThreadId) {
                error.setStackTrace(thread.getStackTrace());
            }
            dump.append('\n').append(thread);
        }
        CrashReport report = new CrashReport(message, error);
        report.addCategory("Thread Dump").setDetail("Threads", dump);
        return report;
    }
}

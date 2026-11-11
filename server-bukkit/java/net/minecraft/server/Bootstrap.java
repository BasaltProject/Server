// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import java.io.PrintStream;
import java.util.Set;
import java.util.concurrent.atomic.AtomicLong;
import java.util.function.Supplier;
import net.minecraft.locale.Language;

public class Bootstrap {
    public static final PrintStream STDOUT = System.out;
    public static final AtomicLong bootstrapDuration = new AtomicLong(-1L);
    private static volatile boolean bootstrapped;

    public static void bootStrap() {
        if (bootstrapped) {
            return;
        }
        long started = System.nanoTime();
        bootstrapped = true;
        bootstrapDuration.set((System.nanoTime() - started) / 1_000_000L);
    }

    public static Set<String> getMissingTranslations(Language language) {
        return Set.of();
    }

    public static void checkBootstrapCalled(Supplier<String> location) {
        if (!bootstrapped) {
            throw new IllegalArgumentException("Not bootstrapped (called from " + location.get() + ")");
        }
    }

    public static void validate() {
        checkBootstrapCalled(() -> "validate");
    }

    public static void realStdoutPrintln(String string) {
        STDOUT.println(string);
    }

    public static void shutdownStdout() {
        STDOUT.close();
    }
}

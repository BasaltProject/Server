// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.ArrayList;
import java.util.List;
import java.util.stream.Stream;
import net.minecraft.core.Holder;

public class FailedTestTracker {
    private static final List<Holder.Reference<GameTestInstance>> LAST_FAILED = new ArrayList<>();

    public static synchronized Stream<Holder.Reference<GameTestInstance>> getLastFailedTests() {
        return List.copyOf(LAST_FAILED).stream();
    }

    public static synchronized void rememberFailedTest(Holder.Reference<GameTestInstance> test) {
        LAST_FAILED.add(test);
    }

    public static synchronized void forgetFailedTests() {
        LAST_FAILED.clear();
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import java.util.ArrayDeque;
import java.util.Deque;
import java.util.LinkedHashMap;
import java.util.Map;

public class SuppressedExceptionCollector {
    private static final int RECENT_LIMIT = 8;
    private final Deque<LongEntry> recent = new ArrayDeque<>();
    private final Map<ShortEntry, Integer> counts = new LinkedHashMap<>();

    public synchronized void addEntry(String location, Throwable throwable) {
        recent.addLast(new LongEntry(System.currentTimeMillis(), location, throwable.getClass(), throwable.getMessage()));
        while (recent.size() > RECENT_LIMIT) {
            recent.removeFirst();
        }
        counts.merge(new ShortEntry(location, throwable.getClass()), 1, Integer::sum);
    }

    public synchronized String dump() {
        long now = System.currentTimeMillis();
        StringBuilder out = new StringBuilder();
        if (!recent.isEmpty()) {
            out.append("\n\t\tLatest entries:\n");
            for (LongEntry entry : recent) {
                out.append("\t\t\t").append(entry.location).append(':').append(entry.cls).append(": ").append(entry.message)
                    .append(" (").append(now - entry.timestampMs).append("ms ago)\n");
            }
        }
        if (!counts.isEmpty()) {
            out.append(out.isEmpty() ? "\n" : "").append("\t\tEntry counts:\n");
            counts.forEach((key, count) -> out.append("\t\t\t").append(key.location).append(':').append(key.cls).append(" x ").append(count).append('\n'));
        }
        return out.isEmpty() ? "~~NONE~~" : out.toString();
    }

    static record LongEntry(long timestampMs, String location, Class<? extends Throwable> cls, String message) {}

    static record ShortEntry(String location, Class<? extends Throwable> cls) {}
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.block.state.BlockState;

public class CrashReportCategory {
    private final String title;
    private final List<Entry> entries = new ArrayList<>();
    private StackTraceElement[] stackTrace = new StackTraceElement[0];

    public CrashReportCategory(String title) {
        this.title = title;
    }

    public static String formatLocation(LevelHeightAccessor heights, double x, double y, double z) {
        return String.format(Locale.ROOT, "%.2f,%.2f,%.2f - %s", x, y, z,
            formatLocation(heights, BlockPos.containing(x, y, z)));
    }

    public static String formatLocation(LevelHeightAccessor heights, BlockPos pos) {
        return formatLocation(heights, pos.getX(), pos.getY(), pos.getZ());
    }

    public static String formatLocation(LevelHeightAccessor heights, int x, int y, int z) {
        StringBuilder text = new StringBuilder();
        text.append(String.format(Locale.ROOT, "World: (%d,%d,%d), ", x, y, z));
        try {
            int sx = x >> 4;
            int sy = y >> 4;
            int sz = z >> 4;
            text.append(String.format(Locale.ROOT,
                "Section: (at %d,%d,%d in %d,%d,%d; chunk contains blocks %d,%d,%d to %d,%d,%d), ", x & 15, y & 15,
                z & 15, sx, sy, sz, sx << 4, heights.getMinY(), sz << 4, (sx << 4) + 15, heights.getMaxY(),
                (sz << 4) + 15));
        } catch (Throwable failure) {
            text.append("(Error finding chunk loc), ");
        }
        int rx = x >> 9;
        int rz = z >> 9;
        text.append(String.format(Locale.ROOT, "Region: (%d,%d; contains chunks %d,%d to %d,%d, blocks %d,%d,%d to %d,%d,%d)",
            rx, rz, rx << 5, rz << 5, (rx << 5) + 31, (rz << 5) + 31, rx << 9, heights.getMinY(), rz << 9,
            (rx << 9) + 511, heights.getMaxY(), (rz << 9) + 511));
        return text.toString();
    }

    public CrashReportCategory setDetail(String key, CrashReportDetail<String> callback) {
        try {
            return setDetail(key, (Object) callback.call());
        } catch (Throwable failure) {
            setDetailError(key, failure);
            return this;
        }
    }

    public CrashReportCategory setDetail(String key, Object value) {
        entries.add(new Entry(key, value));
        return this;
    }

    public void setDetailError(String key, Throwable failure) {
        setDetail(key, (Object) failure);
    }

    public int fillInStackTrace(int nestedOffset) {
        StackTraceElement[] full = Thread.currentThread().getStackTrace();
        int skip = 3 + nestedOffset;
        if (full.length <= skip) {
            return 0;
        }
        stackTrace = java.util.Arrays.copyOfRange(full, skip, full.length);
        return stackTrace.length;
    }

    public boolean validateStackTrace(StackTraceElement source, StackTraceElement next) {
        if (stackTrace.length == 0 || source == null) {
            return false;
        }
        StackTraceElement current = stackTrace[0];
        if (current.isNativeMethod() != source.isNativeMethod()
            || !java.util.Objects.equals(current.getClassName(), source.getClassName())
            || !java.util.Objects.equals(current.getFileName(), source.getFileName())
            || !java.util.Objects.equals(current.getMethodName(), source.getMethodName())) {
            return false;
        }
        if (next != null != stackTrace.length > 1 || next != null && !stackTrace[1].equals(next)) {
            return false;
        }
        stackTrace[0] = source;
        return true;
    }

    public void getDetails(StringBuilder builder) {
        builder.append("-- ").append(title).append(" --\nDetails:");
        for (Entry entry : entries) {
            builder.append("\n\t").append(entry.key()).append(": ").append(entry.value());
        }
        if (stackTrace.length > 0) {
            builder.append("\nStacktrace:");
            for (StackTraceElement element : stackTrace) {
                builder.append("\n\tat ").append(element);
            }
        }
    }

    public StackTraceElement[] getStacktrace() {
        return stackTrace;
    }

    public static void populateBlockDetails(CrashReportCategory category, LevelHeightAccessor heights, BlockPos pos,
            BlockState state) {
        category.setDetail("Block", state::toString);
        populateBlockLocationDetails(category, heights, pos);
    }

    public static CrashReportCategory populateBlockLocationDetails(CrashReportCategory category,
            LevelHeightAccessor heights, BlockPos pos) {
        return category.setDetail("Block location", () -> formatLocation(heights, pos));
    }

    public static record Entry(String key, String value) {
        public Entry(String key, Object rawValue) {
            this(key, rawValue == null ? "~~NULL~~"
                : rawValue instanceof Throwable failure
                    ? "~~ERROR~~ " + failure.getClass().getSimpleName() + ": " + failure.getMessage()
                    : rawValue.toString());
        }
    }
}

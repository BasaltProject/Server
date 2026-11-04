// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.profiling;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
public class FilledProfileResults implements ProfileResults {
    private final Map<String, ? extends ProfilerPathEntry> entries;
    private final long startTimeNano;
    private final int startTimeTicks;
    private final long endTimeNano;
    private final int endTimeTicks;
    public FilledProfileResults(Map<String, ? extends ProfilerPathEntry> entries, long startTimeNano, int startTimeTicks, long endTimeNano, int endTimeTicks) {
        this.entries = entries;
        this.startTimeNano = startTimeNano;
        this.startTimeTicks = startTimeTicks;
        this.endTimeNano = endTimeNano;
        this.endTimeTicks = endTimeTicks;
    }
    private long durationOf(String path) {
        ProfilerPathEntry entry = entries.get(path);
        return entry == null ? 0L : entry.getDuration();
    }
    @Override
    public List<ResultField> getTimes(String path) {
        String root = ProfilerFiller.ROOT;
        long rootTime = durationOf(root);
        long parentTime = path.isEmpty() ? rootTime : durationOf(path);
        String prefix = path.isEmpty() ? "" : path + ProfileResults.PATH_SEPARATOR;
        List<ResultField> out = new ArrayList<>();
        long childrenTotal = 0L;
        for (Map.Entry<String, ? extends ProfilerPathEntry> entry : entries.entrySet()) {
            String key = entry.getKey();
            if (key.length() > prefix.length() && key.startsWith(prefix) && key.indexOf(ProfileResults.PATH_SEPARATOR, prefix.length() + 1) < 0) {
                long time = entry.getValue().getDuration();
                childrenTotal += time;
                out.add(new ResultField(key.substring(prefix.length()), parentTime == 0 ? 0.0 : time * 100.0 / parentTime,
                    rootTime == 0 ? 0.0 : time * 100.0 / rootTime, entry.getValue().getCount()));
            }
        }
        if (!path.isEmpty() && parentTime > childrenTotal) {
            out.add(new ResultField("unspecified", (parentTime - childrenTotal) * 100.0 / parentTime,
                rootTime == 0 ? 0.0 : (parentTime - childrenTotal) * 100.0 / rootTime, 0L));
        }
        Collections.sort(out);
        out.add(0, new ResultField(path.isEmpty() ? ProfilerFiller.ROOT : path.substring(path.lastIndexOf(ProfileResults.PATH_SEPARATOR) + 1),
            100.0, rootTime == 0 ? 0.0 : parentTime * 100.0 / rootTime, 0L));
        return out;
    }
    @Override
    public boolean saveResults(Path file) {
        try {
            Files.writeString(file, getProfilerResults());
            return true;
        } catch (IOException failure) {
            return false;
        }
    }
    @Override
    public long getStartTimeNano() {
        return startTimeNano;
    }
    @Override
    public int getStartTimeTicks() {
        return startTimeTicks;
    }
    @Override
    public long getEndTimeNano() {
        return endTimeNano;
    }
    @Override
    public int getEndTimeTicks() {
        return endTimeTicks;
    }
    @Override
    public String getProfilerResults() {
        StringBuilder out = new StringBuilder();
        out.append("---- Minecraft Profiler Results ----\n");
        out.append("Time span: ").append(getNanoDuration() / 1000000L).append(" ms\n");
        out.append("Tick span: ").append(getTickDuration()).append(" ticks\n\n");
        append(out, 0, ProfilerFiller.ROOT);
        return out.toString();
    }
    private void append(StringBuilder out, int depth, String path) {
        List<ResultField> fields = getTimes(path);
        for (int i = 1; i < fields.size(); i++) {
            ResultField field = fields.get(i);
            out.append("  ".repeat(depth)).append('[').append(String.format("%02d", depth)).append("] ").append(field.name)
                .append(" - ").append(String.format("%.2f", field.percentage)).append("%/").append(String.format("%.2f", field.globalPercentage))
                .append("%\n");
            if (!"unspecified".equals(field.name)) {
                append(out, depth + 1, path + ProfileResults.PATH_SEPARATOR + field.name);
            }
        }
    }
}

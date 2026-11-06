// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;

import java.util.LinkedHashMap;
import java.util.Map;

public class SystemReport {
    public static final long BYTES_PER_MEBIBYTE = 1048576L;
    private final Map<String, String> entries = new LinkedHashMap<>();

    public SystemReport() {
        setDetail("Java Version", System.getProperty("java.version"));
        setDetail("Operating System", System.getProperty("os.name") + " (" + System.getProperty("os.arch") + ") version " + System.getProperty("os.version"));
        Runtime runtime = Runtime.getRuntime();
        setDetail("Memory", runtime.freeMemory() + " bytes (" + (runtime.freeMemory() / BYTES_PER_MEBIBYTE) + " MiB) / " + runtime.totalMemory() + " bytes");
        setDetail("Processors", Integer.toString(runtime.availableProcessors()));
    }

    public void setDetail(String key, String value) {
        entries.put(key, value);
    }

    public void setDetail(String key, CrashReportDetail<Object> valueSupplier) {
        try {
            setDetail(key, String.valueOf(valueSupplier.call()));
        } catch (Exception failure) {
            setDetail(key, "ERR " + failure);
        }
    }

    public static float sizeInMiB(long bytes) {
        return (float) bytes / (float) BYTES_PER_MEBIBYTE;
    }

    public void appendToCrashReportString(StringBuilder sb) {
        sb.append("-- System Details --\n");
        entries.forEach((key, value) -> sb.append("\t").append(key).append(": ").append(value).append('\n'));
    }

    public String toLineSeparatedString() {
        StringBuilder out = new StringBuilder();
        entries.forEach((key, value) -> out.append(key).append(": ").append(value).append('\n'));
        return out.toString();
    }
}

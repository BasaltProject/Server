// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server;

import com.google.common.collect.Lists;
import java.io.IOException;
import java.util.List;
import org.apache.commons.lang3.StringUtils;

public class ChainedJsonException extends IOException {
    private final List<Entry> entries = Lists.newArrayList();
    private final String message;

    public ChainedJsonException(String message) {
        entries.add(new Entry());
        this.message = message;
    }

    public ChainedJsonException(String message, Throwable cause) {
        super(cause);
        entries.add(new Entry());
        this.message = message;
    }

    public void prependJsonKey(String key) {
        entries.get(0).addJsonKey(key);
    }

    public void setFilenameAndFlush(String filename) {
        entries.get(0).filename = filename;
        entries.add(0, new Entry());
    }

    public String getMessage() {
        return "Invalid " + entries.get(entries.size() - 1) + ": " + message;
    }

    public static ChainedJsonException forException(Exception e) {
        if (e instanceof ChainedJsonException chained) {
            return chained;
        }
        String detail = e.getMessage();
        if (e instanceof java.io.FileNotFoundException) {
            detail = "File not found";
        }
        return new ChainedJsonException(detail, e);
    }

    public static class Entry {
        private String filename;
        private final List<String> jsonKeys = Lists.newArrayList();

        private Entry() {}

        private void addJsonKey(String key) {
            jsonKeys.add(0, key);
        }

        public String getFilename() {
            return filename;
        }

        public String getJsonKeys() {
            return StringUtils.join(jsonKeys, "->");
        }

        public String toString() {
            if (filename != null) {
                return jsonKeys.isEmpty() ? filename : filename + " " + getJsonKeys();
            }
            return jsonKeys.isEmpty() ? "(Unknown file)" : "(Unknown file) " + getJsonKeys();
        }
    }
}

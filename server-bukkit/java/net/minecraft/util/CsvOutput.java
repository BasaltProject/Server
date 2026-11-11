// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.io.IOException;
import java.io.Writer;
import java.util.ArrayList;
import java.util.List;

public class CsvOutput {
    private static final String kSeparator = ",";
    private final Writer output;
    private final int columnCount;

    private CsvOutput(Writer output, List<String> headers) throws IOException {
        this.output = output;
        this.columnCount = headers.size();
        writeLine(headers.stream());
    }

    public static Builder builder() {
        return new Builder();
    }

    public void writeRow(Object... values) throws IOException {
        if (values.length != columnCount) {
            throw new IllegalArgumentException("Invalid number of columns, expected " + columnCount + ", but got " + values.length);
        }
        writeLine(java.util.Arrays.stream(values).map(CsvOutput::format));
    }

    private void writeLine(java.util.stream.Stream<?> cells) throws IOException {
        output.write(cells.map(String::valueOf).collect(java.util.stream.Collectors.joining(kSeparator)) + "\r\n");
    }

    private static String format(Object value) {
        String text = value != null ? value.toString() : "[null]";
        return text.contains(kSeparator) || text.contains("\"") ? '"' + text.replace("\"", "\"\"") + '"' : text;
    }

    public static class Builder {
        private final List<String> headers = new ArrayList<>();

        public Builder addColumn(String header) {
            headers.add(header);
            return this;
        }

        public CsvOutput build(Writer writer) throws IOException {
            return new CsvOutput(writer, headers);
        }
    }
}

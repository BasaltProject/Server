// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.TransformerException;

public class JUnitLikeTestReporter implements TestReporter {
    private record Entry(String name, long millis, String failure) {
    }

    private final List<Entry> entries = new ArrayList<>();
    private final File destination;

    public JUnitLikeTestReporter(File destination) throws ParserConfigurationException {
        this.destination = destination;
    }

    private static String escape(String text) {
        return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;");
    }

    @Override
    public void onTestFailed(GameTestInfo testInfo) {
        String reason = testInfo.getError() == null ? "failed" : testInfo.getError().getMessage();
        entries.add(new Entry(testInfo.id().toString(), testInfo.getRunTime(), reason));
    }

    @Override
    public void onTestSuccess(GameTestInfo testInfo) {
        entries.add(new Entry(testInfo.id().toString(), testInfo.getRunTime(), null));
    }

    @Override
    public void finish() {
        try {
            save(destination);
        } catch (TransformerException e) {
            throw new IllegalStateException("Could not write the test report", e);
        }
    }

    public void save(File file) throws TransformerException {
        long failures = entries.stream().filter(entry -> entry.failure() != null).count();
        StringBuilder out = new StringBuilder("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        out.append("<testsuite name=\"root\" tests=\"").append(entries.size()).append("\" failures=\"").append(failures).append("\">\n");
        for (Entry entry : entries) {
            out.append("  <testcase name=\"").append(escape(entry.name())).append("\" time=\"").append(entry.millis() / 1000.0).append('"');
            if (entry.failure() == null) {
                out.append("/>\n");
            } else {
                out.append(">\n    <failure message=\"").append(escape(entry.failure())).append("\"/>\n  </testcase>\n");
            }
        }
        out.append("</testsuite>\n");
        try {
            if (file.getParentFile() != null) {
                Files.createDirectories(file.getParentFile().toPath());
            }
            Files.writeString(file.toPath(), out, StandardCharsets.UTF_8);
        } catch (IOException e) {
            throw new TransformerException(e);
        }
    }
}

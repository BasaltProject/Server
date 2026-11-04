// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
public class CrashReport {
    private final String title;
    private final Throwable exception;
    private final List<CrashReportCategory> details = new ArrayList<>();
    private Path saveFile;
    public CrashReport(String title, Throwable t) {
        this.title = title;
        this.exception = t;
    }
    public String getTitle() {
        return title;
    }
    public Throwable getException() {
        return exception;
    }
    public String getDetails() {
        StringBuilder builder = new StringBuilder();
        getDetails(builder);
        return builder.toString();
    }
    public void getDetails(StringBuilder builder) {
        builder.append("Description: ").append(title).append("\n\n");
        StringWriter trace = new StringWriter();
        exception.printStackTrace(new PrintWriter(trace));
        builder.append(trace);
    }
    public String getExceptionMessage() {
        return exception.getClass().getName() + ": " + exception.getMessage();
    }
    public String getFriendlyReport(ReportType reportType, List<String> extraComments) {
        return String.join("\n", extraComments) + "\n" + getDetails();
    }
    public String getFriendlyReport(ReportType reportType) {
        return getFriendlyReport(reportType, List.of());
    }
    public Path getSaveFile() {
        return saveFile;
    }
    public boolean saveToFile(Path file, ReportType reportType, List<String> extraComments) {
        try {
            Files.writeString(file, getFriendlyReport(reportType, extraComments));
            saveFile = file;
            return true;
        } catch (java.io.IOException failure) {
            return false;
        }
    }
    public boolean saveToFile(Path file, ReportType reportType) {
        return saveToFile(file, reportType, List.of());
    }
    public SystemReport getSystemReport() {
        return new SystemReport();
    }
    public CrashReportCategory addCategory(String name) {
        return addCategory(name, 0);
    }
    public CrashReportCategory addCategory(String name, int nestedOffset) {
        CrashReportCategory category = new CrashReportCategory(name);
        details.add(category);
        return category;
    }
    public static CrashReport forThrowable(Throwable t, String title) {
        while (t instanceof java.util.concurrent.CompletionException && t.getCause() != null) {
            t = t.getCause();
        }
        return t instanceof ReportedException reported ? reported.getReport() : new CrashReport(title, t);
    }
    public static void preload() {}
}

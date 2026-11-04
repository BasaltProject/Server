// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;
public class ReportedException extends RuntimeException {
    private final CrashReport report;
    public ReportedException(CrashReport report) {
        this.report = report;
    }
    public CrashReport getReport() {
        return report;
    }
    @Override
    public Throwable getCause() {
        return report.getException();
    }
    @Override
    public String getMessage() {
        return report.getTitle();
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix;

import java.util.List;
import net.minecraft.CrashReport;
import net.minecraft.CrashReportCategory;
import net.minecraft.util.filefix.virtualfilesystem.FileMove;

public final class AbortedFileFixException extends FileFixException {
    private final List<FileMove> notRevertedMoves;

    public AbortedFileFixException(Exception cause, List<FileMove> notRevertedMoves,
            FileSystemCapabilities fileSystemCapabilities) {
        super(cause, fileSystemCapabilities);
        this.notRevertedMoves = notRevertedMoves;
    }

    public AbortedFileFixException(Exception cause) {
        this(cause, List.of(), null);
    }

    public List<FileMove> notRevertedMoves() {
        return notRevertedMoves;
    }

    @Override
    protected CrashReport createCrashReport() {
        CrashReport report = super.createCrashReport();
        CrashReportCategory category = report.addCategory("Moves that failed to revert");
        for (int index = 0; index < notRevertedMoves.size(); index++) {
            FileMove move = notRevertedMoves.get(index);
            category.setDetail(String.valueOf(index), move.from() + " -> " + move.to());
        }
        return report;
    }
}

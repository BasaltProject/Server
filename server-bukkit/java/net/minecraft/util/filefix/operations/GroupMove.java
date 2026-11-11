// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.operations;

import java.io.IOException;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import net.minecraft.util.worldupdate.UpgradeProgress;

public record GroupMove(Map<String, String> fromTo, List<Move> fixers) implements FileFixOperation {
    @Override
    public void fix(Path baseDirectory, UpgradeProgress progress) throws IOException {
        for (Map.Entry<String, String> folders : fromTo.entrySet()) {
            for (Move move : fixers) {
                move.relative(folders.getKey(), folders.getValue()).fix(baseDirectory, progress);
            }
        }
    }
}

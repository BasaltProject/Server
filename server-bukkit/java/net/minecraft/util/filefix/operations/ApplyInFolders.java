// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.operations;

import java.io.IOException;
import java.nio.file.Path;
import java.util.List;
import net.minecraft.util.filefix.access.FileRelation;
import net.minecraft.util.worldupdate.UpgradeProgress;

public record ApplyInFolders(FileRelation folders, List<FileFixOperation> fileFixOperations)
        implements FileFixOperation {
    @Override
    public void fix(Path baseDirectory, UpgradeProgress progress) throws IOException {
        for (Path folder : folders.getPaths(baseDirectory)) {
            for (FileFixOperation operation : fileFixOperations) {
                operation.fix(folder, progress);
            }
        }
    }
}

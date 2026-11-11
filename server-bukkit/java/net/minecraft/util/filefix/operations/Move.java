// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.operations;

import java.io.IOException;
import java.nio.file.Path;
import net.minecraft.util.filefix.FileFixUtil;
import net.minecraft.util.worldupdate.UpgradeProgress;

public record Move(String from, String to) implements FileFixOperation {
    public Move relative(String sourceFolder, String targetFolder) {
        return new Move(sourceFolder + "/" + from, targetFolder + "/" + to);
    }

    @Override
    public void fix(Path baseDirectory, UpgradeProgress progress) throws IOException {
        FileFixUtil.moveFile(baseDirectory, from, to);
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.fixes;

import com.mojang.datafixers.schemas.Schema;
import net.minecraft.util.filefix.FileFix;
import net.minecraft.util.filefix.operations.FileFixOperations;

public class PlayerStorageFileFix extends FileFix {
    public PlayerStorageFileFix(Schema schema) {
        super(schema);
    }

    @Override
    public void makeFixer() {
        addFileFixOperation(FileFixOperations.move("advancements", "players/advancements"));
        addFileFixOperation(FileFixOperations.move("playerdata", "players/data"));
        addFileFixOperation(FileFixOperations.move("stats", "players/stats"));
    }
}

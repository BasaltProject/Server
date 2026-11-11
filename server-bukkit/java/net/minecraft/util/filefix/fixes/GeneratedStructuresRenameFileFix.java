// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.fixes;

import com.mojang.datafixers.schemas.Schema;
import java.util.List;
import net.minecraft.util.filefix.FileFix;
import net.minecraft.util.filefix.access.FileRelation;
import net.minecraft.util.filefix.operations.FileFixOperations;

public class GeneratedStructuresRenameFileFix extends FileFix {
    public GeneratedStructuresRenameFileFix(Schema schema) {
        super(schema);
    }

    @Override
    public void makeFixer() {
        addFileFixOperation(FileFixOperations.applyInFolders(FileRelation.GENERATED_NAMESPACES,
            List.of(FileFixOperations.move("structures", "structure"))));
    }
}

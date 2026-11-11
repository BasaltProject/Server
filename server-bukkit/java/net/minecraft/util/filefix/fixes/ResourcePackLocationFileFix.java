// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.fixes;

import com.mojang.datafixers.schemas.Schema;
import net.minecraft.util.filefix.FileFix;
import net.minecraft.util.filefix.operations.FileFixOperations;

public class ResourcePackLocationFileFix extends FileFix {
    public ResourcePackLocationFileFix(Schema schema) {
        super(schema);
    }

    @Override
    public void makeFixer() {
        addFileFixOperation(FileFixOperations.move("resources.zip", "resourcepacks/resources.zip"));
    }
}

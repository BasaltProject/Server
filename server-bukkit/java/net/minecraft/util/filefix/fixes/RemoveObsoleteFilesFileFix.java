// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.fixes;

import com.mojang.datafixers.schemas.Schema;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.util.filefix.FileFix;
import net.minecraft.util.filefix.operations.FileFixOperations;

public class RemoveObsoleteFilesFileFix extends FileFix {
    public RemoveObsoleteFilesFileFix(Schema schema) {
        super(schema);
    }

    @Override
    public void makeFixer() {
        for (String obsolete : List.of("data/villages.dat", "data/villages_end.dat", "data/villages_nether.dat",
                "data/advancements", "data/functions")) {
            addFileFixOperation(FileFixOperations.delete(obsolete));
        }
        List<String> leftovers = new ArrayList<>(LegacyStructureFileFix.OVERWORLD_LEGACY_STRUCTURES);
        leftovers.add("Village");
        leftovers.addAll(LegacyStructureFileFix.NETHER_LEGACY_STRUCTURES);
        leftovers.addAll(LegacyStructureFileFix.END_LEGACY_STRUCTURES);
        for (String structure : leftovers) {
            addFileFixOperation(FileFixOperations.delete("data/" + structure + ".dat"));
            addFileFixOperation(FileFixOperations.delete("data/" + structure + "_index.dat"));
        }
    }
}

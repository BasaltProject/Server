// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.access;

import com.mojang.datafixers.DSL;
import com.mojang.serialization.Dynamic;
import java.io.IOException;
import java.nio.file.Path;
import java.util.Optional;
import net.minecraft.nbt.NbtUtils;
import net.minecraft.nbt.Tag;
import net.minecraft.util.datafix.DataFixers;

public class SavedDataNbt extends CompressedNbt {
    private final DSL.TypeReference type;
    private final int targetVersion;

    public SavedDataNbt(DSL.TypeReference type, Path path, int targetVersion, MissingSeverity missingSeverity) {
        super(path, missingSeverity);
        this.type = type;
        this.targetVersion = targetVersion;
    }

    @Override
    public Optional<Dynamic<Tag>> read() throws IOException {
        return readFile().map(file -> {
            int version = NbtUtils.getDataVersion(file);
            Dynamic<Tag> fixed = type == null || version >= targetVersion ? file
                : DataFixers.getDataFixer().update(type, file, version, targetVersion);
            return fixed.get("data").orElseEmptyMap();
        });
    }

    @Override
    public <T> void write(Dynamic<T> data) {
        writeFile(NbtUtils.addDataVersion(data.emptyMap().set("data", data), targetVersion));
    }
}

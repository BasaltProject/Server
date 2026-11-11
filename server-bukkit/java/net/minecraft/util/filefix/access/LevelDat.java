// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.access;

import com.mojang.serialization.Dynamic;
import java.io.IOException;
import java.nio.file.Path;
import java.util.Optional;
import net.minecraft.nbt.NbtUtils;
import net.minecraft.nbt.Tag;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.util.datafix.DataFixers;

public class LevelDat extends CompressedNbt {
    private final int targetVersion;

    public LevelDat(Path path, int targetVersion) {
        super(path, MissingSeverity.IMPORTANT);
        this.targetVersion = targetVersion;
    }

    @Override
    public Optional<Dynamic<Tag>> read() throws IOException {
        return readFile().map(file -> {
            Dynamic<Tag> content = file.get("Data").orElseEmptyMap();
            return DataFixTypes.LEVEL.update(DataFixers.getDataFixer(), content, NbtUtils.getDataVersion(content),
                targetVersion);
        });
    }

    @Override
    public <T> void write(Dynamic<T> data) {
        writeFile(data.emptyMap().set("Data", NbtUtils.addDataVersion(data, targetVersion)));
    }
}

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

public class PlayerData extends CompressedNbt {
    private final int targetVersion;

    public PlayerData(Path path, int targetVersion) {
        super(path, MissingSeverity.NEUTRAL);
        this.targetVersion = targetVersion;
    }

    @Override
    public Optional<Dynamic<Tag>> read() throws IOException {
        return readFile().map(file -> DataFixTypes.PLAYER.update(DataFixers.getDataFixer(), file,
            NbtUtils.getDataVersion(file), targetVersion));
    }

    @Override
    public <T> void write(Dynamic<T> data) {
        writeFile(NbtUtils.addDataVersion(data, targetVersion));
    }
}

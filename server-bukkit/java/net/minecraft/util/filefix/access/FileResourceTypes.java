// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.filefix.access;

import com.mojang.datafixers.DSL;
import net.minecraft.util.datafix.DataFixTypes;
import net.minecraft.world.level.chunk.storage.RegionStorageInfo;

public class FileResourceTypes {
    public static final FileResourceType<LevelDat> LEVEL_DAT = new FileResourceType<>(LevelDat::new);
    public static final FileResourceType<PlayerData> PLAYER_DATA = new FileResourceType<>(PlayerData::new);

    public static FileResourceType<SavedDataNbt> savedData(DSL.TypeReference type) {
        return savedData(type, CompressedNbt.MissingSeverity.NEUTRAL);
    }

    public static FileResourceType<SavedDataNbt> savedData(DSL.TypeReference type,
            CompressedNbt.MissingSeverity missingSeverity) {
        return new FileResourceType<>((path, version) -> new SavedDataNbt(type, path, version, missingSeverity));
    }

    public static FileResourceType<ChunkNbt> chunk(DataFixTypes type, RegionStorageInfo info) {
        return new FileResourceType<>((path, version) -> new ChunkNbt(info, path, type, version));
    }
}

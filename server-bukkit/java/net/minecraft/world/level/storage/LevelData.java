// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import io.netty.buffer.ByteBuf;
import net.minecraft.CrashReportCategory;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelHeightAccessor;
public interface LevelData {
    default RespawnData getRespawnData() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.LevelData#getRespawnData");
    }
    default long getGameTime() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.LevelData#getGameTime");
    }
    default boolean isHardcore() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.LevelData#isHardcore");
    }
    default Difficulty getDifficulty() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.LevelData#getDifficulty");
    }
    default boolean isDifficultyLocked() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.LevelData#isDifficultyLocked");
    }
    default void fillCrashReportCategory(CrashReportCategory category, LevelHeightAccessor levelHeightAccessor) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.LevelData#fillCrashReportCategory");
    }
    public static record RespawnData(GlobalPos globalPos, float yaw, float pitch) {
        public static final RespawnData DEFAULT = null;
        public static final MapCodec<RespawnData> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.LevelData.RespawnData.class, "MAP_CODEC");
        public static final Codec<RespawnData> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.LevelData.RespawnData.class, "CODEC");
        public static final StreamCodec<ByteBuf, RespawnData> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.storage.LevelData.RespawnData.class, "STREAM_CODEC");
        public static RespawnData of(ResourceKey<Level> dimension, BlockPos pos, float yaw, float pitch) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.LevelData.RespawnData#of");
        }
        public ResourceKey<Level> dimension() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.LevelData.RespawnData#dimension");
        }
        public BlockPos pos() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.LevelData.RespawnData#pos");
        }
    }
}

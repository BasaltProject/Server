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
import net.minecraft.util.Mth;
import net.minecraft.world.Difficulty;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelHeightAccessor;
public interface LevelData {
    default RespawnData getRespawnData() {
        return RespawnData.DEFAULT;
    }
    default long getGameTime() {
        return 0L;
    }
    default boolean isHardcore() {
        return false;
    }
    default Difficulty getDifficulty() {
        return Difficulty.NORMAL;
    }
    default boolean isDifficultyLocked() {
        return false;
    }
    default void fillCrashReportCategory(CrashReportCategory category, LevelHeightAccessor levelHeightAccessor) {
        return;
    }
    public static record RespawnData(GlobalPos globalPos, float yaw, float pitch) {
        public static final RespawnData DEFAULT = new RespawnData(GlobalPos.of(Level.OVERWORLD, BlockPos.ZERO), 0.0F, 0.0F);
        public static final MapCodec<RespawnData> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.LevelData.RespawnData.class, "MAP_CODEC");
        public static final Codec<RespawnData> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.LevelData.RespawnData.class, "CODEC");
        public static final StreamCodec<ByteBuf, RespawnData> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.storage.LevelData.RespawnData.class, "STREAM_CODEC");
        public static RespawnData of(ResourceKey<Level> dimension, BlockPos pos, float yaw, float pitch) {
            return new RespawnData(GlobalPos.of(dimension, pos), Mth.wrapDegrees(yaw), Mth.clamp(pitch, -90.0F, 90.0F));
        }
        public ResourceKey<Level> dimension() {
            return globalPos.dimension();
        }
        public BlockPos pos() {
            return globalPos.pos();
        }
    }
}

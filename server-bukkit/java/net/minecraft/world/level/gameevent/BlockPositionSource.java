// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent;

import com.mojang.serialization.MapCodec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;

public record BlockPositionSource(BlockPos pos) implements PositionSource {
    public static final MapCodec<BlockPositionSource> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.gameevent.BlockPositionSource.class, "CODEC");
    public static final StreamCodec<ByteBuf, BlockPositionSource> STREAM_CODEC = StreamCodec.composite(BlockPos.STREAM_CODEC, BlockPositionSource::pos, BlockPositionSource::new);

    public Optional<Vec3> getPosition(Level level) {
        return Optional.of(Vec3.atCenterOf(pos));
    }

    public PositionSourceType<BlockPositionSource> getType() {
        return PositionSourceType.BLOCK;
    }

    public static class Type implements PositionSourceType<BlockPositionSource> {
        public MapCodec<BlockPositionSource> codec() {
            return CODEC;
        }

        public StreamCodec<ByteBuf, BlockPositionSource> streamCodec() {
            return STREAM_CODEC;
        }
    }
}

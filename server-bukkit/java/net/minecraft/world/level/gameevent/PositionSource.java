// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent;

import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.Level;
import net.minecraft.world.phys.Vec3;

public interface PositionSource {
    Codec<PositionSource> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.gameevent.PositionSource.class, "CODEC");
    StreamCodec<RegistryFriendlyByteBuf, PositionSource> STREAM_CODEC = StreamCodec.of(
        (buf, source) -> {
            if (source instanceof BlockPositionSource block) {
                buf.writeVarInt(0);
                BlockPositionSource.STREAM_CODEC.encode(buf, block);
            } else if (source instanceof EntityPositionSource entity) {
                buf.writeVarInt(1);
                EntityPositionSource.STREAM_CODEC.encode(buf, entity);
            } else {
                throw new IllegalArgumentException("Unknown position source " + source);
            }
        },
        buf -> switch (buf.readVarInt()) {
            case 0 -> BlockPositionSource.STREAM_CODEC.decode(buf);
            case 1 -> EntityPositionSource.STREAM_CODEC.decode(buf);
            default -> throw new IllegalArgumentException("Unknown position source type");
        });

    Optional<Vec3> getPosition(Level level);

    PositionSourceType<? extends PositionSource> getType();
}

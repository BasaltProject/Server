// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.Optional;
import net.minecraft.core.GlobalPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.level.ServerLevel;
public record LodestoneTracker(Optional<GlobalPos> target, boolean tracked) {
    public static final Codec<LodestoneTracker> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.LodestoneTracker.class, "CODEC");
    public static final StreamCodec<ByteBuf, LodestoneTracker> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.LodestoneTracker.class, "STREAM_CODEC");
    public LodestoneTracker tick(ServerLevel level) {
        if (!tracked || target.isEmpty() || target.get().dimension() != level.dimension()) {
            return this;
        }
        net.minecraft.core.BlockPos pos = target.get().pos();
        boolean present = level.isInWorldBounds(pos) && level.getBlockState(pos).is(net.minecraft.world.level.block.Blocks.LODESTONE);
        return present ? this : new LodestoneTracker(Optional.empty(), true);
    }
}

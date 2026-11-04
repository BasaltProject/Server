// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.Level;
public record GlobalPos(ResourceKey<Level> dimension, BlockPos pos) {
    public static final MapCodec<GlobalPos> MAP_CODEC =
        com.mojang.serialization.codecs.RecordCodecBuilder.mapCodec(instance -> instance.group(
            ResourceKey.codec(net.minecraft.core.registries.Registries.DIMENSION).fieldOf("dimension").forGetter(GlobalPos::dimension),
            BlockPos.CODEC.fieldOf("pos").forGetter(GlobalPos::pos)).apply(instance, GlobalPos::of));
    public static final Codec<GlobalPos> CODEC = MAP_CODEC.codec();
    public static final StreamCodec<ByteBuf, GlobalPos> STREAM_CODEC = StreamCodec.composite(
        ResourceKey.streamCodec(net.minecraft.core.registries.Registries.DIMENSION), GlobalPos::dimension,
        BlockPos.STREAM_CODEC, GlobalPos::pos, GlobalPos::of);
    public static GlobalPos of(ResourceKey<Level> dimension, BlockPos pos) {
        return new GlobalPos(dimension, pos);
    }
    public String toString() {
        return dimension.identifier() + " " + pos;
    }
    public boolean isCloseEnough(ResourceKey<Level> dimension, BlockPos pos, int maxDistance) {
        return this.dimension == dimension && this.pos.distSqr(pos) <= (double) (maxDistance * maxDistance);
    }
}

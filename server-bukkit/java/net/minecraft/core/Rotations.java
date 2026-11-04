// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public record Rotations(float x, float y, float z) {
    public static final Codec<Rotations> CODEC = Codec.FLOAT.listOf().comapFlatMap(
        list -> Util.fixedSize(list, 3).map(three -> new Rotations(three.get(0), three.get(1), three.get(2))),
        rotations -> List.of(rotations.x(), rotations.y(), rotations.z()));
    public static final StreamCodec<ByteBuf, Rotations> STREAM_CODEC = StreamCodec.composite(
        ByteBufCodecs.FLOAT, Rotations::x, ByteBufCodecs.FLOAT, Rotations::y, ByteBufCodecs.FLOAT, Rotations::z, Rotations::new);

    public Rotations(float x, float y, float z) {
        this.x = Float.isFinite(x) ? x % 360.0F : 0.0F;
        this.y = Float.isFinite(y) ? y % 360.0F : 0.0F;
        this.z = Float.isFinite(z) ? z % 360.0F : 0.0F;
    }

    private static final class Util {
        static com.mojang.serialization.DataResult<List<Float>> fixedSize(List<Float> list, int size) {
            return list.size() == size ? com.mojang.serialization.DataResult.success(list)
                : com.mojang.serialization.DataResult.error(() -> "Input is not a list of " + size + " elements");
        }
    }
}

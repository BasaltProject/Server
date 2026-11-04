// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.network.codec.StreamCodec;
public record CustomModelData(List<Float> floats, List<Boolean> flags, List<String> strings, List<Integer> colors) {
    public static final CustomModelData EMPTY = new CustomModelData(List.of(), List.of(), List.of(), List.of());
    public static final Codec<CustomModelData> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.component.CustomModelData.class, "CODEC");
    public static final StreamCodec<ByteBuf, CustomModelData> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.component.CustomModelData.class, "STREAM_CODEC");
    public Float getFloat(int index) {
        return index >= 0 && index < floats.size() ? floats.get(index) : null;
    }
    public Boolean getBoolean(int index) {
        return index >= 0 && index < flags.size() ? flags.get(index) : null;
    }
    public String getString(int index) {
        return index >= 0 && index < strings.size() ? strings.get(index) : null;
    }
    public Integer getColor(int index) {
        return index >= 0 && index < colors.size() ? colors.get(index) : null;
    }
}

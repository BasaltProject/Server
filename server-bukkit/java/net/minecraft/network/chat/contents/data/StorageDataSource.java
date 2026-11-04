// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.contents.data;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.stream.Stream;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
public record StorageDataSource(Identifier id) implements DataSource {
    public static final MapCodec<StorageDataSource> MAP_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        Identifier.CODEC.fieldOf("storage").forGetter(StorageDataSource::id)).apply(i, StorageDataSource::new));
    public Stream<CompoundTag> getData(CommandSourceStack sender) {
        return Stream.of(sender.getServer().getCommandStorage().get(id));
    }
    public MapCodec<StorageDataSource> codec() {
        return MAP_CODEC;
    }
    public String toString() {
        return "storage=" + id;
    }
}

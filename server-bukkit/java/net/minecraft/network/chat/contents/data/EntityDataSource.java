// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.contents.data;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.stream.Stream;
import net.minecraft.advancements.predicates.NbtPredicate;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.arguments.selector.EntitySelector;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.CompilableString;
public record EntityDataSource(CompilableString<EntitySelector> selector) implements DataSource {
    public static final MapCodec<EntityDataSource> MAP_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        EntitySelector.COMPILABLE_CODEC.fieldOf("entity").forGetter(EntityDataSource::selector)).apply(i, EntityDataSource::new));
    public Stream<CompoundTag> getData(CommandSourceStack sender) throws CommandSyntaxException {
        return selector.compiled().findEntities(sender).stream().map(NbtPredicate::getEntityTagToCompare);
    }
    public MapCodec<EntityDataSource> codec() {
        return MAP_CODEC;
    }
}

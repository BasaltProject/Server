// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.slot;
import java.util.ArrayList;
import java.util.List;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.Validatable;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.function.Function;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
public abstract class CompositeSlotSource implements SlotSource {
    protected final HolderSet<SlotSource> terms;
    protected CompositeSlotSource(HolderSet<SlotSource> terms) {
        this.terms = terms;
    }
    protected static <T extends CompositeSlotSource> MapCodec<T> createCodec(Function<HolderSet<SlotSource>, T> factory) {
        return RecordCodecBuilder.mapCodec(i -> i.group(SlotSources.LIST_CODEC.fieldOf("terms").forGetter(t -> t.terms)).apply(i, factory));
    }
    protected static <T extends CompositeSlotSource> Codec<T> createInlineCodec(Function<HolderSet<SlotSource>, T> factory) {
        return SlotSources.LIST_CODEC.xmap(factory, t -> t.terms);
    }
    public abstract MapCodec<? extends CompositeSlotSource> codec();
    public SlotCollection provide(LootContext context) {
        List<SlotCollection> parts = new ArrayList<>();
        for (Holder<SlotSource> term : terms) {
            parts.add(term.value().provide(context));
        }
        return SlotCollection.concat(parts);
    }
    public void validate(ValidationContext context) {
        SlotSource.super.validate(context);
        Validatable.validateHolderSet(context, "terms", terms);
    }
}

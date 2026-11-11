// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.slot;
import net.minecraft.world.level.storage.loot.Validatable;
import com.mojang.datafixers.Products;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
public abstract class TransformedSlotSource implements SlotSource {
    protected final Holder<SlotSource> slotSource;
    protected TransformedSlotSource(Holder<SlotSource> slotSource) {
        this.slotSource = slotSource;
    }
    public abstract MapCodec<? extends TransformedSlotSource> codec();
    protected static <T extends TransformedSlotSource> Products.P1<RecordCodecBuilder.Mu<T>, Holder<SlotSource>> commonFields(RecordCodecBuilder.Instance<T> i) {
        return i.group(SlotSources.CODEC.fieldOf("slot_source").forGetter(t -> t.slotSource));
    }
    protected abstract SlotCollection transform(SlotCollection slots);
    public final SlotCollection provide(LootContext context) {
        return transform(slotSource.value().provide(context));
    }
    public void validate(ValidationContext context) {
        SlotSource.super.validate(context);
        Validatable.validateHolder(context, "slot_source", slotSource);
    }
}

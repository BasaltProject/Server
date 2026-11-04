// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStackTemplate;
public record TransmuteResult(Optional<Holder<Item>> item, int count, DataComponentPatch components) {
    public static final MapCodec<TransmuteResult> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.crafting.TransmuteResult.class, "MAP_CODEC");
    public static final Codec<TransmuteResult> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.crafting.TransmuteResult.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, TransmuteResult> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.crafting.TransmuteResult.class, "STREAM_CODEC");
    public static final TransmuteResult KEEP_INPUT_ITEM = new TransmuteResult(Optional.empty(), 1, DataComponentPatch.EMPTY);
    public TransmuteResult(Holder<Item> item) {
        this(Optional.of(item), 1, DataComponentPatch.EMPTY);
    }
    public TransmuteResult(Item item) {
        this(item.builtInRegistryHolder());
    }
    public static TransmuteResult fromTemplate(ItemStackTemplate template) {
        return new TransmuteResult(Optional.of(template.item()), template.count(), template.components());
    }
    public ItemStackTemplate resolve(Holder<Item> inputItem) {
        return resolve(inputItem, count);
    }
    public ItemStackTemplate resolve(Holder<Item> inputItem, int count) {
        return new ItemStackTemplate(item.orElse(inputItem), count, components);
    }
}

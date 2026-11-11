// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.world.item.component.WrittenBookContent;
import net.minecraft.core.component.DataComponents;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.server.network.Filterable;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetBookCoverFunction extends LootItemConditionalFunction {
    private final Optional<String> author;
    private final Optional<Filterable<String>> title;
    private final Optional<Integer> generation;
    private WrittenBookContent merge(WrittenBookContent original) {
        return new WrittenBookContent(title.orElseGet(original::title), author.orElseGet(original::author),
            generation.orElseGet(original::generation), original.pages(), original.resolved());
    }
    public static final MapCodec<SetBookCoverFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetBookCoverFunction.class, "MAP_CODEC");
    public SetBookCoverFunction(Optional<Holder<LootItemCondition>> condition, Optional<Filterable<String>> title, Optional<String> author, Optional<Integer> generation) {
        super(condition);
        this.author = author;
        this.title = title;
        this.generation = generation;
    }
    protected ItemStack run(ItemStack itemStack, LootContext context) {
        itemStack.update(DataComponents.WRITTEN_BOOK_CONTENT, WrittenBookContent.EMPTY, this::merge);
        return itemStack;
    }
    public MapCodec<SetBookCoverFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}

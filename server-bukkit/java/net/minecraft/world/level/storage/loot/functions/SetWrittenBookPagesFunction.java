// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.network.chat.Component;
import net.minecraft.server.network.Filterable;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.WrittenBookContent;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetWrittenBookPagesFunction extends LootItemConditionalFunction {
    private final List<Filterable<Component>> pages;
    private final ListOperation pageOperation;
    public static final MapCodec<SetWrittenBookPagesFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetWrittenBookPagesFunction.class, "MAP_CODEC");
    protected SetWrittenBookPagesFunction(Optional<Holder<LootItemCondition>> condition, List<Filterable<Component>> pages, ListOperation pageOperation) {
        super(condition);
        this.pages = pages;
        this.pageOperation = pageOperation;
    }
    protected ItemStack run(ItemStack itemStack, LootContext context) {
        itemStack.update(DataComponents.WRITTEN_BOOK_CONTENT, WrittenBookContent.EMPTY, this::apply);
        return itemStack;


    }
    public WrittenBookContent apply(WrittenBookContent original) {
        return original.withReplacedPages(pageOperation.apply(original.pages(), pages));
    }
    public MapCodec<SetWrittenBookPagesFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}

// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.server.network.Filterable;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.WritableBookContent;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetWritableBookPagesFunction extends LootItemConditionalFunction {
    private final List<Filterable<String>> pages;
    private final ListOperation pageOperation;
    public static final MapCodec<SetWritableBookPagesFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetWritableBookPagesFunction.class, "MAP_CODEC");
    protected SetWritableBookPagesFunction(Optional<Holder<LootItemCondition>> condition, List<Filterable<String>> pages, ListOperation pageOperation) {
        super(condition);
        this.pages = pages;
        this.pageOperation = pageOperation;
    }
    protected ItemStack run(ItemStack itemStack, LootContext context) {
        itemStack.update(DataComponents.WRITABLE_BOOK_CONTENT, WritableBookContent.EMPTY, this::apply);
        return itemStack;
    }
    public WritableBookContent apply(WritableBookContent original) {
        return original.withReplacedPages(pageOperation.apply(original.pages(), pages, 100));
    }
    public MapCodec<SetWritableBookPagesFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}

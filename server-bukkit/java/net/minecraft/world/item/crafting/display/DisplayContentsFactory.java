// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting.display;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
public interface DisplayContentsFactory<T> {
    public interface ForStacks<T> extends DisplayContentsFactory<T> {
        default T forStack(Holder<Item> item) {
            return forStack(new ItemStack(item));
        }
        default T forStack(Item item) {
            return forStack(new ItemStack(item));
        }
        T forStack(ItemStack stack);
    }
    public interface ForRemainders<T> extends DisplayContentsFactory<T> {
        T addRemainder(T entry, List<T> remainders);
    }
}

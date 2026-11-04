// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;
import java.util.stream.Stream;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
public class ItemUtils {
    public static InteractionResult startUsingInstantly(Level level, Player player, InteractionHand hand) {
        player.startUsingItem(hand);
        return InteractionResult.CONSUME;
    }
    public static ItemStack createFilledResult(ItemStack itemStack, Player player, ItemStack newItemStack, boolean limitCreativeStackSize) {
        if (limitCreativeStackSize && player.hasInfiniteMaterials()) {
            player.getInventory().add(newItemStack);
            return itemStack;
        }
        itemStack.consume(1, player);
        if (itemStack.isEmpty()) {
            return newItemStack;
        }
        if (!player.getInventory().add(newItemStack)) {
            player.drop(newItemStack, false, null);
        }
        return itemStack;
    }
    public static ItemStack createFilledResult(ItemStack itemStack, Player player, ItemStack newItemStack) {
        return createFilledResult(itemStack, player, newItemStack, true);
    }
    public static void onContainerDestroyed(ItemEntity container, Stream<ItemStack> contents) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.ItemUtils#onContainerDestroyed");
    }
}

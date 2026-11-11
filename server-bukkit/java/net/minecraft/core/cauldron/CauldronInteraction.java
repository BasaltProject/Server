// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.cauldron;
import java.util.HashMap;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.tags.TagKey;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
@FunctionalInterface
public interface CauldronInteraction {
    CauldronInteraction DEFAULT = (state, level, pos, player, hand, held) -> InteractionResult.TRY_WITH_EMPTY_HAND;
    InteractionResult interact(BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, ItemStack itemInHand);
    public static class Dispatcher {
        private final Map<TagKey<Item>, CauldronInteraction> byTag = new HashMap<>();
        private final Map<Item, CauldronInteraction> byItem = new HashMap<>();
        void put(Item item, CauldronInteraction interaction) {
            byItem.put(item, interaction);
        }
        void put(TagKey<Item> tag, CauldronInteraction interaction) {
            byTag.put(tag, interaction);
        }
        public CauldronInteraction get(ItemStack itemStack) {
            for (Map.Entry<TagKey<Item>, CauldronInteraction> e : byTag.entrySet()) {
                if (itemStack.is(e.getKey())) {
                    return e.getValue();
                }
            }
            return byItem.getOrDefault(itemStack.getItem(), CauldronInteraction.DEFAULT);
        }
    }
}

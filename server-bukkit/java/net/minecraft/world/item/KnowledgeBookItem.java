// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.*;
import java.util.function.*;
import net.minecraft.core.*;
import net.minecraft.core.component.*;
import net.minecraft.network.chat.*;
import net.minecraft.server.level.*;
import net.minecraft.sounds.*;
import net.minecraft.world.*;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.player.*;
import net.minecraft.world.item.context.*;
import net.minecraft.world.level.*;
import net.minecraft.world.level.block.*;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.*;
import net.minecraft.world.item.component.*;
import net.minecraft.world.entity.projectile.*;
import net.minecraft.world.entity.projectile.throwableitemprojectile.*;
import net.minecraft.world.entity.projectile.hurtingprojectile.*;
import net.minecraft.world.entity.projectile.arrow.*;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.advancements.triggers.CriteriaTriggers;
import net.minecraft.stats.Stats;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.level.block.entity.SignBlockEntity;
import net.minecraft.world.level.block.entity.SignTextSlot;
import net.minecraft.tags.*;

import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.crafting.*;
public class KnowledgeBookItem extends Item {
    public KnowledgeBookItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        ItemStack held = player.getItemInHand(hand);
        List<ResourceKey<Recipe<?>>> ids = held.getOrDefault(DataComponents.RECIPES, List.of());
        held.consume(1, player);
        if (ids.isEmpty()) {
            return InteractionResult.FAIL;
        }
        if (!level.isClientSide()) {
            RecipeManager manager = level.getServer().getRecipeManager();
            List<RecipeHolder<?>> recipes = new ArrayList<>(ids.size());
            for (ResourceKey<Recipe<?>> id : ids) {
                Optional<RecipeHolder<?>> recipe = manager.byKey(id);
                if (recipe.isEmpty()) {
                    return InteractionResult.FAIL;
                }
                recipes.add(recipe.get());
            }
            player.awardRecipes(recipes);
            player.awardStat(Stats.ITEM_USED.get(this));
        }
        return InteractionResult.SUCCESS;
    }
}

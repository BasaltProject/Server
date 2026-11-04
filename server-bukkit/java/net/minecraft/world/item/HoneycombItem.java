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

import com.google.common.base.Suppliers;
import com.google.common.collect.BiMap;
import com.google.common.collect.HashBiMap;
import com.google.common.collect.ImmutableBiMap;
import com.google.common.collect.ImmutableMap;
import com.mojang.datafixers.util.Pair;
import net.minecraft.data.recipes.RecipeCategory;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.block.entity.SignText;
import net.minecraft.world.level.block.state.properties.ChestType;
public class HoneycombItem extends Item implements SignApplicator {
    private static final String[] kBases = {"copper_block", "cut_copper", "cut_copper_slab", "cut_copper_stairs", "chiseled_copper",
        "copper_door", "copper_trapdoor", "copper_bars", "copper_grate", "copper_bulb", "copper_chest", "copper_golem_statue",
        "lightning_rod", "copper_lantern", "copper_chain"};
    private static final String[] kStates = {"", "exposed_", "weathered_", "oxidized_"};

    public static final Supplier<BiMap<Block, Block>> WAXABLES = Suppliers.memoize(() -> {
        BiMap<Block, Block> pairs = HashBiMap.create();
        for (String base : kBases) {
            for (String state : kStates) {
                String plain = plainName(base, state);
                Block unwaxed = block(plain);
                Block waxed = block("waxed_" + plain);
                if (unwaxed != Blocks.AIR && waxed != Blocks.AIR) {
                    pairs.put(unwaxed, waxed);
                }
            }
        }
        return ImmutableBiMap.copyOf(pairs);
    });
    public static final Supplier<BiMap<Block, Block>> WAX_OFF_BY_BLOCK = Suppliers.memoize(() -> WAXABLES.get().inverse());
    public static final ImmutableMap<Block, Pair<RecipeCategory, String>> WAXED_RECIPES = Suppliers.memoize(HoneycombItem::waxedRecipes).get();

    public HoneycombItem(Item.Properties properties) {
        super(properties);
    }

    private static String plainName(String base, String state) {
        if (base.equals("copper_block")) {
            return state.isEmpty() ? "copper_block" : state + "copper";
        }
        return state + base;
    }

    private static Block block(String path) {
        return net.minecraft.core.registries.BuiltInRegistries.BLOCK.getValue(Identifier.withDefaultNamespace(path));
    }

    private static ImmutableMap<Block, Pair<RecipeCategory, String>> waxedRecipes() {
        Map<Block, Pair<RecipeCategory, String>> recipes = new java.util.LinkedHashMap<>();
        for (String base : new String[] {"copper_bulb", "copper_door", "copper_trapdoor", "copper_golem_statue", "copper_chest",
            "lightning_rod", "copper_bars", "copper_chain", "copper_lantern", "copper_block"}) {
            RecipeCategory category = base.equals("copper_bulb") || base.equals("copper_door") || base.equals("copper_trapdoor")
                ? RecipeCategory.REDSTONE : RecipeCategory.BUILDING_BLOCKS;
            for (String state : kStates) {
                String waxed = "waxed_" + plainName(base, state);
                String name = base.equals("copper_bulb") ? waxed : "waxed_" + (base.equals("copper_bars") ? "copper_bar" : base);
                Block block = block(waxed);
                if (block != Blocks.AIR) {
                    recipes.put(block, Pair.of(category, name));
                }
            }
        }
        return ImmutableMap.copyOf(recipes);
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        Level level = context.getLevel();
        BlockPos pos = context.getClickedPos();
        BlockState old = level.getBlockState(pos);
        Optional<BlockState> waxed = getWaxed(old);
        if (waxed.isEmpty()) {
            return InteractionResult.PASS;
        }
        Player player = context.getPlayer();
        ItemStack stack = context.getItemInHand();
        if (player instanceof ServerPlayer serverPlayer && CriteriaTriggers.ITEM_USED_ON_BLOCK != null) {
            CriteriaTriggers.ITEM_USED_ON_BLOCK.trigger(serverPlayer, pos, stack);
        }
        stack.shrink(1);
        level.setBlock(pos, waxed.get(), 11);
        level.gameEvent(GameEvent.BLOCK_CHANGE, pos, GameEvent.Context.of(player, waxed.get()));
        level.levelEvent(player, 3003, pos, 0);
        level.playSound(player, pos, SoundEvents.HONEYCOMB_WAX_ON, SoundSource.BLOCKS, 1.0F, 1.0F);
        if (old.getBlock() instanceof ChestBlock && old.getValue(ChestBlock.TYPE) != ChestType.SINGLE) {
            BlockPos other = ChestBlock.getConnectedBlockPos(pos, old);
            level.gameEvent(GameEvent.BLOCK_CHANGE, other, GameEvent.Context.of(player, level.getBlockState(other)));
            level.levelEvent(player, 3003, other, 0);
            level.playSound(player, pos, SoundEvents.HONEYCOMB_WAX_ON, SoundSource.BLOCKS, 1.0F, 1.0F);
        }
        return InteractionResult.SUCCESS;
    }

    public static Optional<BlockState> getWaxed(BlockState oldState) {
        return Optional.ofNullable(WAXABLES.get().get(oldState.getBlock())).map(waxed -> waxed.withPropertiesOf(oldState));
    }

    @Override
    public boolean tryApplyToSign(Level level, SignBlockEntity sign, SignTextSlot slot, ItemStack item, Player player) {
        if (!sign.setWaxed(true)) {
            return false;
        }
        BlockPos pos = sign.getBlockPos();
        level.levelEvent(null, 3003, pos, 0);
        level.playSound(null, pos, SoundEvents.HONEYCOMB_WAX_ON, SoundSource.BLOCKS, 1.0F, 1.0F);
        return true;
    }

    @Override
    public boolean canApplyToSign(SignText text, ItemStack item, Player player) {
        return true;
    }

    static record WaxedRecipeGroup(WeatheringCopperCollection<Block> block, Function<Block, Pair<RecipeCategory, String>> recipeIdProvider) {
    }
}

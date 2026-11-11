// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.cauldron;
import com.mojang.serialization.Codec;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.core.component.DataComponents;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.stats.Stats;
import net.minecraft.tags.FluidTags;
import net.minecraft.tags.ItemTags;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.ItemUtils;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.alchemy.PotionContents;
import net.minecraft.world.item.alchemy.Potions;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.LayeredCauldronBlock;
import net.minecraft.world.level.block.ShulkerBoxBlock;
import net.minecraft.world.level.block.entity.BannerPatternLayers;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
public class CauldronInteractions {
    private static final ExtraCodecs.LateBoundIdMapper<String, CauldronInteraction.Dispatcher> kNames = new ExtraCodecs.LateBoundIdMapper<>();
    public static final Codec<CauldronInteraction.Dispatcher> CODEC = kNames.codec(Codec.STRING);
    public static final CauldronInteraction.Dispatcher EMPTY = named("empty");
    public static final CauldronInteraction.Dispatcher WATER = named("water");
    public static final CauldronInteraction.Dispatcher LAVA = named("lava");
    public static final CauldronInteraction.Dispatcher POWDER_SNOW = named("powder_snow");
    private static final InteractionResult kSkip = InteractionResult.TRY_WITH_EMPTY_HAND;
    private static CauldronInteraction.Dispatcher named(String name) {
        CauldronInteraction.Dispatcher d = new CauldronInteraction.Dispatcher();
        kNames.put(name, d);
        return d;
    }
    private static void lowerFillLevel(BlockState state, Level level, BlockPos pos) {
        int remaining = state.getValue(LayeredCauldronBlock.LEVEL) - 1;
        BlockState next = remaining == 0 ? Blocks.CAULDRON.defaultBlockState() : state.setValue(LayeredCauldronBlock.LEVEL, remaining);
        level.setBlockAndUpdate(pos, next);
        level.gameEvent(GameEvent.BLOCK_CHANGE, pos, GameEvent.Context.of(next));
    }
    private static void stat(Player player, net.minecraft.resources.Identifier stat) {
        player.awardStat(stat);
    }
    private static void awardUse(Player player, Item used) {
        stat(player, Stats.USE_CAULDRON);
        player.awardStat(Stats.ITEM_USED.get(used));
    }
    private static boolean isWaterBottle(ItemStack stack) {
        PotionContents potion = stack.get(DataComponents.POTION_CONTENTS);
        return potion != null && potion.is(Potions.WATER);
    }
    public static void bootStrap() {
        addDefaultInteractions(EMPTY);
        EMPTY.put(Items.POTION, (state, level, pos, player, hand, held) -> {
            if (!isWaterBottle(held)) {
                return kSkip;
            }
            if (!level.isClientSide()) {
                player.setItemInHand(hand, ItemUtils.createFilledResult(held, player, new ItemStack(Items.GLASS_BOTTLE)));
                awardUse(player, held.getItem());
                level.setBlockAndUpdate(pos,  Blocks.WATER_CAULDRON.defaultBlockState());
                level.playSound(null, pos, SoundEvents.BOTTLE_EMPTY, SoundSource.BLOCKS, 1.0F, 1.0F);
                level.gameEvent(null, GameEvent.FLUID_PLACE, pos);
            }
            return InteractionResult.SUCCESS;
        });
        addDefaultInteractions(WATER);
        WATER.put(Items.BUCKET, (state, level, pos, player, hand, held) ->
                fillBucket(state, level, pos, player, hand, held, new ItemStack(Items.WATER_BUCKET), s -> s.getValue(LayeredCauldronBlock.LEVEL) == 3, SoundEvents.BUCKET_FILL));
        WATER.put(Items.GLASS_BOTTLE, (state, level, pos, player, hand, held) -> {
            if (!level.isClientSide()) {
                player.setItemInHand(hand, ItemUtils.createFilledResult(held, player, PotionContents.createItemStack(Items.POTION, Potions.WATER)));
                awardUse(player, held.getItem());
                lowerFillLevel(state, level, pos);
                level.playSound(null, pos, SoundEvents.BOTTLE_FILL, SoundSource.BLOCKS, 1.0F, 1.0F);
                level.gameEvent(null, GameEvent.FLUID_PICKUP, pos);
            }
            return InteractionResult.SUCCESS;
        });
        WATER.put(Items.POTION, (state, level, pos, player, hand, held) -> {
            if (state.getValue(LayeredCauldronBlock.LEVEL) == 3 || !isWaterBottle(held)) {
                return kSkip;
            }
            if (!level.isClientSide()) {
                player.setItemInHand(hand, ItemUtils.createFilledResult(held, player, new ItemStack(Items.GLASS_BOTTLE)));
                awardUse(player, held.getItem());
                level.setBlockAndUpdate(pos, state.cycle(LayeredCauldronBlock.LEVEL));
                level.playSound(null, pos, SoundEvents.BOTTLE_EMPTY, SoundSource.BLOCKS, 1.0F, 1.0F);
                level.gameEvent(null, GameEvent.FLUID_PLACE, pos);
            }
            return InteractionResult.SUCCESS;
        });
        WATER.put(ItemTags.CAULDRON_CAN_REMOVE_DYE, CauldronInteractions::washDye);
        Items.BANNER.forEach(banner -> WATER.put(banner, CauldronInteractions::washBanner));
        Items.DYED_SHULKER_BOX.forEach(box -> WATER.put(box, CauldronInteractions::washShulkerBox));
        LAVA.put(Items.BUCKET, (state, level, pos, player, hand, held) ->
                fillBucket(state, level, pos, player, hand, held, new ItemStack(Items.LAVA_BUCKET), s -> true, SoundEvents.BUCKET_FILL_LAVA));
        addDefaultInteractions(LAVA);
        POWDER_SNOW.put(Items.BUCKET, (state, level, pos, player, hand, held) ->
                fillBucket(state, level, pos, player, hand, held, new ItemStack(Items.POWDER_SNOW_BUCKET), s -> s.getValue(LayeredCauldronBlock.LEVEL) == 3, SoundEvents.BUCKET_FILL_POWDER_SNOW));
        addDefaultInteractions(POWDER_SNOW);
    }
    public static void addDefaultInteractions(CauldronInteraction.Dispatcher interactionMap) {
        interactionMap.put(Items.LAVA_BUCKET, CauldronInteractions::pourLava);
        interactionMap.put(Items.WATER_BUCKET, CauldronInteractions::pourWater);
        interactionMap.put(Items.POWDER_SNOW_BUCKET, CauldronInteractions::pourPowderSnow);
    }
    public static InteractionResult fillBucket(BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, ItemStack itemInHand,
            ItemStack newItem, Predicate<BlockState> canFill, SoundEvent soundEvent) {
        if (!canFill.test(state)) {
            return kSkip;
        }
        if (!level.isClientSide()) {
            player.setItemInHand(hand, ItemUtils.createFilledResult(itemInHand, player, newItem));
            awardUse(player, itemInHand.getItem());
            level.setBlockAndUpdate(pos, Blocks.CAULDRON.defaultBlockState());
            level.playSound(null, pos, soundEvent, SoundSource.BLOCKS, 1.0F, 1.0F);
            level.gameEvent(null, GameEvent.FLUID_PICKUP, pos);
        }
        return InteractionResult.SUCCESS;
    }
    public static InteractionResult emptyBucket(Level level, BlockPos pos, Player player, InteractionHand hand, ItemStack itemInHand, BlockState newState, SoundEvent soundEvent) {
        if (!level.isClientSide()) {
            player.setItemInHand(hand, ItemUtils.createFilledResult(itemInHand, player, new ItemStack(Items.BUCKET)));
            stat(player, Stats.FILL_CAULDRON);
            player.awardStat(Stats.ITEM_USED.get(itemInHand.getItem()));
            level.setBlockAndUpdate(pos, newState);
            level.playSound(null, pos, soundEvent, SoundSource.BLOCKS, 1.0F, 1.0F);
            level.gameEvent(null, GameEvent.FLUID_PLACE, pos);
        }
        return InteractionResult.SUCCESS;
    }
    private static InteractionResult pourWater(BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, ItemStack held) {
        return emptyBucket(level, pos, player, hand, held, Blocks.WATER_CAULDRON.defaultBlockState().setValue(LayeredCauldronBlock.LEVEL, 3), SoundEvents.BUCKET_EMPTY);
    }
    private static InteractionResult pourLava(BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, ItemStack held) {
        return underWater(level, pos) ? InteractionResult.CONSUME : emptyBucket(level, pos, player, hand, held, Blocks.LAVA_CAULDRON.defaultBlockState(), SoundEvents.BUCKET_EMPTY_LAVA);
    }
    private static InteractionResult pourPowderSnow(BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, ItemStack held) {
        return underWater(level, pos) ? InteractionResult.CONSUME
                : emptyBucket(level, pos, player, hand, held, Blocks.POWDER_SNOW_CAULDRON.defaultBlockState().setValue(LayeredCauldronBlock.LEVEL, 3), SoundEvents.BUCKET_EMPTY_POWDER_SNOW);
    }
    private static InteractionResult washShulkerBox(BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, ItemStack held) {
        if (!(Block.byItem(held.getItem()) instanceof ShulkerBoxBlock)) {
            return kSkip;
        }
        if (!level.isClientSide()) {
            player.setItemInHand(hand, ItemUtils.createFilledResult(held, player, held.transmuteCopy(Blocks.SHULKER_BOX, 1), false));
            stat(player, Stats.CLEAN_SHULKER_BOX);
            lowerFillLevel(state, level, pos);
        }
        return InteractionResult.SUCCESS;
    }
    private static InteractionResult washBanner(BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, ItemStack held) {
        BannerPatternLayers patterns = held.getOrDefault(DataComponents.BANNER_PATTERNS, BannerPatternLayers.EMPTY);
        if (patterns.layers().isEmpty()) {
            return kSkip;
        }
        if (!level.isClientSide()) {
            ItemStack cleaned = held.copyWithCount(1);
            cleaned.set(DataComponents.BANNER_PATTERNS, patterns.removeLast());
            player.setItemInHand(hand, ItemUtils.createFilledResult(held, player, cleaned, false));
            stat(player, Stats.CLEAN_BANNER);
            lowerFillLevel(state, level, pos);
        }
        return InteractionResult.SUCCESS;
    }
    private static InteractionResult washDye(BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, ItemStack held) {
        if (!held.has(DataComponents.DYED_COLOR)) {
            return kSkip;
        }
        if (!level.isClientSide()) {
            held.remove(DataComponents.DYED_COLOR);
            stat(player, Stats.CLEAN_ARMOR);
            lowerFillLevel(state, level, pos);
        }
        return InteractionResult.SUCCESS;
    }
    private static boolean underWater(Level level, BlockPos pos) {
        return level.getFluidState(pos.above()).is(FluidTags.WATER);
    }
}

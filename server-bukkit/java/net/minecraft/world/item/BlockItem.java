// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.Map;
import net.minecraft.advancements.triggers.CriteriaTriggers;
import net.minecraft.core.BlockPos;
import net.minecraft.core.component.DataComponents;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.component.BlockItemStateProperties;
import net.minecraft.world.item.component.ItemContainerContents;
import net.minecraft.world.item.component.TypedEntityData;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.item.context.UseOnContext;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.ShulkerBoxBlock;
import net.minecraft.world.level.block.SoundType;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.storage.TagValueOutput;
import net.minecraft.world.phys.shapes.CollisionContext;

public class BlockItem extends Item {
    private final Block block;

    public BlockItem(Block block, Item.Properties properties) {
        super(properties);
        this.block = block;
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        InteractionResult placed = place(new BlockPlaceContext(context));
        if (placed.consumesAction()) {
            return placed;
        }
        InteractionResult direct = super.useOn(context);
        if (direct.consumesAction()) {
            return direct;
        }
        return context.getItemInHand().has(DataComponents.CONSUMABLE)
            ? super.use(context.getLevel(), context.getPlayer(), context.getHand()) : placed;
    }

    public InteractionResult place(BlockPlaceContext placeContext) {
        Level level = placeContext.getLevel();
        if (!getBlock().isEnabled(level.enabledFeatures()) || !placeContext.canPlace()) {
            return InteractionResult.FAIL;
        }
        BlockPlaceContext context = updatePlacementContext(placeContext);
        if (context == null) {
            return InteractionResult.FAIL;
        }
        BlockState wanted = getPlacementState(context);
        if (wanted == null || !placeBlock(context, wanted)) {
            return InteractionResult.FAIL;
        }
        BlockPos pos = context.getClickedPos();
        Player player = context.getPlayer();
        ItemStack stack = context.getItemInHand();
        BlockState placed = level.getBlockState(pos);
        if (placed.is(wanted.getBlock())) {
            placed = applyStateComponent(pos, level, stack, placed);
            updateCustomBlockEntityTag(level, player, pos, stack);
            BlockEntity entity = level.getBlockEntity(pos);
            if (entity != null) {
                entity.applyComponentsFromItemStack(stack);
                entity.setChanged();
            }
            placed.getBlock().setPlacedBy(level, pos, placed, player, stack);
            if (player instanceof ServerPlayer serverPlayer && CriteriaTriggers.PLACED_BLOCK != null) {
                CriteriaTriggers.PLACED_BLOCK.trigger(serverPlayer, pos, stack);
            }
        }
        SoundType sound = placed.getSoundType();
        level.playSound(player, pos, getPlaceSound(placed), SoundSource.BLOCKS, (sound.getVolume() + 1.0F) / 2.0F,
            sound.getPitch() * 0.8F);
        level.gameEvent(GameEvent.BLOCK_PLACE, pos, GameEvent.Context.of(player, placed));
        stack.consume(1, player);
        return InteractionResult.SUCCESS;
    }

    protected SoundEvent getPlaceSound(BlockState blockState) {
        return blockState.getSoundType().getPlaceSound();
    }

    public BlockPlaceContext updatePlacementContext(BlockPlaceContext context) {
        return context;
    }

    protected BlockState getPlacementState(BlockPlaceContext context) {
        BlockState state = getBlock().getStateForPlacement(context);
        return state != null && canPlace(context, state) ? state : null;
    }

    private static BlockState applyStateComponent(BlockPos pos, Level level, ItemStack stack, BlockState placed) {
        BlockItemStateProperties properties = stack.getOrDefault(DataComponents.BLOCK_STATE, BlockItemStateProperties.EMPTY);
        if (properties.isEmpty()) {
            return placed;
        }
        BlockState modified = properties.apply(placed);
        if (modified != placed) {
            level.setBlock(pos, modified, 2);
        }
        return modified;
    }

    protected boolean canPlace(BlockPlaceContext context, BlockState stateForPlacement) {
        Player player = context.getPlayer();
        return (!mustSurvive() || stateForPlacement.canSurvive(context.getLevel(), context.getClickedPos()))
            && context.getLevel().isUnobstructed(stateForPlacement, context.getClickedPos(), CollisionContext.placementContext(player));
    }

    protected boolean mustSurvive() {
        return true;
    }

    protected boolean placeBlock(BlockPlaceContext context, BlockState placementState) {
        return context.getLevel().setBlock(context.getClickedPos(), placementState, 11);
    }

    public static boolean updateCustomBlockEntityTag(Level level, Player player, BlockPos pos, ItemStack itemStack) {
        if (level.isClientSide()) {
            return false;
        }
        TypedEntityData<BlockEntityType<?>> data = itemStack.get(DataComponents.BLOCK_ENTITY_DATA);
        BlockEntity entity = data == null ? null : level.getBlockEntity(pos);
        if (entity == null || entity.getType() != data.type()) {
            return false;
        }
        boolean allowed = !entity.getType().onlyOpCanSetNbt() || player != null && player.canUseGameMasterBlocks();
        return allowed && data.loadInto(entity, level.registryAccess());
    }

    @Override
    public boolean shouldPrintOpWarning(ItemStack stack, Player player) {
        if (player == null || !player.permissions().hasPermission(net.minecraft.server.permissions.Permissions.COMMANDS_GAMEMASTER)) {
            return false;
        }
        TypedEntityData<BlockEntityType<?>> data = stack.get(DataComponents.BLOCK_ENTITY_DATA);
        return data != null && data.type().onlyOpCanSetNbt();
    }

    public Block getBlock() {
        return block;
    }

    public void registerBlocks(Map<Block, Item> map, Item item) {
        map.put(getBlock(), item);
    }

    @Override
    public boolean canFitInsideContainerItems() {
        return !(getBlock() instanceof ShulkerBoxBlock);
    }

    @Override
    public void onDestroyed(ItemEntity entity) {
        ItemContainerContents contents = entity.getItem().set(DataComponents.CONTAINER, ItemContainerContents.EMPTY);
        if (contents != null) {
            ItemUtils.onContainerDestroyed(entity, contents.nonEmptyItemCopyStream());
        }
    }

    public static void setBlockEntityData(ItemStack stack, BlockEntityType<?> type, TagValueOutput output) {
        output.discard("id");
        if (output.isEmpty()) {
            stack.remove(DataComponents.BLOCK_ENTITY_DATA);
            return;
        }
        BlockEntity.addEntityType(output, type);
        stack.set(DataComponents.BLOCK_ENTITY_DATA, TypedEntityData.of(type, output.buildResult()));
    }
}
